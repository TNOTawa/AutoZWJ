#pragma once
#include <vector>
#include <string>
#include <utility>

// ---- 中间数据结构 ----
struct ParsedEffect {
    std::string effect_name;
    std::string effect_name_zh;
    int original_index = 0;
    std::vector<std::pair<std::string, std::string>> params;
};

struct ParamBake {
    int effect_index = -1;
    std::string param_name;
    std::string param_value;   // 用户输入的原始文本（固定值/变量/表达式）
    int value_mode = 0;        // 0=固定值, 1=变量映射, 2=表达式
    bool active = false;
    bool is_motion = false;         // 是否为逗号分隔的运动参数
    std::string motion_end_value;   // 运动参数的终点值（第二个字段）
};

struct PresetEntry {
    std::string display_name;
    std::string effect_block;
    int position = -1;
    bool active = false;
    // 点击预设后高亮参数面板中的对应控件（空字符串=无映射）
    std::string highlight_target;
};

struct EffectParam {
    std::string name;
    std::string default_value;
    int type = 0;
};

struct EffectDef {
    std::string name_ja;
    std::string name_en;
    std::string name_zh;
    std::vector<EffectParam> params;
};