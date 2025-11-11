// httpc://github.com/realsigmamusic/realsigmadrums

#include <clap/clap.h>
#include <clap/ext/audio-ports.h>
#include <clap/ext/note-ports.h>
#include <sndfile.h>
#include <fstream> 
#include <vector>
#include <map>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <algorithm>
#include <cstdint>
#include <stdlib.h>

#define MYDRUMKIT_ID "realsigmadrums"
#define MYDRUMKIT_NAME "Real Sigma Drums"
#define NUM_OUTPUTS 15
#define MAX_VOICES 64

// Leitor de .pak
struct PakReader {
	std::ifstream file;
	std::map<std::string, std::pair<uint64_t, uint64_t>> index; // path -> (offset, size)
	
	bool open(const std::string& pakPath) {
		file.open(pakPath, std::ios::binary);
		if (!file) return false;
		
		uint32_t count;
		file.read((char*)&count, sizeof(count));
		
		for (uint32_t i = 0; i < count; ++i) {
			uint32_t len;
			file.read((char*)&len, sizeof(len));
			
			std::string path(len, '\0');
			file.read(&path[0], len);
			
			uint64_t offset, size;
			file.read((char*)&offset, sizeof(offset));
			file.read((char*)&size, sizeof(size));
			
			index[path] = {offset, size};
		}
		
		fprintf(stderr, "[Real Sigma Drums] PakReader Carregado: %u arquivos\n", count);
		return true;
	}
	
	std::vector<char> read(const std::string& path) {
		auto it = index.find(path);
		if (it == index.end()) return {};
		
		file.seekg(it->second.first);
		std::vector<char> data(it->second.second);
		file.read(data.data(), data.size());
		return data;
	}
};

struct Sample {
	std::vector<float> dataL;
	std::vector<float> dataR;
	int channels;
	int sampleRate;
	bool is_stereo;
	Sample() : channels(0), sampleRate(0), is_stereo(false) {}
};

struct VelocityLayer {
	uint8_t min_vel;
	uint8_t max_vel;
	std::vector<Sample> samples;
	uint32_t current_rr = 0;
	
	const Sample* getNextSample() {
		if (samples.empty()) return nullptr;
		const Sample* s = &samples[current_rr];
		current_rr = (current_rr + 1) % samples.size();
		return s;
	}
};

struct RRGroup {
	std::vector<VelocityLayer> velocity_layers;
	int output = 0;
	int chokeGroup = 0;
	
	const Sample* getSampleForVelocity(uint8_t velocity) {
		// Procura o layer apropriado para esta velocidade
		for (auto& layer : velocity_layers) {
			if (velocity >= layer.min_vel && velocity <= layer.max_vel) {
				return layer.getNextSample();
			}
		}
		// Fallback: retorna do primeiro layer se não encontrar
		if (!velocity_layers.empty()) {
			return velocity_layers[0].getNextSample();
		}
		return nullptr;
	}
};

struct Voice {
	const Sample* sample = nullptr;
	uint32_t pos = 0;
	uint32_t length = 0;
	int output = 0;
	float velocity = 1.0f;
	int chokeGroup = 0;
};

struct MyDrumKit {
	const clap_host_t* host = nullptr;
	std::map<int, std::vector<RRGroup>> rr_groups;
	std::vector<Voice> voices;
	float* outputs[NUM_OUTPUTS];
	double sample_rate = 44100.0;
	PakReader pak;

	MyDrumKit() {
		for (int i = 0; i < NUM_OUTPUTS; ++i) outputs[i] = nullptr;
	}

	// Função load_wav agora é membro estático da classe
	static Sample load_wav(const char* data, size_t size, bool force_stereo = false) {
		SF_INFO info{};
		SF_VIRTUAL_IO vio;
		
		struct MemFile {
			const char* data;
			size_t size;
			size_t pos;
		} memfile = {data, size, 0};
	
		vio.get_filelen = [](void* user_data) -> sf_count_t {
			return ((MemFile*)user_data)->size;
		};
		vio.seek = [](sf_count_t offset, int whence, void* user_data) -> sf_count_t {
			MemFile* mf = (MemFile*)user_data;
			switch (whence) {
				case SEEK_SET: mf->pos = offset; break;
				case SEEK_CUR: mf->pos += offset; break;
				case SEEK_END: mf->pos = mf->size + offset; break;
			}
			if (mf->pos > mf->size) mf->pos = mf->size;
			return mf->pos;
		};
		vio.read = [](void* ptr, sf_count_t count, void* user_data) -> sf_count_t {
			MemFile* mf = (MemFile*)user_data;
			sf_count_t to_read = std::min((sf_count_t)(mf->size - mf->pos), count);
			memcpy(ptr, mf->data + mf->pos, to_read);
			mf->pos += to_read;
			return to_read;
		};
		vio.write = [](const void*, sf_count_t, void*) -> sf_count_t { return 0; };
		vio.tell = [](void* user_data) -> sf_count_t {
			return ((MemFile*)user_data)->pos;
		};
	
		SNDFILE* file = sf_open_virtual(&vio, SFM_READ, &info, &memfile);
		Sample s;
		if (!file) return s;
	
		s.channels = info.channels;
		s.sampleRate = info.samplerate;
		sf_count_t frames = info.frames;
		if (frames <= 0 || info.channels <= 0) {
			sf_close(file);
			return s;
		}
	
		std::vector<float> tmp(frames * info.channels);
		sf_read_float(file, tmp.data(), tmp.size());
		sf_close(file);
	
		if (force_stereo && info.channels >= 2) {
			s.is_stereo = true;
			s.dataL.resize(frames);
			s.dataR.resize(frames);
			for (sf_count_t i = 0; i < frames; ++i) {
				s.dataL[i] = tmp[i * info.channels + 0];
				s.dataR[i] = tmp[i * info.channels + 1];
			}
			s.channels = 2;
		} else if (info.channels == 1) {
			s.is_stereo = false;
			s.dataL = std::move(tmp);
			s.channels = 1;
		} else {
			s.is_stereo = false;
			s.dataL.resize(frames);
			for (sf_count_t i = 0; i < frames; ++i) {
				float sum = 0.0f;
				for (int c = 0; c < info.channels; ++c) sum += tmp[i * info.channels + c];
				s.dataL[i] = sum / (float)info.channels;
			}
			s.channels = 1;
		}
		return s;
	}

	// Agora esta função é um método da classe MyDrumKit
	void add_to_rr_group_with_velocity(int note, const char* relpath, int output, uint8_t min_vel, uint8_t max_vel, bool stereo = false) {
		auto data = pak.read(relpath);
		if (data.empty()) {
			fprintf(stderr, "[Real Sigma Drums] Sample não encontrado: %s\n", relpath);
			return;
		}

		Sample s = load_wav(data.data(), data.size(), stereo);
		if (s.dataL.empty()) return;

		auto& noteGroups = rr_groups[note];
		
		// Procura por um grupo com o mesmo output
		auto group_it = std::find_if(noteGroups.begin(), noteGroups.end(),[&](const RRGroup& g){ return g.output == output; });
		
		if (group_it == noteGroups.end()) {
			// Cria novo grupo
			RRGroup g;
			g.output = output;
			
			VelocityLayer layer;
			layer.min_vel = min_vel;
			layer.max_vel = max_vel;
			layer.samples.push_back(std::move(s));
			
			g.velocity_layers.push_back(std::move(layer));
			noteGroups.push_back(std::move(g));
		} else {
			// Procura layer existente ou cria novo
			auto layer_it = std::find_if(group_it->velocity_layers.begin(),group_it->velocity_layers.end(),[&](const VelocityLayer& l) {
				return l.min_vel == min_vel && l.max_vel == max_vel;
			});
			
			if (layer_it == group_it->velocity_layers.end()) {
				// Cria novo layer
				VelocityLayer layer;
				layer.min_vel = min_vel;
				layer.max_vel = max_vel;
				layer.samples.push_back(std::move(s));
				group_it->velocity_layers.push_back(std::move(layer));
			} else {
				// Adiciona ao layer existente (round robin)
				layer_it->samples.push_back(std::move(s));
			}
		}
	}

	// Agora esta função também é um método da classe MyDrumKit
	void load_instrument_samples(int midi_note, const std::string& base_name, int output, int num_velocity_layers, int num_rr,bool stereo = false) {
		for (int vel_layer = 1; vel_layer <= num_velocity_layers; ++vel_layer) {
			uint8_t min_vel, max_vel;
			
			// Calcula ranges automaticamente baseado no número de layers
			if (num_velocity_layers == 1) {
				min_vel = 1; max_vel = 127;
			} else if (num_velocity_layers == 2) {
				if (vel_layer == 1)      { min_vel = 1;  max_vel = 63; }
				else                     { min_vel = 64; max_vel = 127; }
			} else if (num_velocity_layers == 3) {
				if (vel_layer == 1)      { min_vel = 1;  max_vel = 42; }
				else if (vel_layer == 2) { min_vel = 43; max_vel = 84; }
				else                     { min_vel = 85; max_vel = 127; }
			} else if (num_velocity_layers == 4) {
				if (vel_layer == 1)      { min_vel = 1;  max_vel = 31; }
				else if (vel_layer == 2) { min_vel = 32; max_vel = 63; }
				else if (vel_layer == 3) { min_vel = 64; max_vel = 95; }
				else                     { min_vel = 96; max_vel = 127; }
			} else {
				// Fallback genérico para qualquer número de layers
				int range = 127 / num_velocity_layers;
				min_vel = (vel_layer - 1) * range + 1;
				max_vel = (vel_layer == num_velocity_layers) ? 127 : vel_layer * range;
			}
			
			for (int rr = 1; rr <= num_rr; ++rr) {
				std::string path = base_name + "_r" + std::to_string(rr) + "_v" + std::to_string(vel_layer) + ".wav"; 
				add_to_rr_group_with_velocity(midi_note, path.c_str(), output, min_vel, max_vel, stereo);
			}
		}
	}

	bool loadSamplesFromFolder(const std::string& base) {
		try {
			// Kick (35 &1368
			load_instrument_samples(35, "kick_in", 0, 1, 8);
			load_instrument_samples(36, "kick_in", 0, 1, 8);
			load_instrument_samples(35, "kick_out", 1, 1, 8);
			load_instrument_samples(36, "kick_out", 1, 1, 8);
			load_instrument_samples(35, "kick_overhead", 11, 1, 8, true);
			load_instrument_samples(36, "kick_overhead", 11, 1, 8, true);
			load_instrument_samples(35, "kick_room", 13, 1, 8, true);
			load_instrument_samples(36, "kick_room", 13, 1, 8, true);

			// Sidestick (37)
			load_instrument_samples(37, "sidestick_top", 2, 1, 4);
			load_instrument_samples(37, "sidestick_bottom", 3, 1, 4);
			load_instrument_samples(37, "sidestick_overhead", 11, 1, 4, true);
			load_instrument_samples(37, "sidestick_room", 13, 1, 4, true);

			// Snare (38 e 40)
			load_instrument_samples(38, "snare_top", 2, 7, 9);
			load_instrument_samples(40, "snare_top", 2, 7, 9);
			load_instrument_samples(38, "snare_bottom", 3, 7, 9);
			load_instrument_samples(40, "snare_bottom", 3, 7, 9);
			load_instrument_samples(38, "snare_overhead", 11, 7, 9, true);
			load_instrument_samples(40, "snare_overhead", 11, 7, 9, true);
			load_instrument_samples(38, "snare_room", 13, 7, 9, true);
			load_instrument_samples(40, "snare_room", 13, 7, 9, true);

			// HiHat Closed (42)
			load_instrument_samples(42, "hihat_closed", 4, 1, 7);
			load_instrument_samples(42, "hihat_closed_overhead", 11, 1, 7, true);
			load_instrument_samples(42, "hihat_closed_room", 13, 1, 7, true);

			// HiHat Pedal (44)
			load_instrument_samples(44, "hihat_pedal", 4, 1, 7);
			load_instrument_samples(44, "hihat_pedal_overhead", 11, 1, 7, true);
			load_instrument_samples(44, "hihat_pedal_room", 13, 1, 7, true);

			// HiHat Open (46)
			load_instrument_samples(46, "hihat_open", 4, 1, 7);
			load_instrument_samples(46, "hihat_open_overhead", 11, 1, 7, true);
			load_instrument_samples(46, "hihat_open_room", 13, 1, 7, true);

			// Choke groups (HiHat)
			for (auto& g : rr_groups[46]) g.chokeGroup = 1;
			for (auto& g : rr_groups[42]) g.chokeGroup = 1;
			for (auto& g : rr_groups[44]) g.chokeGroup = 1;

			// Toms
			load_instrument_samples(50, "racktom1", 5, 1, 7);
			load_instrument_samples(50, "racktom1_overhead", 11, 1, 7, true);
			load_instrument_samples(50, "racktom1_room", 13, 1, 7, true);

			load_instrument_samples(48, "racktom2", 6, 1, 7);
			load_instrument_samples(48, "racktom2_overhead", 11, 1, 7, true);
			load_instrument_samples(48, "racktom2_room", 13, 1, 7, true);

			load_instrument_samples(47, "racktom3", 7, 1, 7);
			load_instrument_samples(47, "racktom3_overhead", 11, 1, 7, true);
			load_instrument_samples(47, "racktom3_room", 13, 1, 7, true);

			load_instrument_samples(45, "floortom1", 8, 1, 7);
			load_instrument_samples(45, "floortom1_overhead", 11, 1, 7, true);
			load_instrument_samples(45, "floortom1_room", 13, 1, 7, true);

			load_instrument_samples(43, "floortom2", 9, 1, 7);
			load_instrument_samples(43, "floortom2_overhead", 11, 1, 7, true);
			load_instrument_samples(43, "floortom2_room", 13, 1, 7, true);

			load_instrument_samples(41, "floortom3", 10, 1, 7);
			load_instrument_samples(41, "floortom3_overhead", 11, 1, 7, true);
			load_instrument_samples(41, "floortom3_room", 13, 1, 7, true);

			// Pratos
			load_instrument_samples(49, "crash1_overhead", 11, 1, 7, true);
			load_instrument_samples(49, "crash1_room", 13, 1, 7, true);

			load_instrument_samples(57, "crash2_overhead", 11, 1, 7, true);
			load_instrument_samples(57, "crash2_room", 13, 1, 7, true);

			load_instrument_samples(51, "ride_overhead", 11, 1, 6, true);
			load_instrument_samples(51, "ride_room", 13, 1, 6, true);

			load_instrument_samples(53, "ride_bell_overhead", 11, 1, 7, true);
			load_instrument_samples(53, "ride_bell_room", 13, 1, 7, true);

			load_instrument_samples(59, "ride_edge_overhead", 11, 1, 5, true);
			load_instrument_samples(59, "ride_edge_room", 13, 1, 5, true);

			load_instrument_samples(52, "china_overhead", 11, 1, 7, true);
			load_instrument_samples(52, "china_room", 13, 1, 7, true);

			load_instrument_samples(55, "splash_overhead", 11, 1, 7, true);
			load_instrument_samples(55, "splash_room", 13, 1, 7, true);

			fprintf(stderr, "[Real Sigma Drums] %zu notas carregadas (CLAP)\n", rr_groups.size());
		} catch (...) {
			fprintf(stderr, "[Real Sigma Drums] Erro inesperado ao carregar samples\n");
			return false;
		}
		return true;
	}

	void run_render(uint32_t n_samples) {
		for (int i = 0; i < NUM_OUTPUTS; ++i)
			if (outputs[i])
				std::memset(outputs[i], 0, sizeof(float) * n_samples);

		for (auto it = voices.begin(); it != voices.end();) {
			auto& v = *it;
			if (!v.sample || v.sample->dataL.empty()) {
				it = voices.erase(it);
				continue;
			}
			const float* dataL = v.sample->dataL.data();
			const float* dataR = v.sample->is_stereo ? v.sample->dataR.data() : nullptr;
			for (uint32_t i = 0; i < n_samples && v.pos < v.length; ++i) {
				if (v.output >= 0 && v.output < NUM_OUTPUTS && outputs[v.output]) {
					outputs[v.output][i] += dataL[v.pos] * v.velocity;
				}
				if (dataR && v.output >= 0 && (v.output + 1) < NUM_OUTPUTS && outputs[v.output + 1]) {
					outputs[v.output + 1][i] += dataR[v.pos] * v.velocity;
				}
				v.pos++;
			}
			if (v.pos >= v.length)
				it = voices.erase(it);
			else
				++it;
		}
	}
};

// ---------- (CRÍTICO para ser reconhecido como instrumento) não mexa aqui não! ----------
static uint32_t note_ports_count(const clap_plugin_t* plugin, bool is_input) {
	return is_input ? 1u : 0u; // 1 entrada MIDI
}

static bool note_ports_get(const clap_plugin_t* plugin, uint32_t index, bool is_input, clap_note_port_info_t* info) {
	if (!is_input || index != 0) return false;

	memset(info, 0, sizeof(*info));
	info->id = 0;
	snprintf(info->name, sizeof(info->name), "MIDI In");
	info->supported_dialects = CLAP_NOTE_DIALECT_MIDI;
	info->preferred_dialect = CLAP_NOTE_DIALECT_MIDI;
	return true;
}

static const clap_plugin_note_ports_t note_ports_ext = {
	.count = note_ports_count,
	.get   = note_ports_get
};

// ---------- CLAP: audio ports extension ----------
static uint32_t audio_ports_count(const clap_plugin_t* plugin, bool is_input) {
	return is_input ? 0u : (uint32_t)NUM_OUTPUTS;
}

static bool audio_ports_get(const clap_plugin_t* plugin, uint32_t index, bool is_input, clap_audio_port_info_t* info) {
	if (is_input) return false;
	if (index >= (uint32_t)NUM_OUTPUTS) return false;

	// Nomes dos outputs
	static const char* names[NUM_OUTPUTS] = {
		"Kick In",
		"Kick Out",
		"Snare Top",
		"Snare Bottom",
		"HiHat",
		"Racktom 1",
		"Racktom 2",
		"Racktom 3",
		"Floortom 1",
		"Floortom 2",
		"Floortom 3",
		"Overhead L",
		"Overhead R",
		"Room L",
		"Room R"
	};

	memset(info, 0, sizeof(*info));
	info->id = index;
	snprintf(info->name, sizeof(info->name), "%s", names[index]);
	info->channel_count = 1;
	info->port_type = CLAP_PORT_MONO;

	if (index == 0) {
		info->flags = CLAP_AUDIO_PORT_IS_MAIN;
	} else {
		info->flags = 0;
	}

	info->in_place_pair = CLAP_INVALID_ID;

	return true;
}

static const clap_plugin_audio_ports_t audio_ports_ext = {
	.count = audio_ports_count,
	.get   = audio_ports_get
};

// ---------- CLAP plugin callbacks ----------
static bool my_init(const clap_plugin_t* plugin) {
	fprintf(stderr, "[Real Sigma Drums] CLAP init\n");
	return true;
}

static void my_destroy(const clap_plugin_t* plugin) {
	MyDrumKit* self = (MyDrumKit*)plugin->plugin_data;
	fprintf(stderr, "[Real Sigma Drums] CLAP destroy\n");
	if (self) delete self;
	delete plugin;
}

static bool my_activate(const clap_plugin_t* plugin, double sr, uint32_t min_frames, uint32_t max_frames) {
	MyDrumKit* self = (MyDrumKit*)plugin->plugin_data;
	if (!self) return false;
	self->sample_rate = sr;
	fprintf(stderr, "[Real Sigma Drums] CLAP activate SR=%.1f, min=%u, max=%u\n", sr, min_frames, max_frames);

	if (self->rr_groups.empty()) {
		const char* home = getenv("HOME");
		if (!home) home = ".";
		std::string pak_path = std::string(home) + "/.clap/realsigmadrums.clap/sounds.pak";
		
		fprintf(stderr, "[Real Sigma Drums] Abrindo pak: %s\n", pak_path.c_str());
		
		if (!self->pak.open(pak_path)) {
			fprintf(stderr, "[Real Sigma Drums] ERRO: não conseguiu abrir %s\n", pak_path.c_str());
			return false;
		}
	
		if (!self->loadSamplesFromFolder("")) {
			fprintf(stderr, "[Real Sigma Drums] AVISO: Erro ao carregar samples\n");
		}
	}
	return true;
}

static void my_deactivate(const clap_plugin_t* plugin) {
	fprintf(stderr, "[Real Sigma Drums] CLAP deactivate\n");
}

static bool my_start_processing(const clap_plugin_t* plugin) {
	fprintf(stderr, "[Real Sigma Drums] CLAP start_processing\n");
	return true;
}

static void my_stop_processing(const clap_plugin_t* plugin) {
	fprintf(stderr, "[Real Sigma Drums] CLAP stop_processing\n");
}

static void my_reset(const clap_plugin_t* plugin) {
	MyDrumKit* self = (MyDrumKit*)plugin->plugin_data;
	if (self) {
		self->voices.clear();
		fprintf(stderr, "[Real Sigma Drums] CLAP reset\n");
	}
}

static const void* my_get_extension(const clap_plugin_t* plugin, const char* id) {
	if (!strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &audio_ports_ext;
	if (!strcmp(id, CLAP_EXT_NOTE_PORTS)) return &note_ports_ext;
	return nullptr;
}

static void my_on_main_thread(const clap_plugin_t* plugin) { }

static clap_process_status my_process(const clap_plugin_t* plugin, const clap_process_t* process) {
	MyDrumKit* self = (MyDrumKit*)plugin->plugin_data;
	if (!self || !process) return CLAP_PROCESS_ERROR;

	uint32_t nframes = process->frames_count;
	if (nframes == 0) return CLAP_PROCESS_CONTINUE;

	// Conectar saídas de áudio
	for (int i = 0; i < NUM_OUTPUTS; ++i) self->outputs[i] = nullptr;

	if (process->audio_outputs_count > 0) {
		for (uint32_t i = 0; i < process->audio_outputs_count && i < (uint32_t)NUM_OUTPUTS; ++i) {
			const clap_audio_buffer_t* buf = &process->audio_outputs[i];
			if (buf && buf->channel_count > 0 && buf->data32) {
				self->outputs[i] = buf->data32[0];
			}
		}
	}

	// Processar eventos MIDI
	if (process->in_events) {
		const clap_input_events_t* in = process->in_events;
		uint32_t event_count = in->size(in);

		for (uint32_t i = 0; i < event_count; ++i) {
			const clap_event_header_t* hdr = in->get(in, i);
			if (!hdr) continue;

			// Processar evento MIDI
			if (hdr->space_id == CLAP_CORE_EVENT_SPACE_ID && hdr->type == CLAP_EVENT_MIDI) {
				const clap_event_midi_t* midi = (const clap_event_midi_t*)hdr;
				uint8_t status = midi->data[0] & 0xF0;
				uint8_t note   = midi->data[1];
				uint8_t vel    = midi->data[2];

				if (status == 0x90 && vel > 0) {  // Note ONc
					auto it = self->rr_groups.find(note);
					if (it != self->rr_groups.end()) {
						// Coleta chokeGroups
						std::vector<int> chokes;
						for (RRGroup& group : it->second) {
							if (group.chokeGroup > 0)
								chokes.push_back(group.chokeGroup);
						}

						// Aplica choke
						if (!chokes.empty()) {
							self->voices.erase(
								std::remove_if(self->voices.begin(), self->voices.end(),
									[&](const Voice& v){
										return std::find(chokes.begin(), chokes.end(), v.chokeGroup) != chokes.end();
									}),
								self->voices.end());
						}

						// Dispara samples com velocity layer correto
						for (RRGroup& group : it->second) {
							const Sample* sample = group.getSampleForVelocity(vel);  // MUDANÇA AQUI
							if (!sample || sample->dataL.empty()) continue;

							Voice v;
							v.sample = sample;
							v.pos = 0;
							v.length = (uint32_t)sample->dataL.size();
							v.output = group.output;
							v.chokeGroup = group.chokeGroup;
							float v_norm = (float)vel / 127.0f;
							v.velocity = v_norm; //* v_norm;

							self->voices.push_back(v);

							if (self->voices.size() > MAX_VOICES)
								self->voices.erase(self->voices.begin());
						}
					}
				}
			}
		}
	}

	// Renderizar áudio
	self->run_render(nframes);

	return CLAP_PROCESS_CONTINUE;
}

// ---------- Factory ----------
static uint32_t factory_get_plugin_count(const clap_plugin_factory_t* f) {
	return 1u;
}

static const clap_plugin_descriptor_t* factory_get_plugin_descriptor(const clap_plugin_factory_t* f, uint32_t index) {
	static const char* features[] = {
		CLAP_PLUGIN_FEATURE_INSTRUMENT,
		CLAP_PLUGIN_FEATURE_DRUM,
		CLAP_PLUGIN_FEATURE_SAMPLER,
		nullptr
	};

	static const clap_plugin_descriptor_t desc = {
		CLAP_VERSION_INIT,
		MYDRUMKIT_ID,
		MYDRUMKIT_NAME,
		"Real Sigma Music",
		"https://github.com/realsigmamusic/realsigmadrums",
		"",
		"",
		"1.5.0",
		"Acoustic drum sampler",
		features
	};
	return (index == 0) ? &desc : nullptr;
}

static const clap_plugin_t* factory_create_plugin(const clap_plugin_factory_t* factory, const clap_host_t* host, const char* plugin_id) {
	if (!host || strcmp(plugin_id, MYDRUMKIT_ID) != 0) return nullptr;

	MyDrumKit* self = new MyDrumKit();
	self->host = host;

	clap_plugin_t* p = new clap_plugin_t;
	memset(p, 0, sizeof(clap_plugin_t));
	p->desc = factory_get_plugin_descriptor(nullptr, 0);
	p->plugin_data = self;
	p->init = my_init;
	p->destroy = my_destroy;
	p->activate = my_activate;
	p->deactivate = my_deactivate;
	p->start_processing = my_start_processing;
	p->stop_processing = my_stop_processing;
	p->reset = my_reset;
	p->process = my_process;
	p->get_extension = my_get_extension;
	p->on_main_thread = my_on_main_thread;

	fprintf(stderr, "[Real Sigma Drums] Plugin carregado\n");
	return p;
}

static const clap_plugin_factory_t factory = {
	factory_get_plugin_count,
	factory_get_plugin_descriptor,
	factory_create_plugin
};

// ---------- Entry point ----------
extern "C" {
	const clap_plugin_entry_t clap_entry = {
		CLAP_VERSION_INIT,
		[](const char* plugin_path) -> bool {
			fprintf(stderr, "[Real Sigma Drums] CLAP init: %s\n", plugin_path ? plugin_path : "null");
			return true;
		},
		[]() -> void {
			fprintf(stderr, "[Real Sigma Drums] CLAP deinit\n");
		},
		[](const char* factory_id) -> const void* {
			fprintf(stderr, "[Real Sigma Drums] get_factory: %s\n", factory_id);
			if (!strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID)) return &factory;
			return nullptr;
		}
	};
}
