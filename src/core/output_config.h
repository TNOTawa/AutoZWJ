#pragma once

enum FlipType {
    FLIP_NONE = 0,
    FLIP_HORIZONTAL = 1,
    FLIP_VERTICAL = 2,
    FLIP_CW = 3,
    FLIP_CCW = 4,
};

struct OutputConfig {
    int fps_num = 60;
    int fps_den = 1;
    bool alt_flip = false;
    int flip_type = 0;
    int flip_counter_mode = 0;
    // 默认开启：与 AviUtl2 的 BPM 网格算法一致（向上取整），关闭后物件可能与网格不对齐
    bool use_round_up = true;
    int clipping = 0;
    int is_ex_set = 0;

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