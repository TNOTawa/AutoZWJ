#pragma once
#include <string>

struct OutputConfig {
    int fps_num = 60;
    int fps_den = 1;
    int output_type = 0;
    bool alt_flip = false;
    int flip_type = 0;
    int flip_counter_mode = 0;
    bool adjustment = false;
    bool beatless_sync = false;
    bool no_gap = true;
    bool use_round_up = false;
    int clipping = 0;
    int is_ex_set = 0;
    bool random_play = false;
    double random_end = 0.0;
    bool play_pos_by_frame = false;
    std::string src_path;
    std::string exedit_lang = "ja";

    int sync_mode = 1;
    int fixed_duration_frames = 30;
    int layer_strategy = 0;
    bool reverse_layer_order = false;
    int track_filter_mode = 0;
    int track_filter_n = 1;

    int mapping_strategy = 1;
    int mapping_sequential_order = 0;
    bool mapping_no_consecutive = false;

    double base_time_sec = 0.0;
};