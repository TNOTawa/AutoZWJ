#include "generation/generation.h"
#include "script/expr_evaluator.h"
#include "script/variable_subst.h"
#include <algorithm>
#include <cctype>
#include <cmath>

static double frame_round(double v, bool use_round_up) {
    return use_round_up ? std::ceil(v) : std::round(v);
}

static std::vector<int> generate_shuffled_order(int pool_size, uint32_t seed) {
    std::vector<int> order(pool_size);
    for (int i = 0; i < pool_size; i++) order[i] = i;
    for (int i = pool_size - 1; i > 0; i--) {
        seed = seed * 1103515245u + 12345u;
        int j = static_cast<int>(seed % static_cast<uint32_t>(i + 1));
        std::swap(order[i], order[j]);
    }
    return order;
}

#include <sstream>
#include <cstdint>
#include <format>
#include <map>
#include <unordered_map>
#include <cstdlib>

struct ItemInterval {
    size_t idx;
    double pos_sec;
    double obj_fp, bf;
    double pitch;
    int sf, ef;
    int loop;
    double playrate, soffs;
    int fileidx;
    std::string file_path;
    int chord_index = 1;
    int chord_count = 1;
    bool hold_last_frame = false;
    bool hold_last_prefix = false;
};

static int pick_template_index(int item_count_global, int chord_index, int pool_size,
                                int strategy, const OutputConfig& config, int prev_tpl_idx,
                                const std::vector<int>& shuffled_order, uint32_t seed) {
    if (pool_size <= 1) return 0;

    switch (strategy) {
    case 1: {
        int idx;
        switch (config.mapping_sequential_order) {
        case 0: idx = item_count_global % pool_size; break;
        case 1: idx = (pool_size - 1) - (item_count_global % pool_size); break;
        case 2: {
            if (!shuffled_order.empty())
                idx = shuffled_order[item_count_global % pool_size];
            else
                idx = item_count_global % pool_size;
            break;
        }
        default: idx = item_count_global % pool_size; break;
        }
        return idx;
    }
    case 2: {
        uint32_t rnd = seed ^ static_cast<uint32_t>(item_count_global * 2654435761u);
        rnd = rnd * 1103515245u + 12345u;
        int idx = static_cast<int>(rnd % static_cast<uint32_t>(pool_size));
        if (config.mapping_no_consecutive && idx == prev_tpl_idx && pool_size > 1) {
            idx = (idx + 1) % pool_size;
        }
        return idx;
    }
    case 3: {
        int idx = (chord_index - 1) % pool_size;
        return idx;
    }
    default:
        return 0;
    }
}

static void inject_single_param(std::string& chain, const ParamBake& pb, size_t sec_pos, size_t sec_end) {
    std::string search = "\n" + pb.param_name + "=";
    size_t param_pos = chain.find(search, sec_pos);
    if (param_pos == std::string::npos || param_pos >= sec_end) {
        std::string new_line = "\n" + pb.param_name + "=" + pb.param_value;
        if (sec_end == chain.size()) {
            chain += new_line;
        } else {
            chain.insert(sec_end, new_line);
        }
    } else {
        size_t line_start = param_pos + 1;
        size_t line_end = chain.find('\n', line_start);
        if (line_end == std::string::npos || line_end > sec_end)
            line_end = sec_end;
        chain.replace(line_start, line_end - line_start,
                      pb.param_name + "=" + pb.param_value);
    }
}

static void inject_motion_param(std::string& chain, const ParamBake& pb, size_t sec_pos, size_t sec_end) {
    std::string search = "\n" + pb.param_name + "=";
    size_t param_pos = chain.find(search, sec_pos);
    if (param_pos == std::string::npos || param_pos >= sec_end) {
        std::string new_line = "\n" + pb.param_name + "=" + pb.param_value;
        if (sec_end == chain.size()) {
            chain += new_line;
        } else {
            chain.insert(sec_end, new_line);
        }
        return;
    }

    size_t line_start = param_pos + 1;
    size_t line_end = chain.find('\n', line_start);
    if (line_end == std::string::npos || line_end > sec_end)
        line_end = sec_end;

    std::string old_line = chain.substr(line_start, line_end - line_start);
    size_t eq = old_line.find('=');
    if (eq == std::string::npos) {
        chain.replace(line_start, line_end - line_start,
                      pb.param_name + "=" + pb.param_value);
        return;
    }

    std::string old_val = old_line.substr(eq + 1);
    size_t c1 = old_val.find(',');
    if (c1 == std::string::npos) {
        chain.replace(line_start, line_end - line_start,
                      pb.param_name + "=" + pb.param_value);
        return;
    }
    size_t c2 = old_val.find(',', c1 + 1);
    if (c2 == std::string::npos) {
        chain.replace(line_start, line_end - line_start,
                      pb.param_name + "=" + pb.param_value);
        return;
    }

    std::string new_val = pb.param_value + "," + pb.motion_end_value + old_val.substr(c2);
    chain.replace(line_start, line_end - line_start,
                  pb.param_name + "=" + new_val);
}

static void inject_param_bakes(std::string& chain, const std::vector<ParamBake>& bakes) {
    for (const auto& pb : bakes) {
        if (!pb.active) continue;

        std::string marker = "[0." + std::to_string(pb.effect_index) + "]";
        size_t sec_pos = chain.find(marker);
        if (sec_pos == std::string::npos) continue;

        size_t sec_end = chain.find("\n[0.", sec_pos + 1);
        if (sec_end == std::string::npos) sec_end = chain.size();

        if (pb.is_motion) {
            inject_motion_param(chain, pb, sec_pos, sec_end);
        } else {
            inject_single_param(chain, pb, sec_pos, sec_end);
        }
    }
}

static std::string trim_motion_token(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}

static bool is_number_token(const std::string& value) {
    std::string token = trim_motion_token(value);
    if (token.empty()) return false;
    char* end = nullptr;
    std::strtod(token.c_str(), &end);
    return end != token.c_str() && *end == '\0';
}

// Convert animated parameters to their final values for the continuation object.
static void hold_last_motion_values(std::string& chain) {
    struct Line {
        std::string key;
        std::string value;
        bool removed = false;
    };

    std::vector<Line> lines;
    std::vector<std::string> raw_lines;
    std::istringstream input(chain);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        raw_lines.push_back(line);
        size_t eq = line.find('=');
        if (eq == std::string::npos || line.rfind("[0.", 0) == 0) {
            lines.push_back({});
        } else {
            lines.push_back({line.substr(0, eq), line.substr(eq + 1), false});
        }
    }

    for (size_t i = 0; i < lines.size(); i++) {
        auto& current = lines[i];
        bool is_pair_endpoint = current.key.size() > 2 &&
            current.key.substr(current.key.size() - 2) == ".1";
        if (current.key.empty() || current.key == "effect.name" || is_pair_endpoint) continue;

        size_t c1 = current.value.find(',');
        size_t c2 = c1 == std::string::npos ? std::string::npos : current.value.find(',', c1 + 1);
        if (c2 != std::string::npos &&
            is_number_token(current.value.substr(0, c1)) &&
            is_number_token(current.value.substr(c1 + 1, c2 - c1 - 1))) {
            current.value = trim_motion_token(current.value.substr(c1 + 1, c2 - c1 - 1));
            continue;
        }

        if (!is_number_token(current.value)) continue;
        std::string end_key = current.key + ".1";
        for (size_t j = i + 1; j < lines.size(); j++) {
            if (raw_lines[j].rfind("[0.", 0) == 0) break;
            if (lines[j].key == end_key && is_number_token(lines[j].value)) {
                current.value = trim_motion_token(lines[j].value);
                lines[j].removed = true;
                break;
            }
        }
    }

    std::ostringstream output;
    for (size_t i = 0; i < raw_lines.size(); i++) {
        if (lines[i].removed) continue;
        if (!lines[i].key.empty()) output << lines[i].key << "=" << lines[i].value;
        else output << raw_lines[i];
        if (i + 1 < raw_lines.size()) output << '\n';
    }
    chain = output.str();
}

static void inject_presets(std::string& chain, const std::vector<PresetEntry>& presets) {
    struct Injection { int position; std::string block; };
    std::vector<Injection> injections;
    for (const auto& p : presets) {
        if (!p.active) continue;
        if (p.effect_block.empty()) continue;
        injections.push_back({p.position, p.effect_block});
    }
    if (injections.empty()) return;

    std::stable_sort(injections.begin(), injections.end(),
        [](const Injection& a, const Injection& b) {
            if (a.position == -1) return false;
            if (b.position == -1) return true;
            return a.position < b.position;
        });

    struct Section {
        std::string header;
        std::string body;
    };
    std::vector<Section> sections;

    size_t pos = 0;
    while (pos < chain.size()) {
        size_t marker = chain.find("[0.", pos);
        if (marker == std::string::npos) break;

        size_t bracket_end = chain.find(']', marker);
        if (bracket_end == std::string::npos) break;

        std::string header = chain.substr(marker, bracket_end - marker + 1);
        size_t body_start = bracket_end + 1;
        if (body_start < chain.size() && chain[body_start] == '\n')
            body_start++;

        size_t next_marker = chain.find("[0.", body_start);
        std::string body;
        if (next_marker == std::string::npos) {
            body = chain.substr(body_start);
        } else {
            body = chain.substr(body_start, next_marker - body_start);
        }

        sections.push_back({header, body});
        pos = (next_marker == std::string::npos) ? chain.size() : next_marker;
    }

    int inserted = 0;
    for (const auto& inj : injections) {
        std::string block = inj.block;
        size_t placeholder = block.find("[0.N]");
        if (placeholder != std::string::npos) {
            std::string new_num = "[0." + std::to_string((int)sections.size() + inserted) + "]";
            block.replace(placeholder, 5, new_num);
        }

        size_t hdr_end = block.find(']');
        std::string body;
        if (hdr_end != std::string::npos) {
            body = block.substr(hdr_end + 1);
            if (!body.empty() && body[0] == '\n')
                body = body.substr(1);
        }
        std::string header = (hdr_end != std::string::npos) ? block.substr(0, hdr_end + 1) : "[0.N]";

        Section sec{header, body};
        if (inj.position == -1 || inj.position >= (int)sections.size()) {
            sections.push_back(sec);
        } else {
            sections.insert(sections.begin() + inj.position, sec);
        }
        inserted++;
    }

    std::ostringstream out;
    for (size_t i = 0; i < sections.size(); i++) {
        out << "[0." << i << "]\n" << sections[i].body;
    }
    chain = out.str();
}

static std::string build_flip_block(int flip_type, int layer_item_count) {
    std::ostringstream fe;
    fe << "[0.N]\neffect.name=反転\n";

    if (flip_type == FLIP_HORIZONTAL) {
        fe << "左右反転=" << ((layer_item_count % 2 == 1) ? "1" : "0") << "\n";
        fe << "上下反転=0\n";
    } else if (flip_type == FLIP_VERTICAL) {
        fe << "左右反転=0\n";
        fe << "上下反転=" << ((layer_item_count % 2 == 1) ? "1" : "0") << "\n";
    } else if (flip_type == FLIP_CW) {
        int r = layer_item_count % 4;
        fe << "左右反転=" << ((r == 1 || r == 2) ? "1" : "0") << "\n";
        fe << "上下反転=" << ((r == 2 || r == 3) ? "1" : "0") << "\n";
    } else if (flip_type == FLIP_CCW) {
        int r = layer_item_count % 4;
        fe << "左右反転=" << ((r == 2 || r == 3) ? "1" : "0") << "\n";
        fe << "上下反転=" << ((r == 1 || r == 2) ? "1" : "0") << "\n";
    }

    fe << "輝度反転=0\n色相反転=0\n透明度反転=0\n";
    return fe.str();
}

static std::string apply_template_to_item(const std::string& template_chain,
                                            const std::vector<ParamBake>& param_bakes,
                                            const std::vector<PresetEntry>& presets) {
    std::string result = template_chain;

    inject_param_bakes(result, param_bakes);
    inject_presets(result, presets);

    return result;
}

static std::unordered_map<std::string, double> build_item_vars(
    size_t item_idx,
    const ItemInterval& interval,
    const ObjDict& objdict,
    const OutputConfig& config,
    const TrackNode& track,
    const SceneInfo& scene,
    int item_count_global,
    int total_items,
    double track_pitch_min,
    double track_pitch_max)
{
    std::unordered_map<std::string, double> vars;
    double fps = (double)config.fps_num / (double)config.fps_den;

    vars["note.pitch"]        = objdict.pitch[item_idx] + 69.0;
    vars["note.velocity"]     = (item_idx < objdict.midi_volume.size() && objdict.midi_volume[item_idx] >= 0)
                                  ? objdict.midi_volume[item_idx] : 100.0;
    vars["note.index"]        = static_cast<double>(item_count_global + 1);
    vars["note.index0"]       = static_cast<double>(item_count_global);
    vars["note.duration"]     = std::round(objdict.length[item_idx] * fps);
    vars["note.duration_sec"] = objdict.length[item_idx];
    vars["note.start_frame"]  = frame_round(interval.obj_fp, config.use_round_up);
    vars["note.end_frame"]    = frame_round(interval.bf, config.use_round_up);
    vars["note.bpm"]          = objdict.bpm;

    double pitch_val = objdict.pitch[item_idx] + 69.0;
    vars["note.pitch_min"] = track_pitch_min;
    vars["note.pitch_max"] = track_pitch_max;
    if (track_pitch_max > track_pitch_min)
        vars["note.pitch_ratio"] = (pitch_val - track_pitch_min) / (track_pitch_max - track_pitch_min);
    else
        vars["note.pitch_ratio"] = 0.5;

    vars["item.position"]  = interval.pos_sec;
    vars["item.length"]    = objdict.length[item_idx];
    vars["item.playrate"]  = objdict.playrate[item_idx];
    vars["item.loop"]      = static_cast<double>(objdict.loop[item_idx]);
    vars["item.soffs"]     = objdict.soffs[item_idx];

    vars["midi.volume"]     = (item_idx < objdict.midi_volume.size())
                                ? objdict.midi_volume[item_idx] : -1.0;
    vars["midi.pan"]        = (item_idx < objdict.midi_pan.size())
                                ? objdict.midi_pan[item_idx] : -1.0;
    vars["midi.pitch_bend"] = (item_idx < objdict.midi_pitch_bend.size())
                                ? objdict.midi_pitch_bend[item_idx] : 0.0;

    vars["track.number"] = static_cast<double>(track.number);

    vars["global.fps"]        = static_cast<double>(config.fps_num) / config.fps_den;
    vars["global.width"]      = static_cast<double>(scene.width);
    vars["global.height"]     = static_cast<double>(scene.height);
    vars["global.item_count"] = static_cast<double>(total_items);

    vars["chord.index"] = static_cast<double>(interval.chord_index);
    vars["chord.count"] = static_cast<double>(interval.chord_count);

    return vars;
}

static std::unordered_map<std::string, std::string> build_item_text_vars(
    size_t item_idx,
    const ObjDict& objdict)
{
    std::unordered_map<std::string, std::string> vars;
    if (item_idx < objdict.midi_lyric.size())
        vars["note.lyric"] = objdict.midi_lyric[item_idx];
    else
        vars["note.lyric"] = "";
    return vars;
}

static std::vector<ParamBake> evaluate_bakes_for_item(
    const std::vector<ParamBake>& source_bakes,
    const std::unordered_map<std::string, double>& num_vars,
    const std::unordered_map<std::string, std::string>& text_vars,
    uint32_t rand_seed,
    std::vector<std::string>& warnings)
{
    ExprEvaluator evaluator;
    evaluator.set_vars(num_vars);
    evaluator.set_seed(rand_seed);

    std::vector<ParamBake> result;
    result.reserve(source_bakes.size());

    for (const auto& pb : source_bakes) {
        if (!pb.active) {
            result.push_back(pb);
            continue;
        }

        ParamBake eval_pb = pb;

        if (pb.value_mode == 0) {
            result.push_back(eval_pb);
            continue;
        }

        std::string substituted = substitute_variables(pb.param_value, num_vars, text_vars);

        if (pb.value_mode == 1) {
            eval_pb.param_value = substituted;
        } else if (pb.value_mode == 2) {
            double val = 0.0;
            std::string err;
            if (!evaluator.evaluate(substituted, val, err)) {
                {
                    std::string msg = "AutoZWJ: 表达式求值失败 [" + pb.param_name + "=\""
                                      + pb.param_value + "\"]: " + err;
                    warnings.push_back(msg);
                }
                eval_pb.active = false;
            } else {
                eval_pb.param_value = std::to_string(val);
                std::string& s = eval_pb.param_value;
                if (s.find('.') != std::string::npos) {
                    while (!s.empty() && s.back() == '0') s.pop_back();
                    if (!s.empty() && s.back() == '.') s.pop_back();
                }
                if (s.empty()) s = "0";
            }
        }

        result.push_back(eval_pb);
    }

    return result;
}

// GCC 15.x 对 vector::resize 扩容路径内联 _Construct 的 -Warray-bounds 误报
// （resize(1) 只缩不扩，不可能越界；note 指向 operator new 分配的 8 字节对象）。
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
static int assign_layer_impl(double obj_fp, double bf, std::vector<double>& target, int strategy) {
    if (strategy == 0) {
        for (size_t k = 0; k < target.size(); k++) {
            if (target[k] >= obj_fp) {
                if (k == target.size() - 1) { target.push_back(bf); return (int)k + 1; }
            } else { target[k] = bf; return (int)k; }
        }
        if (target.empty()) { target.push_back(bf); return 0; }
    } else {
        bool all_free = true;
        for (double b : target) {
            if (b >= obj_fp) { all_free = false; break; }
        }
        if (all_free && !target.empty()) {
            target.resize(1);
            target[0] = bf;
            return 0;
        } else {
            int new_layer = (int)target.size();
            target.push_back(bf);
            return new_layer;
        }
    }
    return 0;
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static int map_sequence_frame(int target_sf, int target_ef,
                              double source_frame, double source_sf,
                              double source_ef, bool use_round_up) {
    double source_span = source_ef - source_sf;
    if (source_span <= 0.0)
        return target_sf;

    double target_span = static_cast<double>(target_ef - target_sf);
    double mapped = static_cast<double>(target_sf)
                  + (source_frame - source_sf) * target_span / source_span;
    return static_cast<int>(frame_round(mapped, use_round_up));
}

static std::vector<size_t> animation_sequence_order(
    std::span<const TemplateSession> templates) {
    std::vector<size_t> order(templates.size());
    for (size_t i = 0; i < templates.size(); i++) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        const auto& lhs = templates[a].source;
        const auto& rhs = templates[b].source;
        if (lhs.sf != rhs.sf) return lhs.sf < rhs.sf;
        if (lhs.ef != rhs.ef) return lhs.ef < rhs.ef;
        return lhs.layer < rhs.layer;
    });
    return order;
}

static int animation_sequence_min_layer(
    std::span<const TemplateSession> templates) {
    int min_layer = templates.front().source.layer;
    for (const auto& tpl : templates)
        min_layer = std::min(min_layer, tpl.source.layer);
    return min_layer;
}

static int animation_sequence_layer_span(
    std::span<const TemplateSession> templates) {
    int min_layer = templates.front().source.layer;
    int max_layer = templates.front().source.layer;
    for (const auto& tpl : templates) {
        min_layer = std::min(min_layer, tpl.source.layer);
        max_layer = std::max(max_layer, tpl.source.layer);
    }
    return max_layer - min_layer + 1;
}

static int assign_sequence_root(double obj_fp, double bf, int layer_span,
                                std::vector<double>& target, int strategy) {
    if (layer_span <= 0) layer_span = 1;

    if (strategy == 0) {
        for (size_t root = 0; root + static_cast<size_t>(layer_span) <= target.size(); root++) {
            bool free = true;
            for (int offset = 0; offset < layer_span; offset++) {
                if (target[root + static_cast<size_t>(offset)] >= obj_fp) {
                    free = false;
                    break;
                }
            }
            if (free) {
                for (int offset = 0; offset < layer_span; offset++)
                    target[root + static_cast<size_t>(offset)] = bf;
                return static_cast<int>(root);
            }
        }

        int root = static_cast<int>(target.size());
        target.resize(target.size() + static_cast<size_t>(layer_span), bf);
        return root;
    }

    bool all_free = true;
    for (double b : target) {
        if (b >= obj_fp) {
            all_free = false;
            break;
        }
    }
    int root = all_free ? 0 : static_cast<int>(target.size());
    if (all_free)
        target.clear();
    target.resize(target.size() + static_cast<size_t>(layer_span), bf);
    return root;
}

static void emit_animation_sequence(
    const ItemInterval& group,
    std::span<const TemplateSession> templates,
    const ObjDict& objdict,
    const OutputConfig& config,
    const TrackNode& track,
    const SceneInfo& scene,
    int item_count_global,
    int total_items,
    double track_pitch_min,
    double track_pitch_max,
    int root_layer,
    bool is_gap_only,
    std::map<int, int>& layer_item_counts,
    std::vector<GeneratedObject>& specs,
    std::vector<std::string>& warnings)
{
    if (templates.empty()) return;

    const auto order = animation_sequence_order(templates);
    const double source_sf = templates[order.front()].source.sf;
    const double source_ef = templates[order.back()].source.ef;
    const int min_layer = animation_sequence_min_layer(templates);

    for (size_t sequence_index = 0; sequence_index < order.size(); sequence_index++) {
        const size_t tpl_idx = order[sequence_index];
        const auto& tpl = templates[tpl_idx];

        ItemInterval child = group;
        if (source_ef <= source_sf) {
            child.sf = group.sf;
            child.ef = group.ef;
        } else {
            child.sf = map_sequence_frame(group.sf, group.ef, tpl.source.sf, source_sf, source_ef, config.use_round_up);
            child.ef = map_sequence_frame(group.sf, group.ef, tpl.source.ef, source_sf, source_ef, config.use_round_up);
        }
        if (sequence_index == 0) child.sf = group.sf;
        if (sequence_index + 1 == order.size()) child.ef = group.ef;
        if (child.ef <= child.sf) child.ef = child.sf + 1;
        child.obj_fp = static_cast<double>(child.sf);
        child.bf = static_cast<double>(child.ef);

        int layer_offset = tpl.source.layer - min_layer;
        int use_layer = root_layer + (config.layer_strategy == 2 ? layer_offset * 2 : layer_offset);
        int layer_count = layer_item_counts[use_layer];
        std::vector<PresetEntry> item_presets;
        if (config.alt_flip && config.flip_type != 0) {
            int flip_count = config.flip_counter_mode == 0 ? item_count_global : layer_count;
            PresetEntry p;
            p.effect_block = build_flip_block(config.flip_type, flip_count);
            p.position = tpl.presets.empty() ? -1 : tpl.presets[0].position;
            p.active = true;
            item_presets.push_back(std::move(p));
        }

        int pitch_int = static_cast<int>(std::round(group.pitch + 69.0));
        uint32_t item_rand_seed = static_cast<uint32_t>(
            group.idx * 100003LL + pitch_int * 10007LL + child.sf * 17LL
            + static_cast<int>(sequence_index) * 7919LL);
        auto num_vars = build_item_vars(group.idx, child, objdict,
                                        config, track, scene, item_count_global,
                                        total_items, track_pitch_min, track_pitch_max);
        auto text_vars = build_item_text_vars(group.idx, objdict);
        auto evaluated_bakes = evaluate_bakes_for_item(
            tpl.bakes, num_vars, text_vars, item_rand_seed, warnings);
        std::string chain = apply_template_to_item(
            tpl.source.chain, evaluated_bakes, item_presets);

        std::ostringstream alias;
        alias << "[0]\nlayer=" << use_layer
              << "\nframe=" << child.sf << "," << child.ef
              << "\noverlay=1\ngroup=1\nclipping=" << config.clipping
              << "\ncamera=" << config.is_ex_set << "\n"
              << chain;

        GeneratedObject go;
        go.layer = use_layer;
        go.sf = child.sf;
        go.ef = child.ef;
        go.alias_chain = alias.str();
        if (!group.file_path.empty()) {
            go.name_kind = ObjNameKind::FilePath;
            go.name_text = group.file_path;
            size_t sep = go.name_text.find_last_of('\\');
            if (sep != std::string::npos) go.name_text = go.name_text.substr(sep + 1);
        } else if (is_gap_only) {
            go.name_kind = ObjNameKind::Gap;
            go.name_index = item_count_global;
        } else {
            go.name_kind = ObjNameKind::Item;
            go.name_index = item_count_global;
            go.name_sub_index = sequence_index == 0
                ? 0 : static_cast<int>(sequence_index) + 1;
        }
        specs.push_back(std::move(go));
        layer_item_counts[use_layer] = layer_count + 1;
    }
}

GenerationResult generate(const GenerationInput& in) {
    std::vector<GeneratedObject> specs;
    std::vector<std::string> warnings;

    std::vector<int> shuffled_order;
    if (in.config.mapping_strategy == 1 && in.config.mapping_sequential_order == 2 && in.templates.size() > 1) {
        shuffled_order = generate_shuffled_order(static_cast<int>(in.templates.size()), in.seed);
    }

    auto& objdict = in.objdict;
    auto& tracks = in.tracks;
    auto& config = in.config;
    int base = in.base_layer;

    double fps = (double)config.fps_num / (double)config.fps_den;
    size_t item_start = 1;
    std::vector<double> opt_layer, opt_layer2;

    int total_items = 0;
    {
        size_t t_start = 1;
        for (size_t t = 0; t < tracks.size(); t++) {
            if (!tracks[t].selected || tracks[t].count <= 0) {
                while (t_start < objdict.pos.size() && objdict.pos[t_start] != -1.0) t_start++;
                if (t_start < objdict.pos.size()) t_start++;
                continue;
            }
            size_t t_end = objdict.pos.size();
            for (size_t j = t_start + 1; j < objdict.pos.size(); j++)
                if (objdict.pos[j] == -1.0) { t_end = j; break; }
            for (size_t k = t_start; k < t_end; k++)
                if (objdict.pos[k] > -0.5) total_items++;
            t_start = t_end + 1;
        }
    }

    int bfidx_global = 0;
    int item_count_global = 0;
    int bpos_global = 0;
    std::map<int, int> layer_item_counts;

    for (size_t ti = 0; ti < tracks.size(); ti++) {
        if (!tracks[ti].selected) {
            while (item_start < objdict.pos.size() && objdict.pos[item_start] != -1.0) item_start++;
            if (item_start < objdict.pos.size()) item_start++;
            continue;
        }
        if (tracks[ti].count <= 0) {
            while (item_start < objdict.pos.size() && objdict.pos[item_start] != -1.0) item_start++;
            if (item_start < objdict.pos.size()) item_start++;
            continue;
        }

        size_t track_end = objdict.pos.size();
        for (size_t j = item_start + 1; j < objdict.pos.size(); j++)
            if (objdict.pos[j] == -1.0) { track_end = j; break; }

        int prev_count = item_count_global;

        size_t seg_start = item_start;
        for (size_t i = item_start; i <= track_end; i++) {
            bool at_end = (i == track_end);
            bool is_sep = (!at_end && objdict.pos[i] < -0.5);

            if (is_sep || at_end) {
                if (seg_start < i) {
                    std::vector<ItemInterval> intervals;

                    for (size_t k = seg_start; k < i; k++) {
                        double pos_sec = objdict.pos[k] + config.base_time_sec;
                        double len_sec = objdict.length[k];
                        double pitch = objdict.pitch[k];

                        double obj_fp = pos_sec * fps + 1.0;
                        double obj_fl = len_sec * fps;
                        double next_fp = (k + 1 < i && objdict.pos[k + 1] > -0.5)
                            ? objdict.pos[k + 1] * fps + 1.0 : -1;

                        if (obj_fp < 0) continue;

                        int sf = (int)frame_round(obj_fp, config.use_round_up);
                        if (sf < 1) sf = 1;

                        // 拉伸到下一音符时，必须跳过同一输出帧内的和弦音符，
                        // 否则同一和弦中只有最后一个音符会真正拉伸。
                        double next_stretch_fp = next_fp;
                        if (config.sync_mode == 1 && next_stretch_fp > 0) {
                            next_stretch_fp = -1;
                            for (size_t next = k + 1; next < i; next++) {
                                if (objdict.pos[next] <= -0.5) break;
                                double candidate_fp =
                                    (objdict.pos[next] + config.base_time_sec) * fps + 1.0;
                                int candidate_sf = (int)frame_round(candidate_fp, config.use_round_up);
                                if (candidate_sf != sf) {
                                    next_stretch_fp = candidate_fp;
                                    break;
                                }
                            }
                        }

                        double bf = obj_fp + obj_fl - 1;

                        bool stretch_next = (config.sync_mode == SYNC_MODE_NEXT);
                        if (stretch_next && next_stretch_fp > 0) {
                            double rounded_bf = frame_round(bf, config.use_round_up);
                            double rounded_next = frame_round(next_stretch_fp, config.use_round_up);
                            if (rounded_bf < rounded_next - 1) bf = next_stretch_fp - 1;
                        }

                        int ef;
                        bool use_fixed = (config.sync_mode == SYNC_MODE_FIXED || config.sync_mode == SYNC_MODE_GAP_FIXED);
                        if (use_fixed) {
                            ef = sf + config.fixed_duration_frames - 1;
                        } else {
                            ef = (int)frame_round(bf, config.use_round_up);
                            if (ef <= sf) ef = sf + 1;
                            // 相邻音符：仅在输出帧域存在恰好 1 帧缝隙时补平（ef = next_sf - 2 时），
                            // 避免向上取整下把亚帧缝误判为 1 帧缝造成重叠
                            if (next_fp > 0 && config.sync_mode != SYNC_MODE_NEXT) {
                                int next_sf = (int)frame_round(next_fp, config.use_round_up);
                                if (next_sf < 1) next_sf = 1;
                                if (next_sf - ef == 2) ef = next_sf - 1;
                            }
                        }

                        bf = (double)ef;

                        std::string file_path;
                        int fileidx = objdict.fileidx[k];
                        if (fileidx >= 0 && fileidx < (int)objdict.filelist.size())
                            file_path = objdict.filelist[fileidx];

                        intervals.push_back({k, pos_sec, obj_fp, bf, pitch, sf, ef,
                            objdict.loop[k], objdict.playrate[k], objdict.soffs[k], fileidx, file_path});
                    }

                    if (config.track_filter_mode == 0) {
                        for (size_t k = 0; k < intervals.size(); ) {
                            size_t j = k;
                            while (j < intervals.size() && intervals[j].sf == intervals[k].sf) j++;
                            int count = static_cast<int>(j - k);
                            int ci = 1;
                            for (size_t m = k; m < j; m++) {
                                intervals[m].chord_index = ci++;
                                intervals[m].chord_count = count;
                            }
                            k = j;
                        }
                    } else {
                        for (auto& iv : intervals) { iv.chord_index = 1; iv.chord_count = 1; }
                    }

                    if (config.sync_mode == SYNC_MODE_STRETCH_HOLD_LAST) {
                        std::vector<ItemInterval> expanded;
                        expanded.reserve(intervals.size() * 2);
                        for (size_t n = 0; n < intervals.size(); n++) {
                            const auto& source = intervals[n];
                            int note_frames = source.ef - source.sf + 1;
                            if (note_frames <= config.fixed_duration_frames) {
                                expanded.push_back(source);
                                continue;
                            }

                            ItemInterval prefix = source;
                            prefix.ef = prefix.sf + config.fixed_duration_frames - 1;
                            prefix.bf = static_cast<double>(prefix.ef);

                            ItemInterval tail = source;
                            tail.sf = prefix.ef + 1;
                            int tail_ef = source.ef;
                            if (n + 1 < intervals.size() && intervals[n + 1].sf > tail.sf) {
                                tail_ef = intervals[n + 1].sf - 1;
                            }
                            expanded.push_back(prefix);
                            if (tail_ef >= tail.sf) {
                                prefix.hold_last_prefix = true;
                                expanded.back().hold_last_prefix = true;
                                tail.obj_fp = static_cast<double>(tail.sf);
                                tail.bf = static_cast<double>(tail_ef);
                                tail.ef = tail_ef;
                                tail.pos_sec = (tail.sf - 1.0) / fps;
                                tail.hold_last_frame = true;
                                expanded.push_back(tail);
                            }
                        }
                        intervals = std::move(expanded);
                    }

                    double track_pitch_min = 128.0, track_pitch_max = -1.0;
                    for (const auto& iv : intervals) {
                        if (iv.pitch > -900.0) {
                            double p = iv.pitch + 69.0;
                            if (p < track_pitch_min) track_pitch_min = p;
                            if (p > track_pitch_max) track_pitch_max = p;
                        }
                    }
                    if (track_pitch_max < 0) { track_pitch_min = 0; track_pitch_max = 0; }

                    bool is_gap_only = (config.sync_mode == SYNC_MODE_GAP || config.sync_mode == SYNC_MODE_GAP_FIXED);
                    if (is_gap_only) {
                        std::vector<ItemInterval> gaps;
                        for (size_t g = 0; g + 1 < intervals.size(); g++) {
                            int gap_sf = intervals[g].ef + 1;
                            int gap_ef = intervals[g + 1].sf - 1;
                            if (gap_ef >= gap_sf) {
                                int gap_ef_final;
                                if (config.sync_mode == SYNC_MODE_GAP_FIXED) {
                                    gap_ef_final = gap_sf + config.fixed_duration_frames - 1;
                                    if (gap_ef_final > gap_ef) gap_ef_final = gap_ef;
                                } else {
                                    gap_ef_final = gap_ef;
                                }
                                gaps.push_back({intervals[g].idx,
                                    (gap_sf - 1.0) / fps,
                                    (double)gap_sf, (double)gap_ef_final, -999.0,
                                    gap_sf, gap_ef_final, 0, 1.0, 0.0, -1, ""});
                            }
                        }
                        intervals = std::move(gaps);
                    }

                    std::vector<int> pre_layers;
                    int max_layer = 0;
                    if (config.track_filter_mode != 0 || config.reverse_layer_order) {
                        auto popt = opt_layer;
                        auto popt2 = opt_layer2;
                        int pcount = item_count_global;
                        int pbfidx = bfidx_global;
                        int pbpos = bpos_global;

                        for (auto& iv : intervals) {
                            if (iv.sf == pbpos) pbfidx--;
                            int par = (pbfidx + pcount) % 2;
                            pcount++;
                            pbpos = iv.sf;

                            auto& ptarget = (config.layer_strategy == 2 && par == 1) ? popt2 : popt;
                            int dl;
                            if (config.mapping_strategy == 4 && in.templates.size() > 1) {
                                dl = assign_sequence_root(
                                    static_cast<double>(iv.sf), static_cast<double>(iv.ef),
                                    animation_sequence_layer_span(in.templates), ptarget,
                                    config.layer_strategy);
                            } else {
                                dl = assign_layer_impl((double)iv.sf, (double)iv.ef, ptarget, config.layer_strategy);
                            }
                            pre_layers.push_back(dl);
                        }
                        if (!pre_layers.empty()) {
                            max_layer = *std::max_element(pre_layers.begin(), pre_layers.end());
                        }
                    }

                    int prev_tpl_idx = -1;
                    int pool_size = (int)in.templates.size();

                    for (size_t iv_idx = 0; iv_idx < intervals.size(); iv_idx++) {
                        auto& iv = intervals[iv_idx];

                        if (iv.sf == bpos_global)
                            bfidx_global--;

                        int par = (bfidx_global + item_count_global) % 2;

                        bpos_global = iv.sf;

                        if (config.mapping_strategy == 4 && pool_size > 1) {
                            auto& target = (config.layer_strategy == 2 && par == 1) ? opt_layer2 : opt_layer;
                            int add_layer = assign_sequence_root(
                                static_cast<double>(iv.sf), static_cast<double>(iv.ef),
                                animation_sequence_layer_span(in.templates), target,
                                config.layer_strategy);

                            if (config.reverse_layer_order)
                                add_layer = max_layer - add_layer;

                            if (config.track_filter_mode != 0) {
                                int dl = pre_layers[iv_idx];
                                if ((config.track_filter_mode == 1 && dl + 1 != config.track_filter_n) ||
                                    (config.track_filter_mode == 2 && dl != max_layer - config.track_filter_n + 1)) {
                                    if (!iv.hold_last_prefix) item_count_global++;
                                    continue;
                                }
                            }

                            int root_layer;
                            if (config.layer_strategy == 2)
                                root_layer = base + (par == 0 ? add_layer * 2 : add_layer * 2 + 1);
                            else
                                root_layer = base + add_layer;

                            emit_animation_sequence(
                                iv, in.templates, objdict, config, tracks[ti], in.scene,
                                item_count_global, total_items, track_pitch_min, track_pitch_max,
                                root_layer, is_gap_only, layer_item_counts, specs, warnings);
                            if (!iv.hold_last_prefix) item_count_global++;
                            continue;
                        }

                        int logical_item_count = item_count_global;
                        int tpl_idx = 0;
                        if (iv.hold_last_frame && prev_tpl_idx >= 0) {
                            tpl_idx = prev_tpl_idx;
                        } else if (pool_size > 1) {
                            tpl_idx = pick_template_index(
                                logical_item_count, iv.chord_index, pool_size,
                                config.mapping_strategy, config, prev_tpl_idx,
                                shuffled_order, in.seed);
                        }

                        std::string tpl_chain = in.templates[tpl_idx].source.chain;
                        const auto& tpl_bakes = in.templates[tpl_idx].bakes;

                        int pitch_int = static_cast<int>(std::round(iv.pitch + 69.0));
                        uint32_t item_rand_seed = static_cast<uint32_t>(iv.idx * 100003LL + pitch_int * 10007LL + iv.sf * 17LL);
                        auto num_vars = build_item_vars(iv.idx, iv, objdict, config, tracks[ti], in.scene,
                                                         logical_item_count, total_items,
                                                         track_pitch_min, track_pitch_max);
                        auto text_vars = build_item_text_vars(iv.idx, objdict);
                        auto evaluated_bakes = evaluate_bakes_for_item(tpl_bakes, num_vars, text_vars, item_rand_seed, warnings);

                        std::vector<PresetEntry> item_presets;
                        int use_layer;

                        if (config.flip_counter_mode == 0) {
                            int flip_count = logical_item_count;
                            if (config.alt_flip && config.flip_type != 0) {
                                PresetEntry p;
                                p.effect_block = build_flip_block(config.flip_type, flip_count);
                                const auto& tpl_presets_ref = in.templates[tpl_idx].presets;
                                if (!tpl_presets_ref.empty()) {
                                    p.position = tpl_presets_ref[0].position;
                                } else {
                                    p.position = -1;
                                }
                                p.active = true;
                                item_presets.push_back(p);
                            }

                            auto& target = (config.layer_strategy == 2 && par == 1) ? opt_layer2 : opt_layer;
                            int add_layer = assign_layer_impl((double)iv.sf, (double)iv.ef, target, config.layer_strategy);

                            if (config.reverse_layer_order) {
                                add_layer = max_layer - add_layer;
                            }

                            if (config.track_filter_mode != 0) {
                                int dl = pre_layers[iv_idx];
                                if (config.track_filter_mode == 1 && dl + 1 != config.track_filter_n) {
                                    if (!iv.hold_last_prefix) item_count_global++;
                                    continue;
                                }
                                if (config.track_filter_mode == 2 && dl != max_layer - config.track_filter_n + 1) {
                                    if (!iv.hold_last_prefix) item_count_global++;
                                    continue;
                                }
                            }

                            if (config.layer_strategy == 2)
                                use_layer = base + (par == 0 ? add_layer * 2 : add_layer * 2 + 1);
                            else
                                use_layer = base + add_layer;
                        } else {
                            auto& target = (config.layer_strategy == 2 && par == 1) ? opt_layer2 : opt_layer;
                            int add_layer = assign_layer_impl((double)iv.sf, (double)iv.ef, target, config.layer_strategy);

                            if (config.reverse_layer_order) {
                                add_layer = max_layer - add_layer;
                            }

                            if (config.track_filter_mode != 0) {
                                int dl = pre_layers[iv_idx];
                                if (config.track_filter_mode == 1 && dl + 1 != config.track_filter_n) {
                                    if (!iv.hold_last_prefix) item_count_global++;
                                    continue;
                                }
                                if (config.track_filter_mode == 2 && dl != max_layer - config.track_filter_n + 1) {
                                    if (!iv.hold_last_prefix) item_count_global++;
                                    continue;
                                }
                            }

                            if (config.layer_strategy == 2)
                                use_layer = base + (par == 0 ? add_layer * 2 : add_layer * 2 + 1);
                            else
                                use_layer = base + add_layer;

                            int layer_count = layer_item_counts[use_layer];
                            if (config.alt_flip && config.flip_type != 0) {
                                PresetEntry p;
                                int flip_count = iv.hold_last_frame ? std::max(0, layer_count - 1) : layer_count;
                                p.effect_block = build_flip_block(config.flip_type, flip_count);
                                const auto& tpl_presets_ref = in.templates[tpl_idx].presets;
                                if (!tpl_presets_ref.empty()) {
                                    p.position = tpl_presets_ref[0].position;
                                } else {
                                    p.position = -1;
                                }
                                p.active = true;
                                item_presets.push_back(p);
                            }
                            if (!iv.hold_last_frame) {
                                layer_item_counts[use_layer] = layer_count + 1;
                            }
                        }

                        std::string chain = apply_template_to_item(
                            tpl_chain, evaluated_bakes, item_presets);
                        if (iv.hold_last_frame) {
                            hold_last_motion_values(chain);
                        }

                        prev_tpl_idx = tpl_idx;

                        std::ostringstream alias;
                        alias << "[0]\nlayer=" << use_layer
                              << "\nframe=" << iv.sf << "," << iv.ef
                              << "\noverlay=1\ngroup=1\nclipping=" << config.clipping
                              << "\ncamera=" << config.is_ex_set << "\n"
                              << chain;

                        GeneratedObject go;
                        go.layer = use_layer;
                        go.sf = iv.sf;
                        go.ef = iv.ef;
                        go.alias_chain = alias.str();
                        if (!iv.hold_last_frame && !iv.file_path.empty()) {
                            go.name_kind = ObjNameKind::FilePath;
                            go.name_text = iv.file_path;
                            size_t sep = go.name_text.find_last_of('\\');
                            if (sep != std::string::npos) go.name_text = go.name_text.substr(sep + 1);
                        } else if (is_gap_only) {
                            go.name_kind = ObjNameKind::Gap;
                            go.name_index = item_count_global;
                        } else {
                            go.name_kind = ObjNameKind::Item;
                            go.name_index = logical_item_count;
                            go.name_sub_index = iv.hold_last_frame ? 2 : 0;
                        }
                        specs.push_back(go);
                        if (!iv.hold_last_prefix) item_count_global++;
                    }
                }

                if (is_sep) {
                    bfidx_global = -item_count_global;
                    base += (int)(opt_layer.size() + opt_layer2.size());
                    opt_layer.clear();
                    opt_layer2.clear();
                    seg_start = i + 1;
                }
                continue;
            }
        }

        if (item_count_global > prev_count)
            item_start = track_end + 1;
        else
            item_start = track_end;
    }

    return { specs, warnings };
}
