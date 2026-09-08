#include "ustx_parser.h"
#include "parsers/ust/ust_common.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct YamlLine {
    int indent = 0;
    std::string text;
};

struct YamlItem {
    size_t begin = 0;
    size_t end = 0;
    int indent = 0;
    std::unordered_map<std::string, std::string> fields;
    std::unordered_map<std::string, size_t> field_lines;
};

struct UstxPart {
    int track_no = 0;
    int position = 0;
    std::string name;
    std::vector<autozwj_ust::RawNote> notes;
};

struct TimeSignature {
    int bar_position = 0;
    int beat_per_bar = 4;
    int beat_unit = 4;
    size_t order = 0;
};

std::string strip_comment(const std::string& line) {
    bool single_quote = false;
    bool double_quote = false;
    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (c == '\'' && !double_quote) single_quote = !single_quote;
        if (c == '"' && !single_quote && (i == 0 || line[i - 1] != '\\')) double_quote = !double_quote;
        if (c == '#' && !single_quote && !double_quote &&
            (i == 0 || std::isspace(static_cast<unsigned char>(line[i - 1])))) {
            return line.substr(0, i);
        }
    }
    return line;
}

std::vector<YamlLine> read_yaml_lines(const std::string& text) {
    std::vector<YamlLine> result;
    size_t begin = 0;
    while (begin <= text.size()) {
        size_t end = text.find('\n', begin);
        if (end == std::string::npos) end = text.size();
        std::string line = strip_comment(text.substr(begin, end - begin));
        if (!line.empty() && line.back() == '\r') line.pop_back();

        int indent = 0;
        while (indent < static_cast<int>(line.size()) && line[indent] == ' ') indent++;
        std::string content = autozwj_ust::trim(line.substr(static_cast<size_t>(indent)));
        if (!content.empty()) result.push_back({indent, std::move(content)});

        if (end == text.size()) break;
        begin = end + 1;
    }
    return result;
}

bool find_mapping_colon(const std::string& text, size_t& position) {
    bool single_quote = false;
    bool double_quote = false;
    int braces = 0;
    for (size_t i = 0; i < text.size(); i++) {
        char c = text[i];
        if (c == '\'' && !double_quote) single_quote = !single_quote;
        if (c == '"' && !single_quote && (i == 0 || text[i - 1] != '\\')) double_quote = !double_quote;
        if (single_quote || double_quote) continue;
        if (c == '{' || c == '[') braces++;
        else if (c == '}' || c == ']') braces--;
        else if (c == ':' && braces == 0) {
            position = i;
            return true;
        }
    }
    return false;
}

std::string unquote_scalar(std::string value) {
    value = autozwj_ust::trim(value);
    if (value.size() < 2) return value;
    if (value.front() == '\'' && value.back() == '\'') {
        std::string result = value.substr(1, value.size() - 2);
        std::string output;
        for (size_t i = 0; i < result.size(); i++) {
            if (result[i] == '\'' && i + 1 < result.size() && result[i + 1] == '\'') {
                output.push_back('\'');
                i++;
            } else {
                output.push_back(result[i]);
            }
        }
        return output;
    }
    if (value.front() == '"' && value.back() == '"') {
        std::string result = value.substr(1, value.size() - 2);
        std::string output;
        for (size_t i = 0; i < result.size(); i++) {
            if (result[i] != '\\' || i + 1 >= result.size()) {
                output.push_back(result[i]);
                continue;
            }
            char escaped = result[++i];
            switch (escaped) {
            case 'n': output.push_back('\n'); break;
            case 'r': output.push_back('\r'); break;
            case 't': output.push_back('\t'); break;
            case '\\': output.push_back('\\'); break;
            case '"': output.push_back('"'); break;
            default: output.push_back(escaped); break;
            }
        }
        return output;
    }
    return value;
}

std::unordered_map<std::string, std::string> parse_flow_map(const std::string& value) {
    std::unordered_map<std::string, std::string> result;
    std::string body = autozwj_ust::trim(value);
    if (body.size() < 2 || body.front() != '{' || body.back() != '}') return result;
    body = body.substr(1, body.size() - 2);

    size_t begin = 0;
    bool single_quote = false;
    bool double_quote = false;
    int nested = 0;
    auto parse_piece = [&](const std::string& piece) {
        size_t colon = 0;
        if (!find_mapping_colon(piece, colon)) return;
        std::string key = unquote_scalar(piece.substr(0, colon));
        if (key.empty()) return;
        result[key] = unquote_scalar(piece.substr(colon + 1));
    };

    for (size_t i = 0; i <= body.size(); i++) {
        char c = i < body.size() ? body[i] : ',';
        if (c == '\'' && !double_quote) single_quote = !single_quote;
        if (c == '"' && !single_quote && (i == 0 || body[i - 1] != '\\')) double_quote = !double_quote;
        if (!single_quote && !double_quote) {
            if (c == '{' || c == '[') nested++;
            else if (c == '}' || c == ']') nested--;
        }
        if (c == ',' && !single_quote && !double_quote && nested == 0) {
            parse_piece(body.substr(begin, i - begin));
            begin = i + 1;
        }
    }
    return result;
}

std::unordered_map<std::string, std::string> parse_item_inline(const std::string& text) {
    std::unordered_map<std::string, std::string> result;
    std::string rest = autozwj_ust::trim(text);
    if (rest.empty()) return result;
    if (rest.front() == '{') return parse_flow_map(rest);

    size_t colon = 0;
    if (find_mapping_colon(rest, colon)) {
        result[unquote_scalar(rest.substr(0, colon))] = unquote_scalar(rest.substr(colon + 1));
    }
    return result;
}

bool is_sequence_item(const YamlLine& line) {
    return line.text == "-" || (line.text.size() >= 2 && line.text[0] == '-' &&
                                 std::isspace(static_cast<unsigned char>(line.text[1])));
}

bool is_mapping_field(const YamlLine& line) {
    size_t colon = 0;
    return find_mapping_colon(line.text, colon) && line.text.front() != '-';
}

std::vector<YamlItem> parse_sequence(const std::vector<YamlLine>& lines, size_t section_line,
                                     size_t limit = std::numeric_limits<size_t>::max()) {
    std::vector<YamlItem> result;
    if (section_line >= lines.size()) return result;
    int section_indent = lines[section_line].indent;
    size_t first = section_line + 1;
    while (first < lines.size() && first < limit) {
        if (is_sequence_item(lines[first]) && lines[first].indent >= section_indent) break;
        if (lines[first].indent <= section_indent && is_mapping_field(lines[first])) return result;
        first++;
    }
    if (first >= lines.size() || first >= limit || !is_sequence_item(lines[first])) return result;

    int item_indent = lines[first].indent;
    for (size_t i = first; i < lines.size() && i < limit;) {
        if (lines[i].indent != item_indent || !is_sequence_item(lines[i])) break;
        size_t end = i + 1;
        while (end < lines.size() && end < limit &&
               !(lines[end].indent == item_indent && is_sequence_item(lines[end]))) {
            if (lines[end].indent <= item_indent) break;
            end++;
        }

        YamlItem item;
        item.begin = i;
        item.end = end;
        item.indent = item_indent;
        std::string inline_text = lines[i].text.size() > 1 ?
            autozwj_ust::trim(lines[i].text.substr(1)) : "";
        auto inline_fields = parse_item_inline(inline_text);
        item.fields.insert(inline_fields.begin(), inline_fields.end());

        if (!inline_text.empty() && inline_text.front() != '{') {
            size_t colon = 0;
            if (find_mapping_colon(inline_text, colon)) {
                item.field_lines[unquote_scalar(inline_text.substr(0, colon))] = i;
            }
        }

        for (size_t j = i + 1; j < end; j++) {
            if (lines[j].indent <= item_indent || !is_mapping_field(lines[j])) continue;
            size_t colon = 0;
            if (!find_mapping_colon(lines[j].text, colon)) continue;
            std::string key = unquote_scalar(lines[j].text.substr(0, colon));
            std::string value = unquote_scalar(lines[j].text.substr(colon + 1));
            if (item.fields.find(key) == item.fields.end() || !value.empty()) {
                item.fields[key] = value;
                item.field_lines[key] = j;
            }
        }
        result.push_back(std::move(item));
        i = end;
    }
    return result;
}

size_t find_top_level_section(const std::vector<YamlLine>& lines, const char* key) {
    std::string expected = std::string(key) + ":";
    for (size_t i = 0; i < lines.size(); i++) {
        if (lines[i].indent == 0 && lines[i].text == expected) return i;
    }
    return lines.size();
}

std::vector<YamlItem> parse_nested_sequence(const std::vector<YamlLine>& lines,
                                            const YamlItem& parent, const char* key) {
    auto line_it = parent.field_lines.find(key);
    if (line_it == parent.field_lines.end()) return {};
    return parse_sequence(lines, line_it->second, parent.end);
}

bool get_int(const std::unordered_map<std::string, std::string>& fields,
             const char* key, int& result) {
    auto it = fields.find(key);
    return it != fields.end() && autozwj_ust::parse_int(unquote_scalar(it->second), result);
}

bool get_double(const std::unordered_map<std::string, std::string>& fields,
                const char* key, double& result) {
    auto it = fields.find(key);
    return it != fields.end() && autozwj_ust::parse_double(unquote_scalar(it->second), result);
}

std::string get_string(const std::unordered_map<std::string, std::string>& fields,
                       const char* key) {
    auto it = fields.find(key);
    if (it == fields.end()) return {};
    return maybe_cp932_to_utf8(unquote_scalar(it->second));
}

std::vector<TimeSignature> parse_time_signatures(const std::vector<YamlLine>& lines) {
    std::vector<TimeSignature> result;
    size_t section = find_top_level_section(lines, "time_signatures");
    if (section == lines.size()) return result;
    size_t order = 0;
    for (const auto& item : parse_sequence(lines, section)) {
        TimeSignature value;
        get_int(item.fields, "bar_position", value.bar_position);
        get_int(item.fields, "beat_per_bar", value.beat_per_bar);
        get_int(item.fields, "beat_unit", value.beat_unit);
        value.order = order++;
        if (value.bar_position >= 0 && value.beat_per_bar > 0 && value.beat_unit > 0) {
            result.push_back(value);
        }
    }
    std::stable_sort(result.begin(), result.end(), [](const TimeSignature& lhs, const TimeSignature& rhs) {
        if (lhs.bar_position != rhs.bar_position) return lhs.bar_position < rhs.bar_position;
        return lhs.order < rhs.order;
    });
    return result;
}

int bar_to_tick(int bar, const std::vector<TimeSignature>& signatures, int resolution) {
    if (bar <= 0 || signatures.empty()) return 0;
    int tick = 0;
    int previous_bar = 0;
    int beat_per_bar = 4;
    int beat_unit = 4;
    for (const auto& signature : signatures) {
        if (signature.bar_position >= bar) break;
        int count = signature.bar_position - previous_bar;
        if (count > 0) {
            tick += count * beat_per_bar * resolution * 4 / beat_unit;
        }
        previous_bar = signature.bar_position;
        beat_per_bar = signature.beat_per_bar;
        beat_unit = signature.beat_unit;
    }
    if (bar > previous_bar) {
        tick += (bar - previous_bar) * beat_per_bar * resolution * 4 / beat_unit;
    }
    return tick;
}

std::vector<autozwj_ust::TempoChange> parse_tempos(const std::vector<YamlLine>& lines,
                                                    double default_bpm,
                                                    const std::vector<TimeSignature>& signatures) {
    std::vector<autozwj_ust::TempoChange> result;
    result.push_back({0, default_bpm, 4, true, false, 0});

    size_t order = 1;
    size_t section = find_top_level_section(lines, "tempos");
    if (section != lines.size()) {
        for (const auto& item : parse_sequence(lines, section)) {
            int position = 0;
            double bpm = 0.0;
            if (!get_int(item.fields, "position", position) ||
                !get_double(item.fields, "bpm", bpm) || !autozwj_ust::valid_bpm(bpm)) {
                continue;
            }
            result.push_back({position, bpm, 4, true, false, order++});
        }
    }

    for (const auto& signature : signatures) {
        int tick = bar_to_tick(signature.bar_position, signatures, 480);
        result.push_back({tick, default_bpm, signature.beat_per_bar, false, true, order++});
    }
    return result;
}

void parse_track_names(const std::vector<YamlLine>& lines, std::vector<std::string>& names) {
    size_t section = find_top_level_section(lines, "tracks");
    if (section == lines.size()) return;
    for (const auto& item : parse_sequence(lines, section)) {
        std::string name = get_string(item.fields, "track_name");
        if (name.empty()) name = get_string(item.fields, "name");
        names.push_back(std::move(name));
    }
}

std::vector<UstxPart> parse_parts(const std::vector<YamlLine>& lines, const char* section_name) {
    std::vector<UstxPart> result;
    size_t section = find_top_level_section(lines, section_name);
    if (section == lines.size()) return result;

    size_t note_order = 0;
    for (const auto& part_item : parse_sequence(lines, section)) {
        UstxPart part;
        get_int(part_item.fields, "track_no", part.track_no);
        get_int(part_item.fields, "position", part.position);
        part.name = get_string(part_item.fields, "name");

        auto note_items = parse_nested_sequence(lines, part_item, "notes");
        for (const auto& note_item : note_items) {
            autozwj_ust::RawNote note;
            note.order = note_order++;
            if (!get_int(note_item.fields, "position", note.tick) ||
                !get_int(note_item.fields, "duration", note.duration)) {
                continue;
            }
            int tone = 60;
            get_int(note_item.fields, "tone", tone);
            note.note_num = tone;
            get_double(note_item.fields, "tuning", note.tuning);
            note.lyric = get_string(note_item.fields, "lyric");

            for (const auto& expression : parse_nested_sequence(lines, note_item, "phoneme_expressions")) {
                std::string abbr = get_string(expression.fields, "abbr");
                double value = 0.0;
                if (abbr == "vel" && get_double(expression.fields, "value", value)) {
                    note.velocity = value;
                    break;
                }
            }

            if (note.duration > 0 && note.tick >= 0 && !autozwj_ust::is_rest_lyric(note.lyric)) {
                note.tick += part.position;
                part.notes.push_back(std::move(note));
            }
        }
        result.push_back(std::move(part));
    }
    return result;
}

} // namespace

bool parse_ustx(const std::string& path, ObjDict& objdict,
                std::vector<TrackNode>& tracks, std::vector<std::string>& file_paths) {
    std::string text;
    if (!autozwj_ust::read_text_file_utf8(path, text)) return false;
    auto lines = read_yaml_lines(text);
    if (lines.empty()) return false;

    double default_bpm = 120.0;
    for (const auto& line : lines) {
        if (line.indent != 0 || line.text.rfind("bpm:", 0) != 0) continue;
        double parsed = 0.0;
        if (autozwj_ust::parse_double(unquote_scalar(line.text.substr(4)), parsed) &&
            autozwj_ust::valid_bpm(parsed)) {
            default_bpm = parsed;
        }
    }

    auto signatures = parse_time_signatures(lines);
    auto tempo_changes = parse_tempos(lines, default_bpm, signatures);
    auto tempo_segments = autozwj_ust::normalize_tempo_changes(tempo_changes, default_bpm, 4);
    auto parts = parse_parts(lines, "voice_parts");
    if (parts.empty()) parts = parse_parts(lines, "parts");
    if (parts.empty()) return false;

    std::vector<std::string> track_names;
    parse_track_names(lines, track_names);

    int track_count = static_cast<int>(track_names.size());
    for (const auto& part : parts) track_count = std::max(track_count, part.track_no + 1);
    if (track_count <= 0) track_count = 1;

    std::vector<std::vector<autozwj_ust::RawNote>> notes_by_track(static_cast<size_t>(track_count));
    for (auto& part : parts) {
        if (part.track_no < 0) part.track_no = 0;
        if (part.track_no >= static_cast<int>(notes_by_track.size())) {
            notes_by_track.resize(static_cast<size_t>(part.track_no + 1));
        }
        auto& target = notes_by_track[static_cast<size_t>(part.track_no)];
        target.insert(target.end(), part.notes.begin(), part.notes.end());
    }
    track_count = static_cast<int>(notes_by_track.size());

    autozwj_ust::initialize_note_arrays(objdict);
    file_paths.clear();
    objdict.track_count = track_count;
    objdict.tempo_map.clear();
    objdict.bpm = tempo_segments.front().bpm;
    for (const auto& segment : tempo_segments) {
        objdict.tempo_map.push_back({
            autozwj_ust::tick_to_seconds(segment.tick, tempo_segments),
            segment.bpm,
            segment.beat,
        });
    }

    tracks.clear();
    for (int track_index = 0; track_index < track_count; track_index++) {
        if (track_index > 0) autozwj_ust::append_separator(objdict);

        auto& note_list = notes_by_track[static_cast<size_t>(track_index)];
        std::stable_sort(note_list.begin(), note_list.end(), [](const autozwj_ust::RawNote& lhs,
                                                                const autozwj_ust::RawNote& rhs) {
            if (lhs.tick != rhs.tick) return lhs.tick < rhs.tick;
            return lhs.order < rhs.order;
        });

        TrackNode track;
        track.number = track_index + 1;
        track.index = static_cast<int>(objdict.pos.size());
        track.name = track_index < static_cast<int>(track_names.size()) && !track_names[track_index].empty()
            ? utf8_to_wide(track_names[track_index])
            : L"Track " + std::to_wstring(track_index + 1);
        track.selected = true;

        for (const auto& note : note_list) {
            double position_sec = autozwj_ust::tick_to_seconds(note.tick, tempo_segments);
            double end_sec = autozwj_ust::tick_to_seconds(note.tick + note.duration, tempo_segments);
            double length_sec = end_sec - position_sec;
            if (length_sec <= 0.0) continue;
            autozwj_ust::append_note(objdict, note, position_sec, length_sec, "USTX");
        }
        track.count = static_cast<int>(objdict.pos.size()) - track.index;
        tracks.push_back(std::move(track));
    }

    bool has_note = false;
    for (const auto& track : tracks) {
        if (track.count > 0) {
            has_note = true;
            break;
        }
    }
    return has_note;
}
