#pragma once
#include <string>
#include <unordered_map>
#include <sstream>
#include <iomanip>

// ------------------------------------------------------------------
// substitute_variables — 将 "$var$" 替换为变量值
// ------------------------------------------------------------------
// 数值变量 → 替换为数字字符串（去除 trailing zeros）
// 文本变量 → 替换为原字符串（不包裹引号）
// 未找到   → 保留原样（调用方可选择是否记录警告）
// ------------------------------------------------------------------

inline std::string substitute_variables(
    const std::string& input,
    const std::unordered_map<std::string, double>& num_vars,
    const std::unordered_map<std::string, std::string>& text_vars
) {
    std::string result = input;
    size_t pos = 0;

    while ((pos = result.find('$', pos)) != std::string::npos) {
        size_t end = result.find('$', pos + 1);
        if (end == std::string::npos) break; // 不成对的 $ 不处理

        std::string var_name = result.substr(pos + 1, end - pos - 1);

        auto num_it = num_vars.find(var_name);
        if (num_it != num_vars.end()) {
            // 数值变量 → 替换为数值字面量，去除无意义尾随零
            std::ostringstream oss;
            oss << std::setprecision(15) << num_it->second;
            std::string replacement = oss.str();
            // 去除 trailing zeros 和可能的 trailing dot
            if (replacement.find('.') != std::string::npos) {
                while (!replacement.empty() && replacement.back() == '0')
                    replacement.pop_back();
                if (!replacement.empty() && replacement.back() == '.')
                    replacement.pop_back();
            }
            if (replacement.empty()) replacement = "0";
            result.replace(pos, end - pos + 1, replacement);
            pos += replacement.size();
            continue;
        }

        auto text_it = text_vars.find(var_name);
        if (text_it != text_vars.end()) {
            // 文本变量 → 替换为原字符串
            result.replace(pos, end - pos + 1, text_it->second);
            pos += text_it->second.size();
            continue;
        }

        // 变量未找到：保留原样，跳过
        pos = end + 1;
    }

    return result;
}
