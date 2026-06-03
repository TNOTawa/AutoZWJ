#pragma once
#include <cstring>
#include <unordered_map>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <algorithm>

// ------------------------------------------------------------------
// ExprEvaluator — 递归下降算术/逻辑表达式求值器（零外部依赖）
// ------------------------------------------------------------------
// 支持：
//   运算符: + - * / % == != < > <= >= && || !
//   函数:   abs, min, max, clamp, floor, ceil, round, sin, cos, sqrt, pow
//           rand(max), rand(min,max), rand_int(max), rand_int(min,max)
//   常量:   pi, e
//   括号:   ()
//   变量:   通过 set_var() / set_vars() 预先注入（键不含 $ 包裹）
//   一元负号、逻辑非
//
// 错误处理：不抛异常，通过 bool + error 字符串返回
// ------------------------------------------------------------------

class ExprEvaluator {
public:
    void set_var(const std::string& name, double value) {
        vars_[name] = value;
    }

    void set_vars(const std::unordered_map<std::string, double>& vars) {
        for (const auto& kv : vars) vars_[kv.first] = kv.second;
    }

    void clear_vars() {
        vars_.clear();
    }

    // 设置确定性随机种子（默认 0）
    void set_seed(uint32_t seed) {
        rand_seed_ = seed;
    }

    // 求值算术/逻辑表达式
    bool evaluate(const std::string& expr, double& result, std::string& error) {
        s_ = expr;
        pos_ = 0;
        error_.clear();
        skip_ws();
        if (pos_ >= s_.size()) {
            result = 0.0;
            return true; // 空表达式 => 0.0
        }
        double val = 0;
        if (!parse_expr(val)) {
            error = error_;
            return false;
        }
        skip_ws();
        if (pos_ != s_.size()) {
            error = "表达式语法错误: 位置 " + std::to_string(pos_);
            return false;
        }
        result = val;
        return true;
    }

    bool evaluate_condition(const std::string& cond, double& result, std::string& error) {
        return evaluate(cond, result, error);
    }

private:
    std::string s_;
    size_t pos_ = 0;
    std::string error_;
    std::unordered_map<std::string, double> vars_;
    uint32_t rand_seed_ = 0;

    // ---- 词法辅助 ----
    void skip_ws() {
        while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) pos_++;
    }

    bool peek(char& c) {
        skip_ws();
        if (pos_ >= s_.size()) return false;
        c = s_[pos_];
        return true;
    }

    bool consume(char c) {
        skip_ws();
        if (pos_ < s_.size() && s_[pos_] == c) { pos_++; return true; }
        return false;
    }

    bool consume_str(const char* str) {
        skip_ws();
        size_t len = std::strlen(str);
        if (pos_ + len <= s_.size() && std::strncmp(s_.c_str() + pos_, str, len) == 0) {
            pos_ += len;
            return true;
        }
        return false;
    }

    bool is_id_start(char c) const {
        return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
    }

    bool is_id_char(char c) const {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.';
    }

    // ---- 语法解析 ----

    // expr = or_expr
    bool parse_expr(double& out) {
        return parse_or(out);
    }

    // or_expr = and_expr ( "||" and_expr )*
    bool parse_or(double& out) {
        if (!parse_and(out)) return false;
        while (true) {
            if (consume_str("||")) {
                double rhs = 0;
                if (!parse_and(rhs)) return false;
                out = (out != 0.0 || rhs != 0.0) ? 1.0 : 0.0;
            } else break;
        }
        return true;
    }

    // and_expr = eq_expr ( "&&" eq_expr )*
    bool parse_and(double& out) {
        if (!parse_eq(out)) return false;
        while (true) {
            if (consume_str("&&")) {
                double rhs = 0;
                if (!parse_eq(rhs)) return false;
                out = (out != 0.0 && rhs != 0.0) ? 1.0 : 0.0;
            } else break;
        }
        return true;
    }

    // eq_expr = rel_expr ( ( "==" | "!=" ) rel_expr )*
    bool parse_eq(double& out) {
        if (!parse_rel(out)) return false;
        while (true) {
            if (consume_str("==")) {
                double rhs = 0;
                if (!parse_rel(rhs)) return false;
                out = (std::abs(out - rhs) < 1e-9) ? 1.0 : 0.0;
            } else if (consume_str("!=")) {
                double rhs = 0;
                if (!parse_rel(rhs)) return false;
                out = (std::abs(out - rhs) >= 1e-9) ? 1.0 : 0.0;
            } else break;
        }
        return true;
    }

    // rel_expr = add_expr ( ( "<=" | ">=" | "<" | ">" ) add_expr )*
    bool parse_rel(double& out) {
        if (!parse_add(out)) return false;
        while (true) {
            if (consume_str("<=")) {
                double rhs = 0;
                if (!parse_add(rhs)) return false;
                out = (out <= rhs + 1e-9) ? 1.0 : 0.0;
            } else if (consume_str(">=")) {
                double rhs = 0;
                if (!parse_add(rhs)) return false;
                out = (out >= rhs - 1e-9) ? 1.0 : 0.0;
            } else if (consume_str("<")) {
                double rhs = 0;
                if (!parse_add(rhs)) return false;
                out = (out < rhs - 1e-9) ? 1.0 : 0.0;
            } else if (consume_str(">")) {
                double rhs = 0;
                if (!parse_add(rhs)) return false;
                out = (out > rhs + 1e-9) ? 1.0 : 0.0;
            } else break;
        }
        return true;
    }

    // add_expr = mul_expr ( ( "+" | "-" ) mul_expr )*
    bool parse_add(double& out) {
        if (!parse_mul(out)) return false;
        while (true) {
            if (consume('+')) {
                double rhs = 0;
                if (!parse_mul(rhs)) return false;
                out += rhs;
            } else if (consume('-')) {
                double rhs = 0;
                if (!parse_mul(rhs)) return false;
                out -= rhs;
            } else break;
        }
        return true;
    }

    // mul_expr = unary ( ( "*" | "/" | "%" ) unary )*
    bool parse_mul(double& out) {
        if (!parse_unary(out)) return false;
        while (true) {
            if (consume('*')) {
                double rhs = 0;
                if (!parse_unary(rhs)) return false;
                out *= rhs;
            } else if (consume('/')) {
                double rhs = 0;
                if (!parse_unary(rhs)) return false;
                if (std::abs(rhs) < 1e-12) { error_ = "除零错误"; return false; }
                out /= rhs;
            } else if (consume('%')) {
                double rhs = 0;
                if (!parse_unary(rhs)) return false;
                if (std::abs(rhs) < 1e-12) { error_ = "除零错误(取模)"; return false; }
                out = std::fmod(out, rhs);
            } else break;
        }
        return true;
    }

    // unary = ( "!" | "-" ) unary | primary
    bool parse_unary(double& out) {
        if (consume('!')) {
            if (!parse_unary(out)) return false;
            out = (out == 0.0) ? 1.0 : 0.0;
            return true;
        }
        if (consume('-')) {
            if (!parse_unary(out)) return false;
            out = -out;
            return true;
        }
        return parse_primary(out);
    }

    // primary = number | identifier [ "(" args ")" ] | "(" expr ")"
    bool parse_primary(double& out) {
        skip_ws();
        if (consume('(')) {
            if (!parse_expr(out)) return false;
            if (!consume(')')) { error_ = "缺少右括号"; return false; }
            return true;
        }

        char c;
        if (!peek(c)) { error_ = "表达式意外结束"; return false; }

        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            return parse_number(out);
        }

        if (is_id_start(c)) {
            std::string id = parse_identifier();
            if (id.empty()) { error_ = "无效的标识符"; return false; }

            // 常量
            if (id == "pi") { out = 3.14159265358979323846; return true; }
            if (id == "e")  { out = 2.71828182845904523536; return true; }

            // 变量
            auto it = vars_.find(id);
            if (it != vars_.end()) {
                out = it->second;
                return true;
            }

            // 函数调用
            if (consume('(')) {
                std::vector<double> args;
                skip_ws();
                if (!peek(c) || c != ')') {
                    while (true) {
                        double arg = 0;
                        if (!parse_expr(arg)) return false;
                        args.push_back(arg);
                        skip_ws();
                        if (!peek(c)) { error_ = "函数参数意外结束"; return false; }
                        if (c == ')') { pos_++; break; }
                        if (c == ',') { pos_++; continue; }
                        error_ = "函数参数分隔符错误"; return false;
                    }
                } else {
                    pos_++; // consume ')'
                }
                return eval_function(id, args, out);
            }

            error_ = "未知标识符: " + id;
            return false;
        }

        error_ = std::string("意外的字符: ") + c;
        return false;
    }

    bool parse_number(double& out) {
        skip_ws();
        size_t start = pos_;
        bool has_dot = false;
        while (pos_ < s_.size()) {
            char c = s_[pos_];
            if (std::isdigit(static_cast<unsigned char>(c))) { pos_++; }
            else if (c == '.' && !has_dot) { has_dot = true; pos_++; }
            else break;
        }
        if (start == pos_) { error_ = "数字解析失败"; return false; }
        try {
            out = std::stod(s_.substr(start, pos_ - start));
        } catch (...) {
            error_ = "无效的数字"; return false;
        }
        return true;
    }

    std::string parse_identifier() {
        skip_ws();
        size_t start = pos_;
        while (pos_ < s_.size() && is_id_char(s_[pos_])) pos_++;
        return s_.substr(start, pos_ - start);
    }

    bool eval_function(const std::string& name, const std::vector<double>& args, double& out) {
        if (name == "abs") {
            if (args.size() != 1) { error_ = "abs() 需要 1 个参数"; return false; }
            out = std::abs(args[0]); return true;
        }
        if (name == "floor") {
            if (args.size() != 1) { error_ = "floor() 需要 1 个参数"; return false; }
            out = std::floor(args[0]); return true;
        }
        if (name == "ceil") {
            if (args.size() != 1) { error_ = "ceil() 需要 1 个参数"; return false; }
            out = std::ceil(args[0]); return true;
        }
        if (name == "round") {
            if (args.size() != 1) { error_ = "round() 需要 1 个参数"; return false; }
            out = std::round(args[0]); return true;
        }
        if (name == "sin") {
            if (args.size() != 1) { error_ = "sin() 需要 1 个参数"; return false; }
            out = std::sin(args[0]); return true;
        }
        if (name == "cos") {
            if (args.size() != 1) { error_ = "cos() 需要 1 个参数"; return false; }
            out = std::cos(args[0]); return true;
        }
        if (name == "sqrt") {
            if (args.size() != 1) { error_ = "sqrt() 需要 1 个参数"; return false; }
            if (args[0] < 0) { error_ = "sqrt() 参数为负"; return false; }
            out = std::sqrt(args[0]); return true;
        }
        if (name == "min") {
            if (args.size() < 1) { error_ = "min() 至少需要 1 个参数"; return false; }
            out = args[0];
            for (size_t i = 1; i < args.size(); i++) out = std::min(out, args[i]);
            return true;
        }
        if (name == "max") {
            if (args.size() < 1) { error_ = "max() 至少需要 1 个参数"; return false; }
            out = args[0];
            for (size_t i = 1; i < args.size(); i++) out = std::max(out, args[i]);
            return true;
        }
        if (name == "clamp") {
            if (args.size() != 3) { error_ = "clamp() 需要 3 个参数"; return false; }
            out = std::max(args[1], std::min(args[0], args[2])); return true;
        }
        if (name == "pow") {
            if (args.size() != 2) { error_ = "pow() 需要 2 个参数"; return false; }
            out = std::pow(args[0], args[1]); return true;
        }
        if (name == "rand") {
            if (args.size() == 1) {
                double r = next_rand_double();
                out = r * args[0];
                return true;
            }
            if (args.size() == 2) {
                double r = next_rand_double();
                out = args[0] + r * (args[1] - args[0]);
                return true;
            }
            error_ = "rand() 需要 1 或 2 个参数"; return false;
        }
        if (name == "map_pitch") {
            if (args.size() != 2) { error_ = "map_pitch() 需要 2 个参数"; return false; }
            auto it = vars_.find("note.pitch_ratio");
            double ratio = (it != vars_.end()) ? it->second : 0.5;
            out = args[0] + (args[1] - args[0]) * ratio;
            return true;
        }
        if (name == "rand_int") {
            if (args.size() == 1) {
                int maxv = static_cast<int>(std::floor(args[0]));
                if (maxv <= 0) { out = 0; return true; }
                double r = next_rand_double();
                out = static_cast<double>(static_cast<int>(r * maxv));
                return true;
            }
            if (args.size() == 2) {
                int minv = static_cast<int>(std::ceil(args[0]));
                int maxv = static_cast<int>(std::floor(args[1]));
                if (maxv < minv) std::swap(minv, maxv);
                int range = maxv - minv + 1;
                if (range <= 0) { out = static_cast<double>(minv); return true; }
                double r = next_rand_double();
                out = static_cast<double>(minv + static_cast<int>(r * range));
                return true;
            }
            error_ = "rand_int() 需要 1 或 2 个参数"; return false;
        }
        error_ = "未知函数: " + name;
        return false;
    }

    double next_rand_double() {
        rand_seed_ = rand_seed_ * 1103515245u + 12345u;
        // 返回 [0, 1)
        return static_cast<double>(rand_seed_ & 0x7FFFFFFFu) / static_cast<double>(0x80000000u);
    }
};
