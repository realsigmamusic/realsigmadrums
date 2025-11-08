// https://github.com/realsigmamusic/realsigmadrums

#include <clap/clap.h>
#include <clap/ext/audio-ports.h>
#include <clap/ext/note-ports.h>
#include <sndfile.h>
#include <vector>
#include <map>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <algorithm>
#include <cstdint>

#define MYDRUMKIT_ID "realsigmadrums"
#define MYDRUMKIT_NAME "Real Sigma Drums"
#define NUM_OUTPUTS 15
#define MAX_VOICES 64

struct Sample {
	std::vector<float> dataL;
	std::vector<float> dataR;
	int channels;
	int sampleRate;
	bool is_stereo;
	Sample() : channels(0), sampleRate(0), is_stereo(false) {}
};

struct RRGroup {
	std::vector<Sample> samples;
	uint32_t current_rr = 0;
	int output = 0;
	int chokeGroup = 0;
	const Sample* getNextSample() {
		if (samples.empty()) return nullptr;
		const Sample* s = &samples[current_rr];
		current_rr = (current_rr + 1) % samples.size();
		return s;
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

	MyDrumKit() {
		for (int i = 0; i < NUM_OUTPUTS; ++i) outputs[i] = nullptr;
	}

	bool loadSamplesFromFolder(const std::string& base) {
		try {
			// Kick (35 & 36)
			for (int i = 1; i <= 8; ++i) {
				std::string path = base + "/kick_in_v9_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(35, path.c_str(), 0);
				add_to_rr_group_path(36, path.c_str(), 0);
			}
			for (int i = 1; i <= 8; ++i) {
				std::string path = base + "/kick_out_v9_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(35, path.c_str(), 1);
				add_to_rr_group_path(36, path.c_str(), 1);
			}
			for (int i = 1; i <= 8; ++i) {
				std::string path = base + "/kick_overhead_v9_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(35, path.c_str(), 11, true);
				add_to_rr_group_path(36, path.c_str(), 11, true);
			}
			for (int i = 1; i <= 8; ++i) {
				std::string path = base + "/kick_room_v9_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(35, path.c_str(), 13, true);
				add_to_rr_group_path(36, path.c_str(), 13, true);
			}

			// Sidestick (37)
			for (int i = 1; i <= 4; ++i) {
				std::string path = base + "/sidestick_top_v4_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(37, path.c_str(), 2);
			}
			for (int i = 1; i <= 4; ++i) {
				std::string path = base + "/sidestick_bottom_v4_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(37, path.c_str(), 3);
			}
			for (int i = 1; i <= 4; ++i) {
				std::string path = base + "/sidestick_overhead_v4_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(37, path.c_str(), 11, true);
			}
			for (int i = 1; i <= 4; ++i) {
				std::string path = base + "/sidestick_room_v4_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(37, path.c_str(), 13, true);
			}

			// Snare (38 and 40)
			for (int i = 1; i <= 9; ++i) {
				std::string path = base + "/snare_top_v7_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(38, path.c_str(), 2);
				add_to_rr_group_path(40, path.c_str(), 2);
			}
			for (int i = 1; i <= 9; ++i) {
				std::string path = base + "/snare_bottom_v7_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(38, path.c_str(), 3);
				add_to_rr_group_path(40, path.c_str(), 3);
			}
			for (int i = 1; i <= 9; ++i) {
				std::string path = base + "/snare_overhead_v7_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(38, path.c_str(), 11, true);
				add_to_rr_group_path(40, path.c_str(), 11, true);
			}
			for (int i = 1; i <= 9; ++i) {
				std::string path = base + "/snare_room_v7_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(38, path.c_str(), 13, true);
				add_to_rr_group_path(40, path.c_str(), 13, true);
			}

			// HiHat Closed
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/hihat_closed_v6_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(42, path.c_str(), 4);
			}
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/hihat_closed_overhead_v6_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(42, path.c_str(), 11, true);
			}
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/hihat_closed_room_v6_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(42, path.c_str(), 13, true);
			}

			// HiHat Pedal
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/hihat_pedal_v4_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(44, path.c_str(), 4);
			}
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/hihat_pedal_overhead_v4_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(44, path.c_str(), 11, true);
			}
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/hihat_pedal_room_v4_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(44, path.c_str(), 13, true);
			}

			// HiHat Open
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/hihat_open_v6_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(46, path.c_str(), 4);
			}
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/hihat_open_overhead_v6_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(46, path.c_str(), 11, true);
			}
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/hihat_open_room_v6_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(46, path.c_str(), 13, true);
			}

			// Choke groups (HiHat)
			for (auto& g : rr_groups[46]) g.chokeGroup = 1;
			for (auto& g : rr_groups[42]) g.chokeGroup = 1;
			for (auto& g : rr_groups[44]) g.chokeGroup = 1;

			// Racktom 1
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/racktom1_v8_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(50, path.c_str(), 4);
			}
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/racktom1_overhead_v8_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(50, path.c_str(), 11, true);
			}
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/racktom1_room_v8_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(50, path.c_str(), 13, true);
			}

			// Racktom 2
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/racktom2_v8_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(48, path.c_str(), 4);
			}
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/racktom2_overhead_v8_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(48, path.c_str(), 11, true);
			}
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/racktom2_room_v8_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(48, path.c_str(), 13, true);
			}

			// Racktom 3
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/racktom3_v8_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(47, path.c_str(), 4);
			}
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/racktom3_overhead_v8_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(47, path.c_str(), 11, true);
			}
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/racktom3_room_v8_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(47, path.c_str(), 13, true);
			}

			// Floorom 1
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/floortom1_v8_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(45, path.c_str(), 4);
			}
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/floortom1_overhead_v8_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(45, path.c_str(), 11, true);
			}
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/floortom1_room_v8_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(45, path.c_str(), 13, true);
			}

			// Floortom 2
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/floortom2_v8_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(43, path.c_str(), 4);
			}
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/floortom2_overhead_v8_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(43, path.c_str(), 11, true);
			}
			for (int i = 1; i <= 7; ++i) {
				std::string path = base + "/floortom2_room_v8_r" + std::to_string(i) + ".wav";
				add_to_rr_group_path(43, path.c_str(), 13, true);
			}

			fprintf(stderr, "MyDrumKit: %zu notas carregadas (CLAP)\n", rr_groups.size());
		} catch (...) {
			fprintf(stderr, "MyDrumKit: Erro inesperado ao carregar samples\n");
			return false;
		}
		return true;
	}

	static Sample load_wav(const char* path, bool force_stereo = false) {
		SF_INFO info{};
		SNDFILE* file = sf_open(path, SFM_READ, &info);
		Sample s;
		if (!file) {
			fprintf(stderr, "MyDrumKit: Erro ao carregar %s: %s\n", path, sf_strerror(nullptr));
			return s;
		}
		s.channels = info.channels;
		s.sampleRate = info.samplerate;
		sf_count_t frames = info.frames;
		if (frames <= 0 || info.channels <= 0) {
			sf_close(file);
			return s;
		}
		std::vector<float> tmp(frames * info.channels);
		sf_count_t read = sf_read_float(file, tmp.data(), tmp.size());
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

	void add_to_rr_group_path(int note, const char* fullpath, int output, bool stereo = false) {
	    Sample s = load_wav(fullpath, stereo);
	    if (s.dataL.empty())
	        return;

	    // verifica se já existe grupo com mesmo output (mesmo mic)
	    auto& noteGroups = rr_groups[note];
	    auto it = std::find_if(noteGroups.begin(), noteGroups.end(),
	                           [&](const RRGroup& g){ return g.output == output; });

	    if (it == noteGroups.end()) {
	        // cria novo grupo (novo mic)
	        RRGroup g;
	        g.output = output;
	        g.samples.push_back(std::move(s));
	        noteGroups.push_back(std::move(g));
	    } else {
	        // adiciona round robin dentro do mesmo mic
	        it->samples.push_back(std::move(s));
	    }
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
		std::string samples_dir = std::string(home) + "/.clap/realsigmadrums.clap/samples";
		fprintf(stderr, "[Real Sigma Drums] Carregando samples de: %s\n", samples_dir.c_str());
		if (!self->loadSamplesFromFolder(samples_dir)) {
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
				uint8_t vel	= midi->data[2];

				// só pra conferir se está tudo ok...
				//fprintf(stderr, "[Real Sigma Drums] MIDI: status=0x%02X note=%d vel=%d\n", status, note, vel);

				if (status == 0x90 && vel > 0) {  // Note ON
				    auto it = self->rr_groups.find(note);
				    if (it != self->rr_groups.end()) {
				        // 1) coleta chokeGroups que devem ser aplicados para esta nota
				        std::vector<int> chokes;
				        for (RRGroup& group : it->second) {
				            if (group.chokeGroup > 0)
				                chokes.push_back(group.chokeGroup);
				        }
					
				        // 2) aplica choke: remove todas as vozes ativas que pertencem a esses chokeGroups
				        if (!chokes.empty()) {
				            self->voices.erase(
				                std::remove_if(self->voices.begin(), self->voices.end(),
				                    [&](const Voice& v){
				                        return std::find(chokes.begin(), chokes.end(), v.chokeGroup) != chokes.end();
				                    }),
				                self->voices.end());
				        }
					
				        // 3) agora dispare todas as RRGroups (cada mic) para esta nota
				        for (RRGroup& group : it->second) {
				            const Sample* sample = group.getNextSample();
				            if (!sample || sample->dataL.empty()) continue;
						
				            Voice v;
				            v.sample = sample;
				            v.pos = 0;
				            v.length = (uint32_t)sample->dataL.size();
				            v.output = group.output;
				            v.chokeGroup = group.chokeGroup;
				            float v_norm = (float)vel / 127.0f;
				            v.velocity = v_norm * v_norm;
						
				            self->voices.push_back(v);
						
				            // limite de vozes para não bugar tudo na hora do play
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
		"1.0.0",
		"Acoustic drum sampler",
		features
	};
	return (index == 0) ? &desc : nullptr;
}

static const clap_plugin_t* factory_create_plugin(const clap_plugin_factory_t* factory,
												   const clap_host_t* host,
												   const char* plugin_id) {
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

	fprintf(stderr, "[Real Sigma Drums] Plugin criado\n");
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