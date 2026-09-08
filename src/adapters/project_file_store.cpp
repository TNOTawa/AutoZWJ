#include "project_file_store.h"
#include "application/project_controller.h"
#include "codec/codec.h"
#include <string>
#include <algorithm>

struct SaveCfgCtx {
    const OutputConfig* cfg;
    EDIT_HANDLE* eh;
    const std::wstring* last_dir;
};

static void save_config_callback(void* param, EDIT_SECTION* edit) {
    auto* ctx = static_cast<SaveCfgCtx*>(param);
    auto* pf = edit->get_project_file(ctx->eh);
    if (!pf) return;
    auto* cfg = ctx->cfg;
    pf->set_param_string("rppinexo.fps_num", std::to_string(cfg->fps_num).c_str());
    pf->set_param_string("rppinexo.fps_den", std::to_string(cfg->fps_den).c_str());
    pf->set_param_string("rppinexo.alt_flip", cfg->alt_flip ? "1" : "0");
    pf->set_param_string("rppinexo.flip_type", std::to_string(cfg->flip_type).c_str());
    pf->set_param_string("rppinexo.flip_counter_mode", std::to_string(cfg->flip_counter_mode).c_str());
    pf->set_param_string("rppinexo.clipping", std::to_string(cfg->clipping).c_str());
    pf->set_param_string("rppinexo.is_ex_set", std::to_string(cfg->is_ex_set).c_str());
    pf->set_param_string("rppinexo.sync_mode", std::to_string(cfg->sync_mode).c_str());
    pf->set_param_string("rppinexo.fixed_duration_frames", std::to_string(cfg->fixed_duration_frames).c_str());
    pf->set_param_string("rppinexo.stretch_hold_last_to_next",
        cfg->stretch_hold_last_to_next ? "1" : "0");
    pf->set_param_string("rppinexo.mapping_strategy", std::to_string(cfg->mapping_strategy).c_str());
    pf->set_param_string("rppinexo.mapping_sequential_order", std::to_string(cfg->mapping_sequential_order).c_str());
    pf->set_param_string("rppinexo.mapping_no_consecutive", cfg->mapping_no_consecutive ? "1" : "0");
    pf->set_param_string("rppinexo.animation_sequence_allow_stretch",
        cfg->animation_sequence_allow_stretch ? "1" : "0");
    pf->set_param_string("rppinexo.last_directory", wide_to_utf8(*ctx->last_dir).c_str());
}

void save_config_to_project_file(const OutputConfig& cfg, const AppState& app, const HostContext& host) {
    SaveCfgCtx ctx = { &cfg, host.edit_handle, &app.project.last_directory };
    host.edit_handle->call_edit_section_param(&ctx, save_config_callback);
}

void load_project_state_from_project_file(EDIT_SECTION* edit, AppState& app, const HostContext& host) {
    if (!edit) return;
    auto* pf = edit->get_project_file(host.edit_handle);
    if (!pf) return;

    g_project_state_dirty = false;
    app.project = ProjectState{};

    OutputConfig& cfg = app.project.config;
    auto     val = pf->get_param_string("rppinexo.fps_num");
    if (val) cfg.fps_num = std::atoi(val);
    val = pf->get_param_string("rppinexo.fps_den");
    if (val) cfg.fps_den = std::atoi(val);
    val = pf->get_param_string("rppinexo.alt_flip");
    if (val) cfg.alt_flip = (std::atoi(val) != 0);
    val = pf->get_param_string("rppinexo.flip_type");
    if (val) cfg.flip_type = std::atoi(val);
    val = pf->get_param_string("rppinexo.flip_counter_mode");
    if (val) cfg.flip_counter_mode = std::atoi(val);
    val = pf->get_param_string("rppinexo.clipping");
    if (val) cfg.clipping = std::atoi(val);
    val = pf->get_param_string("rppinexo.is_ex_set");
    if (val) cfg.is_ex_set = std::atoi(val);
    val = pf->get_param_string("rppinexo.sync_mode");
    if (val) cfg.sync_mode = std::clamp(std::atoi(val), SYNC_MODE_NOTE, SYNC_MODE_STRETCH_HOLD_LAST);
    val = pf->get_param_string("rppinexo.fixed_duration_frames");
    if (val) cfg.fixed_duration_frames = std::max(1, std::atoi(val));
    val = pf->get_param_string("rppinexo.stretch_hold_last_to_next");
    if (val) cfg.stretch_hold_last_to_next = (std::atoi(val) != 0);
    val = pf->get_param_string("rppinexo.mapping_strategy");
    if (val) cfg.mapping_strategy = std::clamp(
        std::atoi(val), MAPPING_STRATEGY_SEQUENTIAL, MAPPING_STRATEGY_ANIMATION_SEQUENCE);
    val = pf->get_param_string("rppinexo.mapping_sequential_order");
    if (val) cfg.mapping_sequential_order = std::atoi(val);
    val = pf->get_param_string("rppinexo.mapping_no_consecutive");
    if (val) cfg.mapping_no_consecutive = (std::atoi(val) != 0);
    val = pf->get_param_string("rppinexo.animation_sequence_allow_stretch");
    if (val) cfg.animation_sequence_allow_stretch = (std::atoi(val) != 0);
    val = pf->get_param_string("rppinexo.last_directory");
    if (val) app.project.last_directory = utf8_to_wide(val);

    val = pf->get_param_string("rppinexo.history_count");
    int count = val ? std::atoi(val) : 0;
    if (count < 0) count = 0;
    if (count > 10) count = 10;
    app.project.file_history.clear();
    for (int i = 0; i < count; i++) {
        std::string key = "rppinexo.history_" + std::to_string(i);
        val = pf->get_param_string(key.c_str());
        if (val && val[0]) {
            FileHistoryEntry entry;
            entry.path = utf8_to_wide(val);
            std::string offset_key = "rppinexo.history_offset_" + std::to_string(i);
            auto oval = pf->get_param_string(offset_key.c_str());
            if (oval && oval[0]) {
                entry.base_time_sec = std::atof(oval);
            }
            app.project.file_history.push_back(entry);
        }
    }

    val = pf->get_param_string("rppinexo.file_path");
    if (val) app.project.file_path = utf8_to_wide(val);

    for (auto& entry : app.project.file_history) {
        if (entry.path == app.project.file_path) {
            app.project.config.base_time_sec = entry.base_time_sec;
            break;
        }
    }
}

void save_project_file_path_and_history(EDIT_SECTION* edit, const AppState& app, const HostContext& host) {
    if (!edit) return;
    auto* pf = edit->get_project_file(host.edit_handle);
    if (!pf) return;
    pf->set_param_string("rppinexo.file_path", wide_to_utf8(app.project.file_path).c_str());
    int count = (int)std::min(app.project.file_history.size(), size_t(10));
    pf->set_param_string("rppinexo.history_count", std::to_string(count).c_str());
    for (int i = 0; i < 10; i++) {
        std::string key = "rppinexo.history_" + std::to_string(i);
        std::string offset_key = "rppinexo.history_offset_" + std::to_string(i);
        if (i < count) {
            pf->set_param_string(key.c_str(), wide_to_utf8(app.project.file_history[i].path).c_str());
            pf->set_param_string(offset_key.c_str(), std::to_string(app.project.file_history[i].base_time_sec).c_str());
        } else {
            pf->set_param_string(key.c_str(), "");
            pf->set_param_string(offset_key.c_str(), "");
        }
    }
}

void flush_project_file_state(EDIT_SECTION* edit, const AppState& app, const HostContext& host) {
    if (!g_project_state_dirty || !edit) return;
    save_project_file_path_and_history(edit, app, host);
    g_project_state_dirty = false;
}
