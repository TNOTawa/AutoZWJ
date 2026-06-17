#include "plugin.h"
#include "rpp/rpp_parser.h"
#include "midi/midi_parser.h"
#include "exo/object_generator.h"
#include "effect/effect_dict.h"
#include "ui/file_picker.h"
#include "ui/imgui_window.h"
#include "ui/effect_chain_editor.h"
#include "script/expr_evaluator.h"
#include "script/variable_subst.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <cstdint>

ProjectState g_project_state;
SceneInfo g_scene_info;
EDIT_HANDLE* g_edit_handle = nullptr;
LOG_HANDLE* g_logger = nullptr;
HINSTANCE g_dll_hinst = nullptr;
std::vector<TemplateEntry> g_template_pool;

static uint32_t hash_string(const std::string& s) {
    uint32_t h = 5381;
    for (char c : s) {
        h = ((h << 5) + h) + static_cast<unsigned char>(c);
    }
    return h;
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

static int pick_template_index(int item_count_global, int chord_index, int pool_size,
                                int strategy, const OutputConfig& config, int prev_tpl_idx,
                                const std::vector<int>& shuffled_order, uint32_t seed_base) {
    if (pool_size <= 1) return 0;

    switch (strategy) {
    case 1: { // 顺序轮替
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
    case 2: { // 随机抽选
        uint32_t seed = seed_base ^ static_cast<uint32_t>(item_count_global * 2654435761u);
        seed = seed * 1103515245u + 12345u;
        int idx = static_cast<int>(seed % static_cast<uint32_t>(pool_size));
        if (config.mapping_no_consecutive && idx == prev_tpl_idx && pool_size > 1) {
            idx = (idx + 1) % pool_size;
        }
        return idx;
    }
    case 3: { // 和弦映射
        int idx = (chord_index - 1) % pool_size;
        return idx;
    }
    default:
        return 0;
    }
}

std::wstring utf8_to_wide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring result(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], len);
    return result;
}

std::string wide_to_utf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &result[0], len, nullptr, nullptr);
    return result;
}

std::string cp932_to_utf8(const std::string& cp932) {
    if (cp932.empty()) return "";
    int wide_len = MultiByteToWideChar(932, 0, cp932.c_str(), -1, nullptr, 0);
    if (wide_len <= 0) return cp932;
    std::wstring wide(wide_len - 1, L'\0');
    MultiByteToWideChar(932, 0, cp932.c_str(), -1, &wide[0], wide_len);
    return wide_to_utf8(wide);
}

static bool is_valid_utf8(const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = s[i];
        if (c < 0x80) { i++; continue; }
        size_t n = 0;
        if ((c & 0xE0) == 0xC0) n = 2;
        else if ((c & 0xF0) == 0xE0) n = 3;
        else if ((c & 0xF8) == 0xF0) n = 4;
        else return false;
        if (i + n > s.size()) return false;
        for (size_t j = 1; j < n; j++) {
            if ((s[i+j] & 0xC0) != 0x80) return false;
        }
        i += n;
    }
    return true;
}

std::string maybe_cp932_to_utf8(const std::string& s) {
    if (s.empty()) return s;
    if (is_valid_utf8(s)) return s;
    return cp932_to_utf8(s);
}

COMMON_PLUGIN_TABLE common_plugin_table = {
    L"AutoZWJ",
    L"AutoZWJ - RPP/MIDI to AviUtl2 object importer",
};

EXTERN_C __declspec(dllexport) COMMON_PLUGIN_TABLE* GetCommonPluginTable(void) {
    return &common_plugin_table;
}

EXTERN_C __declspec(dllexport) DWORD RequiredVersion() {
    return 2003300;
}

EXTERN_C __declspec(dllexport) void InitializeLogger(LOG_HANDLE* handle) {
    g_logger = handle;
}

EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD version) {
    return true;
}

EXTERN_C __declspec(dllexport) void UninitializePlugin() {
    imgui_window_shutdown();
}

static void update_scene_from_edit(EDIT_SECTION* edit) {
    if (!edit || !edit->info) return;
    auto& info = *edit->info;
    g_scene_info.width = info.width;
    g_scene_info.height = info.height;
    g_scene_info.rate = info.rate;
    g_scene_info.scale = info.scale;
    g_scene_info.sample_rate = info.sample_rate;
    g_scene_info.frame = info.frame;
    g_scene_info.layer = info.layer;
    g_scene_info.valid = true;
    g_project_state.config.fps_num = info.rate;
    g_project_state.config.fps_den = info.scale;
}

std::string extract_template_chain(const std::string& alias) {
    size_t pos = alias.find("\n[Object.0]");
    if (pos != std::string::npos) { pos++; }
    else {
        pos = alias.find("\n[0.0]");
        if (pos != std::string::npos) { pos++; }
        else if (alias.find("[Object.0]") == 0) { pos = 0; }
        else if (alias.find("[0.0]") == 0) { pos = 0; }
        else { return ""; }
    }

    std::string chain = alias.substr(pos);

    size_t rp = 0;
    while ((rp = chain.find("[Object.", rp)) != std::string::npos) {
        chain.replace(rp, 8, "[0.");
        rp += 3;
    }

    return chain;
}

std::vector<ParsedEffect> parse_effect_chain(const std::string& chain) {
    std::vector<ParsedEffect> result;
    size_t pos = 0;
    int index = 0;

    while (pos < chain.size()) {
        size_t sec_marker = chain.find("[0.", pos);
        if (sec_marker == std::string::npos) break;

        size_t bracket_end = chain.find(']', sec_marker);
        if (bracket_end == std::string::npos) break;

        size_t body_start = bracket_end + 1;
        if (body_start < chain.size() && chain[body_start] == '\n')
            body_start++;

        size_t next_marker = chain.find("[0.", body_start);

        ParsedEffect eff;
        eff.original_index = index++;

        size_t line_start = body_start;
        bool has_effect_name = false;

        while (line_start < chain.size() && (next_marker == std::string::npos || line_start < next_marker)) {
            size_t line_end = chain.find('\n', line_start);
            if (line_end == std::string::npos) line_end = chain.size();

            std::string line = chain.substr(line_start, line_end - line_start);
            if (!line.empty() && line.back() == '\r') line.pop_back();

            if (!line.empty()) {
                if (line.find("effect.name=") == 0) {
                    eff.effect_name = line.substr(12);
                    has_effect_name = true;
                } else {
                    size_t eq = line.find('=');
                    if (eq != std::string::npos) {
                        std::string pname = line.substr(0, eq);
                        std::string pval = line.substr(eq + 1);
                        eff.params.push_back({pname, pval});
                    }
                }
            }
            line_start = line_end + 1;
        }

        if (has_effect_name) {
            const EffectDef* def = find_effect(eff.effect_name);
            if (def) eff.effect_name_zh = def->name_zh;
            result.push_back(std::move(eff));
        }

        pos = (next_marker == std::string::npos) ? chain.size() : next_marker;
    }

    return result;
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
        // 参数不存在：追加简化版（无运动格式）
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

// 生成循环中使用的 interval 结构（定义在 lambda 外以便 build_item_vars 可见）
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
};

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
    vars["note.start_frame"]  = std::round(interval.pos_sec * fps) + 1.0;
    vars["note.end_frame"]    = std::round((interval.pos_sec + objdict.length[item_idx]) * fps);
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
    LOG_HANDLE* logger)
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
                if (logger) {
                    std::string msg = "AutoZWJ: 表达式求值失败 [" + pb.param_name + "=\""
                                      + pb.param_value + "\"]: " + err;
                    logger->warn(logger, utf8_to_wide(msg).c_str());
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

static void on_generate_from_imgui() {
    if (!g_project_state.has_data) {
        if (g_logger) g_logger->warn(g_logger, L"AutoZWJ: 未加载音频工程");
        return;
    }
    if (g_template_pool.empty()) {
        if (g_logger) g_logger->error(g_logger, L"AutoZWJ: 未设置模板，请右键已选物件 → 配置导入... 重新打开");
        return;
    }

    struct GenState {
        ObjDict* objdict;
        std::vector<TrackNode>* tracks;
        OutputConfig* config;
        std::vector<TemplateEntry> template_pool;
        std::vector<std::vector<ParamBake>>* bakes_per_tpl;
        std::vector<std::vector<PresetEntry>>* presets_per_tpl;
        LOG_HANDLE* logger;
        int created;
        int base_layer;
        uint32_t seed_base;
        std::vector<int> shuffled_order;
    };

    // 保存当前效果链编辑器数据
    save_current_template_data();

    uint32_t seed_base = 0;
    if (!g_project_state.file_path.empty()) {
        seed_base = hash_string(wide_to_utf8(g_project_state.file_path));
    }

    // 生成洗牌顺序（仅当需要时）
    std::vector<int> shuffled_order;
    if (g_project_state.config.mapping_strategy == 1 &&
        g_project_state.config.mapping_sequential_order == 2 &&
        g_template_pool.size() > 1) {
        shuffled_order = generate_shuffled_order(static_cast<int>(g_template_pool.size()), seed_base);
    }

    auto* state = new GenState{
        &g_project_state.objdict, &g_project_state.tracks,
        &g_project_state.config, g_template_pool,
        &g_param_bakes_per_tpl, &g_template_presets_per_tpl,
        g_logger, 0, g_project_state.template_layer + 1,
        seed_base, shuffled_order
    };

    g_edit_handle->call_edit_section_param(state, [](void* param, EDIT_SECTION* edit) {
        auto* gs = static_cast<GenState*>(param);
        update_scene_from_edit(edit);

        auto& objdict = *gs->objdict;
        auto& tracks = *gs->tracks;
        auto& config = *gs->config;
        int base = gs->base_layer;

        double fps = (double)config.fps_num / (double)config.fps_den;
        size_t item_start = 1;
        std::vector<double> opt_layer, opt_layer2;

        // 预计算总 item 数（近似，用于 $global.item_count$）
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

        auto sur_round = [&](double v) -> double {
            return config.use_round_up ? std::ceil(v) : std::round(v);
        };

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

            // === segment-based 处理：在每个 separator/track 边界内收集并生成 ===
            size_t seg_start = item_start;
            for (size_t i = item_start; i <= track_end; i++) {
                bool at_end = (i == track_end);
                bool is_sep = (!at_end && objdict.pos[i] < -0.5);

                if (is_sep || at_end) {
                    if (seg_start < i) {
                        // --- 1. 收集 intervals ---
                        std::vector<ItemInterval> intervals;

                        for (size_t k = seg_start; k < i; k++) {
                            double pos_sec = objdict.pos[k] + config.base_time_sec;
                            double len_sec = objdict.length[k];
                            double pitch = objdict.pitch[k];

                            double obj_fp = pos_sec * fps + 1.0;
                            double obj_fl = len_sec * fps;
                            double next_fp = (k + 1 < i && objdict.pos[k + 1] > -0.5)
                                ? objdict.pos[k + 1] * fps + 1.0 : -1;

                            if (std::round(obj_fp + obj_fl) == std::round(next_fp) - 1)
                                obj_fl += 1;

                            if (obj_fp < 0) continue;

                            double bf = obj_fp + obj_fl - 1;

                            bool stretch_next = (config.sync_mode == 1);
                            if (stretch_next && next_fp > 0) {
                                double rounded_bf = sur_round(bf);
                                double rounded_next = sur_round(next_fp);
                                if (rounded_bf < rounded_next - 1) bf = next_fp - 1;
                            }

                            int sf = (int)sur_round(obj_fp);
                            if (sf < 1) sf = 1;
                            int ef;
                            bool use_fixed = (config.sync_mode == 2 || config.sync_mode == 4);
                            if (use_fixed) {
                                ef = sf + config.fixed_duration_frames - 1;
                            } else {
                                ef = (int)sur_round(bf);
                                if (ef <= sf) ef = sf + 1;
                            }

                            // 同步 bf 为实际整数结束帧，用于层分配时避免浮点误差导致重叠误判
                            bf = (double)ef;

                            std::string file_path;
                            int fileidx = objdict.fileidx[k];
                            if (fileidx >= 0 && fileidx < (int)objdict.filelist.size())
                                file_path = objdict.filelist[fileidx];

                            intervals.push_back({k, pos_sec, obj_fp, bf, pitch, sf, ef,
                                objdict.loop[k], objdict.playrate[k], objdict.soffs[k], fileidx, file_path});
                        }

                        // 计算和弦索引（同一 sf 的连续 items 视为和弦）
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

                        // 计算轨道音高极值（用于 $note.pitch_ratio$ 等）
                        double track_pitch_min = 128.0, track_pitch_max = -1.0;
                        for (const auto& iv : intervals) {
                            if (iv.pitch > -900.0) {
                                double p = iv.pitch + 69.0;
                                if (p < track_pitch_min) track_pitch_min = p;
                                if (p > track_pitch_max) track_pitch_max = p;
                            }
                        }
                        if (track_pitch_max < 0) { track_pitch_min = 0; track_pitch_max = 0; }

                        bool is_gap_only = (config.sync_mode == 3 || config.sync_mode == 4);
                        // --- 2. 仅在间隙生成模式：转换为间隙 intervals ---
                        if (is_gap_only) {
                            std::vector<ItemInterval> gaps;
                            for (size_t g = 0; g + 1 < intervals.size(); g++) {
                                int gap_sf = intervals[g].ef + 1;
                                int gap_ef = intervals[g + 1].sf - 1;
                                if (gap_ef >= gap_sf) {
                                    double gap_fp = gap_sf;
                                    int gap_ef_final;
                                    if (config.sync_mode == 4) {
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

                        // --- 3. 预计算层分配（用于过滤与反转） ---
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
                                int dl = assign_layer_impl((double)iv.sf, (double)iv.ef, ptarget, config.layer_strategy);
                                pre_layers.push_back(dl);
                            }
                            if (!pre_layers.empty()) {
                                max_layer = *std::max_element(pre_layers.begin(), pre_layers.end());
                            }
                        }

                        // --- 4. 实际生成 ---
                        int prev_tpl_idx = -1;
                        int pool_size = (int)gs->template_pool.size();

                        for (size_t iv_idx = 0; iv_idx < intervals.size(); iv_idx++) {
                            auto& iv = intervals[iv_idx];

                            if (iv.sf == bpos_global)
                                bfidx_global--;

                            int par = (bfidx_global + item_count_global) % 2;

                            bpos_global = iv.sf;

                            // 选择模板
                            int tpl_idx = 0;
                            if (pool_size > 1) {
                                tpl_idx = pick_template_index(
                                    item_count_global, iv.chord_index, pool_size,
                                    config.mapping_strategy, config, prev_tpl_idx,
                                    gs->shuffled_order, gs->seed_base);
                            }

                            // 获取模板链与每模板数据
                            std::string tpl_chain = gs->template_pool[tpl_idx].chain;
                            const auto& tpl_bakes = (*gs->bakes_per_tpl)[tpl_idx];

                            // 变量求值
                            int pitch_int = static_cast<int>(std::round(iv.pitch + 69.0));
                            uint32_t item_rand_seed = static_cast<uint32_t>(iv.idx * 100003LL + pitch_int * 10007LL + iv.sf * 17LL);
                            auto num_vars = build_item_vars(iv.idx, iv, objdict, config, tracks[ti], g_scene_info,
                                                             item_count_global, total_items,
                                                             track_pitch_min, track_pitch_max);
                            auto text_vars = build_item_text_vars(iv.idx, objdict);
                            auto evaluated_bakes = evaluate_bakes_for_item(tpl_bakes, num_vars, text_vars, item_rand_seed, gs->logger);

                            // 翻转判定与图层分配
                            std::vector<PresetEntry> item_presets;
                            int use_layer;

                            if (config.flip_counter_mode == 0) {
                                // 全部模式: 先翻转
                                int flip_count = item_count_global;
                                if (config.alt_flip && config.flip_type != 0) {
                                    PresetEntry p;
                                    p.effect_block = build_flip_block(config.flip_type, flip_count);
                                    const auto& tpl_presets_ref = (*gs->presets_per_tpl)[tpl_idx];
                                    if (!tpl_presets_ref.empty()) {
                                        p.position = tpl_presets_ref[0].position;
                                    } else {
                                        p.position = -1;
                                    }
                                    p.active = true;
                                    item_presets.push_back(p);
                                }

                                // 再图层分配
                                auto& target = (config.layer_strategy == 2 && par == 1) ? opt_layer2 : opt_layer;
                                int add_layer = assign_layer_impl((double)iv.sf, (double)iv.ef, target, config.layer_strategy);

                                if (config.reverse_layer_order) {
                                    add_layer = max_layer - add_layer;
                                }

                                if (config.track_filter_mode != 0) {
                                    int dl = pre_layers[iv_idx];
                                    if (config.track_filter_mode == 1 && dl + 1 != config.track_filter_n) {
                                        item_count_global++;
                                        continue;
                                    }
                                    if (config.track_filter_mode == 2 && dl != max_layer - config.track_filter_n + 1) {
                                        item_count_global++;
                                        continue;
                                    }
                                }

                                if (config.layer_strategy == 2)
                                    use_layer = base + (par == 0 ? add_layer * 2 : add_layer * 2 + 1);
                                else
                                    use_layer = base + add_layer;
                            } else {
                                // 图层模式: 先图层分配
                                auto& target = (config.layer_strategy == 2 && par == 1) ? opt_layer2 : opt_layer;
                                int add_layer = assign_layer_impl((double)iv.sf, (double)iv.ef, target, config.layer_strategy);

                                if (config.reverse_layer_order) {
                                    add_layer = max_layer - add_layer;
                                }

                                if (config.track_filter_mode != 0) {
                                    int dl = pre_layers[iv_idx];
                                    if (config.track_filter_mode == 1 && dl + 1 != config.track_filter_n) {
                                        item_count_global++;
                                        continue;
                                    }
                                    if (config.track_filter_mode == 2 && dl != max_layer - config.track_filter_n + 1) {
                                        item_count_global++;
                                        continue;
                                    }
                                }

                                if (config.layer_strategy == 2)
                                    use_layer = base + (par == 0 ? add_layer * 2 : add_layer * 2 + 1);
                                else
                                    use_layer = base + add_layer;

                                // 再翻转
                                int layer_count = layer_item_counts[use_layer];
                                if (config.alt_flip && config.flip_type != 0) {
                                    PresetEntry p;
                                    p.effect_block = build_flip_block(config.flip_type, layer_count);
                                    const auto& tpl_presets_ref = (*gs->presets_per_tpl)[tpl_idx];
                                    if (!tpl_presets_ref.empty()) {
                                        p.position = tpl_presets_ref[0].position;
                                    } else {
                                        p.position = -1;
                                    }
                                    p.active = true;
                                    item_presets.push_back(p);
                                }
                                layer_item_counts[use_layer] = layer_count + 1;
                            }

                            std::string chain = apply_template_to_item(
                                tpl_chain, evaluated_bakes, item_presets);

                            prev_tpl_idx = tpl_idx;

                            std::ostringstream alias;
                            alias << "[0]\nlayer=" << use_layer
                                  << "\nframe=" << iv.sf << "," << iv.ef
                                  << "\noverlay=1\ngroup=1\nclipping=" << config.clipping
                                  << "\ncamera=" << config.is_ex_set << "\n"
                                  << chain;

                            auto created_obj = edit->create_object_from_alias(alias.str().c_str(), use_layer, iv.sf, iv.ef - iv.sf);
                            if (created_obj) {
                                std::wstring obj_name;
                                if (!iv.file_path.empty()) {
                                    obj_name = utf8_to_wide(iv.file_path);
                                    size_t sep = obj_name.find_last_of(L'\\');
                                    if (sep != std::wstring::npos) obj_name = obj_name.substr(sep + 1);
                                } else if (is_gap_only) {
                                    obj_name = L"Gap " + std::to_wstring(item_count_global);
                                } else {
                                    obj_name = L"Item " + std::to_wstring(item_count_global);
                                }
                                edit->set_object_name(created_obj, obj_name.c_str());
                            }
                            gs->created++;
                            item_count_global++;
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

        if (gs->logger)
            gs->logger->log(gs->logger, (L"AutoZWJ: 已生成 " + std::to_wstring(gs->created) + L" 个物件").c_str());
        delete gs;
    });
}

static void on_select_project(EDIT_SECTION* edit) {
    update_scene_from_edit(edit);
    load_project_state_from_project_file(edit);
    flush_project_file_state(edit);
    imgui_window_show_import_page();
}

static void on_open_config(EDIT_SECTION* edit) {
    update_scene_from_edit(edit);
    load_project_state_from_project_file(edit);

    if (!g_project_state.has_data) {
        std::wstring path_to_load;
        if (!g_project_state.file_path.empty()) {
            path_to_load = g_project_state.file_path;
        } else if (!g_project_state.file_history.empty()) {
            path_to_load = g_project_state.file_history[0].path;
        }
        if (!path_to_load.empty()) {
            parse_project_file(path_to_load);
        }
    }

    flush_project_file_state(edit);

    int sel_num = edit->get_selected_object_num();
    if (sel_num <= 0) {
        if (g_logger) g_logger->error(g_logger, L"AutoZWJ: 请先在时间轴上选择一个物件作为模板，再右键打开配置");
        return;
    }

    // 构建模板池
    g_template_pool.clear();
    g_template_aliases.clear();

    for (int i = 0; i < sel_num; i++) {
        auto obj = edit->get_selected_object(i);
        if (!obj) continue;
        LPCSTR alias_ptr = edit->get_object_alias(obj);
        if (!alias_ptr) continue;

        TemplateEntry entry;
        entry.object_id = i;
        entry.alias = alias_ptr;
        entry.chain = extract_template_chain(entry.alias);
        if (entry.chain.empty()) continue;

        auto lf = edit->get_object_layer_frame(obj);
        entry.layer = lf.layer;
        entry.sf = static_cast<double>(lf.start);

        LPCWSTR name_ptr = edit->get_object_name(obj);
        if (name_ptr && name_ptr[0]) {
            entry.display_name = wide_to_utf8(name_ptr);
        } else {
            entry.display_name = u8"模板" + std::to_string(i + 1);
        }

        g_template_pool.push_back(entry);
        g_template_aliases.push_back(entry.alias);
    }

    if (g_template_pool.empty()) {
        if (g_logger) g_logger->error(g_logger, L"AutoZWJ: 无法读取任何选中物件的数据");
        return;
    }

    // 按 (layer, frame) 排序
    std::sort(g_template_pool.begin(), g_template_pool.end(), [](const TemplateEntry& a, const TemplateEntry& b) {
        if (a.layer != b.layer) return a.layer < b.layer;
        return a.sf < b.sf;
    });

    // 排序后重新同步 g_template_aliases
    g_template_aliases.clear();
    for (auto& tpl : g_template_pool) {
        g_template_aliases.push_back(tpl.alias);
    }

    g_project_state.template_alias = g_template_pool[0].alias;
    g_project_state.template_layer = g_template_pool[0].layer;
    g_current_template_idx = 0;

    // 重置每模板数据
    int pool_size = static_cast<int>(g_template_pool.size());
    g_template_effects_per_tpl.assign(pool_size, {});
    g_param_bakes_per_tpl.assign(pool_size, {});
    g_template_presets_per_tpl.assign(pool_size, {});

    // 重置当前活跃数据
    g_param_bakes.clear();
    g_presets.clear();
    g_template_effects.clear();
    g_template_effects_dirty = true;
    refresh_template_effects();
    sync_presets_from_config();

    if (!g_project_state.has_data) {
        imgui_window_show_import_page();
        return;
    }
    imgui_window_show();
}

static void on_file_drop(EDIT_SECTION* edit, LPCWSTR file) {
    update_scene_from_edit(edit);
    load_project_state_from_project_file(edit);
    if (parse_project_file(file)) {
        flush_project_file_state(edit);
        if (g_logger)
            g_logger->log(g_logger, (L"AutoZWJ: " + get_project_summary()).c_str());
    }
}

void sync_scene_info() {
    if (!g_edit_handle) return;
    EDIT_INFO info = {};
    g_edit_handle->get_edit_info(&info, sizeof(info));
    g_scene_info.width = info.width;
    g_scene_info.height = info.height;
    g_scene_info.rate = info.rate;
    g_scene_info.scale = info.scale;
    g_scene_info.sample_rate = info.sample_rate;
    g_scene_info.frame = info.frame;
    g_scene_info.layer = info.layer;
    g_scene_info.valid = true;
    g_project_state.config.fps_num = info.rate;
    g_project_state.config.fps_den = info.scale;
}

EXTERN_C __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host) {
    g_dll_hinst = GetModuleHandle(nullptr);

    host->register_layer_menu(L"[AutoZWJ] 选择音频工程...", on_select_project);
    host->register_object_menu(L"[AutoZWJ] 配置导入...", on_open_config);
    host->register_file_drop_handler(L"[AutoZWJ] RPP/MIDI Input", L"*.rpp;*.mid", on_file_drop);

    g_edit_handle = host->create_edit_handle();

    HWND host_wnd = g_edit_handle->get_host_app_window();
    imgui_window_init(g_dll_hinst, host_wnd);
    imgui_window_set_generate_callback(on_generate_from_imgui);

    if (g_logger) g_logger->log(g_logger, L"AutoZWJ plugin registered");
}
