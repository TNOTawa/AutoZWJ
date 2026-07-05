#include "chain/template_chain.h"
#include "ui/effect_chain_editor.h"
#include "effect/effect_dict.h"

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
