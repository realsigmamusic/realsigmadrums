// https://github.com/realsigmamusic/realsigmadrums

#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/atom/util.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>
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

#define REALSIGMADRUMS_URI "http://realsigmamusic.com/plugins/realsigmadrums"
#define NUM_OUTPUTS 15
#define MAX_VOICES 64

// Port indices
enum {
	PORT_MIDI_IN = 0,
	PORT_KICK_IN = 1,
	PORT_KICK_OUT = 2,
	PORT_SNARE_TOP = 3,
	PORT_SNARE_BOTTOM = 4,
	PORT_HIHAT = 5,
	PORT_RACKTOM1 = 6,
	PORT_RACKTOM2 = 7,
	PORT_RACKTOM3 = 8,
	PORT_FLOORTOM1 = 9,
	PORT_FLOORTOM2 = 10,
	PORT_FLOORTOM3 = 11,
	PORT_OVERHEAD_L = 12,
	PORT_OVERHEAD_R = 13,
	PORT_ROOM_L = 14,
	PORT_ROOM_R = 15
};

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
		
		fprintf(stderr, "[Real Sigma Drums LV2] PakReader Carregado: %u arquivos\n", count);
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
		for (auto& layer : velocity_layers) {
			if (velocity >= layer.min_vel && velocity <= layer.max_vel) {
				return layer.getNextSample();
			}
		}
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

typedef struct {
	LV2_URID atom_Sequence;
	LV2_URID midi_MidiEvent;
} MyDrumKitURIs;

struct MyDrumKit {
	std::map<int, std::vector<RRGroup>> rr_groups;
	std::vector<Voice> voices;
	float* ports[16]; // 1 MIDI + 15 audio outputs
	double sample_rate = 44100.0;
	PakReader pak;
	MyDrumKitURIs uris;
	LV2_URID_Map* map;
	std::string bundle_path;

	MyDrumKit() {
		for (int i = 0; i < 16; ++i) ports[i] = nullptr;
	}

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

	void add_to_rr_group_with_velocity(int note, const char* relpath, int output, uint8_t min_vel, uint8_t max_vel, bool stereo = false) {
		auto data = pak.read(relpath);
		if (data.empty()) {
			fprintf(stderr, "[Real Sigma Drums LV2] Sample não encontrado: %s\n", relpath);
			return;
		}

		Sample s = load_wav(data.data(), data.size(), stereo);
		if (s.dataL.empty()) return;

		auto& noteGroups = rr_groups[note];
		
		auto group_it = std::find_if(noteGroups.begin(), noteGroups.end(),[&](const RRGroup& g){ return g.output == output; });
		
		if (group_it == noteGroups.end()) {
			RRGroup g;
			g.output = output;
			
			VelocityLayer layer;
			layer.min_vel = min_vel;
			layer.max_vel = max_vel;
			layer.samples.push_back(std::move(s));
			
			g.velocity_layers.push_back(std::move(layer));
			noteGroups.push_back(std::move(g));
		} else {
			auto layer_it = std::find_if(group_it->velocity_layers.begin(),group_it->velocity_layers.end(),[&](const VelocityLayer& l) {
				return l.min_vel == min_vel && l.max_vel == max_vel;
			});
			
			if (layer_it == group_it->velocity_layers.end()) {
				VelocityLayer layer;
				layer.min_vel = min_vel;
				layer.max_vel = max_vel;
				layer.samples.push_back(std::move(s));
				group_it->velocity_layers.push_back(std::move(layer));
			} else {
				layer_it->samples.push_back(std::move(s));
			}
		}
	}

	void load_instrument_samples(int midi_note, const std::string& base_name, int output, int num_velocity_layers, int num_rr, bool stereo = false) {
		for (int vel_layer = 1; vel_layer <= num_velocity_layers; ++vel_layer) {
			uint8_t min_vel, max_vel;
			
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

	bool loadSamplesFromFolder() {
		try {
			// Kick (35 & 36)
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

			fprintf(stderr, "[Real Sigma Drums LV2] %zu notas carregadas\n", rr_groups.size());
		} catch (...) {
			fprintf(stderr, "[Real Sigma Drums LV2] Erro ao carregar samples\n");
			return false;
		}
		return true;
	}

	void run_render(uint32_t n_samples) {
		// Limpa outputs de áudio (pula porta 0 que é MIDI)
		for (int i = 1; i < 16; ++i) {
			if (ports[i])
				std::memset(ports[i], 0, sizeof(float) * n_samples);
		}

		for (auto it = voices.begin(); it != voices.end();) {
			auto& v = *it;
			if (!v.sample || v.sample->dataL.empty()) {
				it = voices.erase(it);
				continue;
			}
			
			const float* dataL = v.sample->dataL.data();
			const float* dataR = v.sample->is_stereo ? v.sample->dataR.data() : nullptr;
			
			// output + 1 porque porta 0 é MIDI
			int audio_port_L = v.output + 1;
			int audio_port_R = v.output + 2;
			
			for (uint32_t i = 0; i < n_samples && v.pos < v.length; ++i) {
				if (audio_port_L < 16 && ports[audio_port_L]) {
					ports[audio_port_L][i] += dataL[v.pos] * v.velocity;
				}
				if (dataR && audio_port_R < 16 && ports[audio_port_R]) {
					ports[audio_port_R][i] += dataR[v.pos] * v.velocity;
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

// LV2 Callbacks
static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double rate, const char* bundle_path, const LV2_Feature* const* features) {
	MyDrumKit* self = new MyDrumKit();
	self->sample_rate = rate;
	self->bundle_path = bundle_path;
	
	// Map URIDs
	for (int i = 0; features[i]; ++i) {
		if (!strcmp(features[i]->URI, LV2_URID__map)) {
			self->map = (LV2_URID_Map*)features[i]->data;
		}
	}
	
	if (!self->map) {
		fprintf(stderr, "[Real Sigma Drums LV2] ERRO: Host não forneceu LV2_URID_Map\n");
		delete self;
		return nullptr;
	}
	
	self->uris.atom_Sequence = self->map->map(self->map->handle, LV2_ATOM__Sequence);
	self->uris.midi_MidiEvent = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);
	
	fprintf(stderr, "[Real Sigma Drums LV2] Instantiate SR=%.1f, bundle=%s\n", rate, bundle_path);
	return (LV2_Handle)self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
	MyDrumKit* self = (MyDrumKit*)instance;
	if (port < 16) {
		self->ports[port] = (float*)data;
	}
}

static void activate(LV2_Handle instance) {
	MyDrumKit* self = (MyDrumKit*)instance;
	
	if (self->rr_groups.empty()) {
		std::string pak_path = self->bundle_path + "/sounds.pak";
		fprintf(stderr, "[Real Sigma Drums LV2] Carregando pak: %s\n", pak_path.c_str());
		
		if (!self->pak.open(pak_path)) {
			fprintf(stderr, "[Real Sigma Drums LV2] ERRO: não conseguiu abrir %s\n", pak_path.c_str());
			return;
		}
		
		if (!self->loadSamplesFromFolder()) {
			fprintf(stderr, "[Real Sigma Drums LV2] AVISO: Erro ao carregar samples\n");
		}
	}
	
	fprintf(stderr, "[Real Sigma Drums LV2] Activate\n");
}

static void run(LV2_Handle instance, uint32_t n_samples) {
	MyDrumKit* self = (MyDrumKit*)instance;
	if (!self) return;
	
	// Processar eventos MIDI da porta 0
	const LV2_Atom_Sequence* seq = (const LV2_Atom_Sequence*)self->ports[PORT_MIDI_IN];
	
	if (seq) {
		LV2_ATOM_SEQUENCE_FOREACH(seq, ev) {
			if (ev->body.type == self->uris.midi_MidiEvent) {
				const uint8_t* const msg = (const uint8_t*)(ev + 1);
				uint8_t status = msg[0] & 0xF0;
				
				if (status == 0x90 && msg[2] > 0) {  // Note ON
					uint8_t note = msg[1];
					uint8_t vel = msg[2];
					
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

						// Dispara samples
						for (RRGroup& group : it->second) {
							const Sample* sample = group.getSampleForVelocity(vel);
							if (!sample || sample->dataL.empty()) continue;

							Voice v;
							v.sample = sample;
							v.pos = 0;
							v.length = (uint32_t)sample->dataL.size();
							v.output = group.output;
							v.chokeGroup = group.chokeGroup;
							float v_norm = (float)vel / 127.0f;
							v.velocity = v_norm;

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
	self->run_render(n_samples);
}

static void deactivate(LV2_Handle instance) {
	fprintf(stderr, "[Real Sigma Drums LV2] Deactivate\n");
}

static void cleanup(LV2_Handle instance) {
	MyDrumKit* self = (MyDrumKit*)instance;
	fprintf(stderr, "[Real Sigma Drums LV2] Cleanup\n");
	if (self) delete self;
}

static const void* extension_data(const char* uri) {
	return nullptr;
}

static const LV2_Descriptor descriptor = {
	REALSIGMADRUMS_URI,
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

extern "C" {
	LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index) {
		return (index == 0) ? &descriptor : nullptr;
	}
}