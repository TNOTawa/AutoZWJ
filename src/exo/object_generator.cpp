#include "object_generator.h"
#include "effect/effect_dict.h"
#include <cmath>
#include <sstream>
#include <algorithm>
#include <iomanip>

double seconds_to_frames(double seconds, int fps_num, int fps_den) {
    double fps = (double)fps_num / (double)fps_den;
    return std::round(seconds * fps * 1000.0) / 1000.0;
}

static int frame_to_int(double frame) {
    return (int)std::round(frame);
}

static std::string format_double(double v, int precision) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(precision) << v;
    std::string s = ss.str();
    size_t dot = s.find('.');
    if (dot != std::string::npos) {
        while (s.size() > dot + 1 && s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
    }
    return s;
}

static bool is_audio_type(const std::string& filetype) {
    return filetype == "AUDIO" || filetype == "WAVE" || filetype == "MP3" ||
           filetype == "VORBIS" || filetype == "FLAC";
}

static bool is_video_type(const std::string& filetype) {
    return filetype == "VIDEO";
}

static bool is_xmidi_type(const std::string& filetype) {
    return filetype == "XMIDI";
}

static bool is_text_type(const std::string& filetype) {
    return filetype.size() >= 5 && filetype.substr(0, 5) == "TEXT:";
}

static bool is_image_type(const std::string& filetype) {
    return filetype == "IMAGE";
}

static std::string generate_effect_block(const EffectDef& effect,
                                          const std::map<std::string, std::string>& overrides) {
    std::ostringstream ss;
    ss << "effect.name=" << effect.name_ja << "\n";
    for (auto& param : effect.params) {
        std::string value = param.default_value;
        auto it = overrides.find(param.name);
        if (it != overrides.end()) value = it->second;
        if (!value.empty()) {
            ss << param.name << "=" << value << "\n";
        }
    }
    return ss.str();
}

static std::string make_object_alias_item(int idx, int layer, int start_frame, int end_frame,
                                           int clipping, int camera,
                                           const std::string& effect_blocks) {
    std::ostringstream ss;
    ss << "[" << idx << "]\n";
    ss << "layer=" << layer << "\n";
    ss << "frame=" << start_frame << "," << end_frame << "\n";
    ss << "overlay=1\n";
    ss << "group=" << (idx + 1) << "\n";
    ss << "clipping=" << clipping << "\n";
    ss << "camera=" << camera << "\n";
    ss << effect_blocks;
    return ss.str();
}

int generate_objects(const ObjDict& objdict,
                     const std::vector<TrackNode>& tracks,
                     const OutputConfig& config,
                     const std::vector<std::string>& file_paths,
                     std::vector<GenerateContext>& out_contexts) {
    out_contexts.clear();

    struct LayerOccupancy {
        std::vector<std::pair<int, int>> ranges;
        bool overlaps(int start, int end) const {
            for (auto& r : ranges) {
                if (start < r.second && end > r.first) return true;
            }
            return false;
        }
        void add(int start, int end) {
            ranges.push_back({start, end});
        }
    };

    std::vector<LayerOccupancy> layers;
    layers.resize(200);

    int current_layer_base = 0;
    int item_idx = 0;
    int prev_pitch_int = -999;

    size_t item_start = 1;

    const EffectDef* eff_std_draw = find_effect("標準描画");
    const EffectDef* eff_flip = find_effect("反転");

    for (size_t ti = 0; ti < tracks.size(); ti++) {
        if (!tracks[ti].selected) {
            while (item_start < objdict.pos.size() && objdict.pos[item_start] != -1.0) item_start++;
            if (item_start < objdict.pos.size()) item_start++;
            continue;
        }

        size_t track_end = objdict.pos.size();
        for (size_t j = item_start + 1; j < objdict.pos.size(); j++) {
            if (objdict.pos[j] == -1.0) { track_end = j; break; }
        }

        int track_layer = 0;

        for (size_t i = item_start; i < track_end; i++) {
            double pos_sec = objdict.pos[i];
            double len_sec = objdict.length[i];
            if (pos_sec < -0.5) continue;

            int loop = objdict.loop[i];
            double playrate = objdict.playrate[i];
            double soffs = objdict.soffs[i];
            double pitch = objdict.pitch[i];
            int fileidx = objdict.fileidx[i];
            const std::string& filetype = objdict.filetype[i];

            double start_frame_d = seconds_to_frames(pos_sec, config.fps_num, config.fps_den);
            double end_frame_d = seconds_to_frames(pos_sec + len_sec, config.fps_num, config.fps_den);
            int sf = frame_to_int(start_frame_d);
            int ef = frame_to_int(end_frame_d);
            if (ef <= sf) ef = sf + 1;

            std::string alias;
            int use_layer = current_layer_base + track_layer;
            std::string effect_blocks;
            std::wstring obj_name;

            bool has_media = (fileidx >= 0 && fileidx < (int)file_paths.size() && !file_paths[fileidx].empty());

            if (config.output_type == 0) {
                if (is_video_type(filetype) && has_media) {
                    effect_blocks = "[0.0]\n";
                    effect_blocks += "effect.name=動画ファイル\n";
                    effect_blocks += "再生位置=" + format_double(soffs, 1) + "\n";
                    effect_blocks += "再生速度=" + format_double(playrate * 100.0, 1) + "\n";
                    effect_blocks += "ファイル=" + file_paths[fileidx] + "\n";
                    effect_blocks += "ループ再生=" + std::to_string(loop) + "\n";
                    effect_blocks += "[0.1]\n";
                    if (eff_std_draw) effect_blocks += generate_effect_block(*eff_std_draw, {});
                    obj_name = utf8_to_wide(file_paths[fileidx]);
                    size_t sep = obj_name.find_last_of(L'\\');
                    if (sep != std::wstring::npos) obj_name = obj_name.substr(sep + 1);
                } else if (is_audio_type(filetype) && has_media) {
                    effect_blocks = "[0.0]\n";
                    effect_blocks += "effect.name=音声ファイル\n";
                    effect_blocks += "再生位置=" + format_double(soffs, 1) + "\n";
                    effect_blocks += "再生速度=" + format_double(playrate * 100.0, 1) + "\n";
                    effect_blocks += "ファイル=" + file_paths[fileidx] + "\n";
                    effect_blocks += "ループ再生=" + std::to_string(loop) + "\n";
                    effect_blocks += "[0.1]\n";
                    if (eff_std_draw) effect_blocks += generate_effect_block(*eff_std_draw, {});
                    obj_name = utf8_to_wide(file_paths[fileidx]);
                    size_t sep = obj_name.find_last_of(L'\\');
                    if (sep != std::wstring::npos) obj_name = obj_name.substr(sep + 1);
                } else if (is_xmidi_type(filetype)) {
                    effect_blocks = "[0.0]\n";
                    effect_blocks += "effect.name=図形\n";
                    effect_blocks += "サイズ=200\n";
                    effect_blocks += "[0.1]\n";
                    if (eff_std_draw) effect_blocks += generate_effect_block(*eff_std_draw, {});
                    std::wstring note_name = L"Note " + std::to_wstring(item_idx);
                    if (item_idx < (int)objdict.midi_note.size()) {
                        static const char* note_names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
                        int n = objdict.midi_note[item_idx];
                        note_name = L"MIDI " + std::to_wstring(n) + L" " +
                            utf8_to_wide(std::string(note_names[n % 12]) + std::to_string(n / 12 - 1));
                    }
                    obj_name = note_name;
                } else if (is_text_type(filetype)) {
                    std::string text = filetype.substr(5);
                    effect_blocks = "[0.0]\n";
                    effect_blocks += "effect.name=テキスト\n";
                    effect_blocks += "テキスト=" + text + "\n";
                    effect_blocks += "サイズ=100.00\n";
                    effect_blocks += "[0.1]\n";
                    if (eff_std_draw) effect_blocks += generate_effect_block(*eff_std_draw, {});
                    obj_name = L"Text";
                } else if (is_image_type(filetype) && has_media) {
                    effect_blocks = "[0.0]\n";
                    effect_blocks += "effect.name=画像ファイル\n";
                    effect_blocks += "ファイル=" + file_paths[fileidx] + "\n";
                    effect_blocks += "[0.1]\n";
                    if (eff_std_draw) effect_blocks += generate_effect_block(*eff_std_draw, {});
                    obj_name = utf8_to_wide(file_paths[fileidx]);
                    size_t sep = obj_name.find_last_of(L'\\');
                    if (sep != std::wstring::npos) obj_name = obj_name.substr(sep + 1);
                } else if (has_media) {
                    effect_blocks = "[0.0]\n";
                    effect_blocks += "effect.name=動画ファイル\n";
                    effect_blocks += "再生位置=" + format_double(soffs, 1) + "\n";
                    effect_blocks += "再生速度=" + format_double(playrate * 100.0, 1) + "\n";
                    effect_blocks += "ファイル=" + file_paths[fileidx] + "\n";
                    effect_blocks += "[0.1]\n";
                    if (eff_std_draw) effect_blocks += generate_effect_block(*eff_std_draw, {});
                    obj_name = utf8_to_wide(file_paths[fileidx]);
                    size_t sep = obj_name.find_last_of(L'\\');
                    if (sep != std::wstring::npos) obj_name = obj_name.substr(sep + 1);
                } else {
                    effect_blocks = "[0.0]\n";
                    effect_blocks += "effect.name=図形\n";
                    effect_blocks += "サイズ=100\n";
                    effect_blocks += "[0.1]\n";
                    if (eff_std_draw) effect_blocks += generate_effect_block(*eff_std_draw, {});
                    obj_name = L"Empty";
                }

                if (config.alt_flip && config.flip_type != FLIP_NONE && eff_flip) {
                    int current_pitch_int = (int)std::round(pitch);
                    bool same_pitch = (current_pitch_int == prev_pitch_int);
                    prev_pitch_int = current_pitch_int;

                    bool should_flip = false;
                    if (item_idx % 2 == 0) {
                        should_flip = !same_pitch || item_idx == 0;
                    } else {
                        should_flip = true;
                    }

                    if (should_flip) {
                        std::ostringstream fe;
                        fe << "[0.2]\n";
                        fe << "effect.name=反転\n";
                        if (config.flip_type == FLIP_HORIZONTAL) {
                            fe << "左右反転=1\n";
                        } else if (config.flip_type == FLIP_VERTICAL) {
                            fe << "上下反転=1\n";
                        } else if (config.flip_type >= 3) {
                            fe << "左右反転=1\n";
                            fe << "上下反転=1\n";
                        }
                        effect_blocks += fe.str();
                    }
                }
            } else {
                effect_blocks = "[0.0]\n";
                effect_blocks += "effect.name=図形\n";
                effect_blocks += "サイズ=100\n";
                effect_blocks += "[0.1]\n";
                if (eff_std_draw) effect_blocks += generate_effect_block(*eff_std_draw, {});
                obj_name = L"Object " + std::to_wstring(item_idx);
            }

            alias = make_object_alias_item(0, use_layer, sf, ef, config.clipping, config.is_ex_set, effect_blocks);

            int actual_layer = use_layer;
            while (actual_layer < (int)layers.size() && layers[actual_layer].overlaps(sf, ef))
                actual_layer++;

            if (actual_layer >= (int)layers.size()) layers.resize(actual_layer + 10);
            layers[actual_layer].add(sf, ef);

            GenerateContext ctx;
            ctx.alias_data = alias;
            ctx.layer = actual_layer;
            ctx.start_frame = sf;
            ctx.length = ef - sf;
            ctx.name = obj_name;
            out_contexts.push_back(ctx);

            item_idx++;
        }

        current_layer_base += track_layer + 10;
        item_start = track_end + 1;
    }

    return (int)out_contexts.size();
}
