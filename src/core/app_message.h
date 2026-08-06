#pragma once
#include <string>

// 全局结果消息（窗口底部结果栏的数据源）。
// 供任意层写入：工程解析失败、生成结果、BPM 网格应用、剪贴板导入等。
// 只保留最近一条消息，渲染循环在窗口底部结果栏显示；写入失败原因时
// 使用 Error 严重度，成功汇总使用 Success。
//
// 注意：消息文本为已翻译的 UTF-8 字符串，写入方负责调用 tr()/tr_fmt()。

enum class AppMsgSeverity { Info, Success, Warn, Error };

struct AppMessage {
    AppMsgSeverity severity = AppMsgSeverity::Info;
    std::string text;
};

inline AppMessage& app_msg_slot() {
    static AppMessage s_msg;
    return s_msg;
}

inline void app_msg_set(AppMsgSeverity severity, const std::string& text) {
    auto& m = app_msg_slot();
    m.severity = severity;
    m.text = text;
}

inline const AppMessage& app_msg_current() {
    return app_msg_slot();
}
