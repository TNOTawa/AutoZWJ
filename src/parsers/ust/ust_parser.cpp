#include "ust_parser.h"
#include "ust_common.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct UstBlock {
    std::string header;
    std::vector<std::string> lines;
};

struct ParsedUstNote : autozwj_ust::RawNote {
    bool has_tempo = false;
    double tempo = 120.0;
};

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> result;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        result.push_back(std::move(line));
    }
    return result;
}

std::vector<UstBlock> read_blocks(const std::string& text) {
    std::vector<UstBlock> result;
    for (const auto& line : split_lines(text)) {
        std::string trimmed = autozwj_ust::trim(line);
        if (trimmed.size() >= 4 && trimmed.front() == '[' && trimmed.back() == ']') {
            result.push_back({trimmed, {}});
        } else if (!result.empty()) {
            result.back().lines.push_back(line);
        }
    }
    return result;
}

std::unordered_map<std::string, std::string> parse_key_values(const std::vector<std::string>& lines) {
    std::unordered_map<std::string, std::string> result;
    for (const auto& line : lines) {
        size_t equal = line.find('=');
        if (equal == std::string::npos) continue;

        std::string key = autozwj_ust::trim(line.substr(0, equal));
        if (key.empty()) continue;
        result[key] = line.substr(equal + 1);
    }
    return result;
}

bool parse_note_index(const std::string& header) {
    if (header.size() < 5 || header[0] != '[' || header[1] != '#' || header.back() != ']') {
        return false;
    }
    std::string index = header.substr(2, header.size() - 3);
    if (index.empty()) return false;
    return std::all_of(index.begin(), index.end(), [](char c) {
        return c >= '0' && c <= '9';
    });
}

std::string get_project_name(const std::string& path,
                             const std::unordered_map<std::string, std::string>& settings) {
    auto it = settings.find("ProjectName");
    if (it != settings.end()) {
        std::string name = autozwj_ust::trim(it->second);
        if (!name.empty()) return maybe_cp932_to_utf8(name);
    }

    size_t slash = path.find_last_of("\\/");
    size_t begin = slash == std::string::npos ? 0 : slash + 1;
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || dot < begin) dot = path.size();
    std::string fallback = path.substr(begin, dot - begin);
    return fallback.empty() ? "UST" : fallback;
}

bool get_int(const std::unordered_map<std::string, std::string>& values,
             const char* key, int& output) {
    auto it = values.find(key);
    return it != values.end() && autozwj_ust::parse_int(it->second, output);
}

bool get_double(const std::unordered_map<std::string, std::string>& values,
                const char* key, double& output) {
    auto it = values.find(key);
    return it != values.end() && autozwj_ust::parse_double(it->second, output);
}

} // namespace

bool parse_ust(const std::string& path, ObjDict& objdict,
               std::vector<TrackNode>& tracks, std::vector<std::string>& file_paths) {
    std::string text;
    if (!autozwj_ust::read_text_file_utf8(path, text)) return false;

    auto blocks = read_blocks(text);
    if (blocks.empty()) return false;

    std::unordered_map<std::string, std::string> settings;
    for (const auto& block : blocks) {
        if (block.header == "[#SETTING]") {
            settings = parse_key_values(block.lines);
            break;
        }
    }

    double default_bpm = 120.0;
    bool fix_initial_tempo = false;
    if (auto it = settings.find("Tempo"); it != settings.end()) {
        double parsed = 0.0;
        if (autozwj_ust::parse_double(it->second, parsed) && autozwj_ust::valid_bpm(parsed)) {
            default_bpm = parsed;
        } else {
            fix_initial_tempo = true;
        }
    }

    std::vector<ParsedUstNote> notes;
    std::vector<autozwj_ust::TempoChange> tempo_changes;
    tempo_changes.push_back({0, default_bpm, 4, true, false, 0});

    int last_note_pos = 0;
    int last_note_end = 0;
    size_t order = 1;
    for (const auto& block : blocks) {
        if (!parse_note_index(block.header)) continue;

        auto values = parse_key_values(block.lines);

        int length = 0;
        int duration = 0;
        int delta = 0;
        bool has_length = get_int(values, "Length", length);
        bool has_duration = get_int(values, "Duration", duration);
        bool has_delta = get_int(values, "Delta", delta);

        ParsedUstNote note;
        note.order = order++;
        if (has_delta && has_duration) {
            note.tick = last_note_pos + delta;
            note.duration = duration;
        } else if (has_length) {
            note.tick = last_note_end;
            note.duration = length;
        } else {
            continue;
        }

        int note_num = 60;
        if (get_int(values, "NoteNum", note_num)) note.note_num = note_num;

        auto lyric_it = values.find("Lyric");
        if (lyric_it != values.end()) {
            note.lyric = lyric_it->second;
            if (!note.lyric.empty() && note.lyric.front() == '?') note.lyric.erase(0, 1);
            note.lyric = maybe_cp932_to_utf8(note.lyric);
        }

        double velocity = 0.0;
        if (get_double(values, "Velocity", velocity)) note.velocity = velocity;

        double tempo = 0.0;
        if (get_double(values, "Tempo", tempo) && autozwj_ust::valid_bpm(tempo)) {
            note.has_tempo = true;
            note.tempo = tempo;
            if (fix_initial_tempo) {
                default_bpm = tempo;
                tempo_changes[0].bpm = tempo;
                fix_initial_tempo = false;
            } else {
                tempo_changes.push_back({note.tick, tempo, 4, true, false, order});
            }
        }

        last_note_pos = note.tick;
        last_note_end = note.tick + std::max(note.duration, 0);
        if (note.duration <= 0 || note.tick < 0) continue;
        if (autozwj_ust::is_rest_lyric(note.lyric)) continue;
        notes.push_back(std::move(note));
    }

    if (notes.empty()) return false;

    std::stable_sort(notes.begin(), notes.end(), [](const ParsedUstNote& lhs, const ParsedUstNote& rhs) {
        if (lhs.tick != rhs.tick) return lhs.tick < rhs.tick;
        return lhs.order < rhs.order;
    });

    auto tempo_segments = autozwj_ust::normalize_tempo_changes(tempo_changes, default_bpm, 4);

    autozwj_ust::initialize_note_arrays(objdict);
    file_paths.clear();
    tracks.clear();
    objdict.track_count = 1;
    objdict.filelist.clear();
    objdict.tempo_map.clear();
    objdict.bpm = tempo_segments.front().bpm;
    for (const auto& segment : tempo_segments) {
        objdict.tempo_map.push_back({
            autozwj_ust::tick_to_seconds(segment.tick, tempo_segments),
            segment.bpm,
            segment.beat,
        });
    }

    for (const auto& note : notes) {
        double position_sec = autozwj_ust::tick_to_seconds(note.tick, tempo_segments);
        double end_sec = autozwj_ust::tick_to_seconds(note.tick + note.duration, tempo_segments);
        double length_sec = end_sec - position_sec;
        if (length_sec <= 0.0) continue;
        autozwj_ust::append_note(objdict, note, position_sec, length_sec, "UST");
    }

    if (objdict.pos.size() <= 1) return false;

    TrackNode track;
    track.name = utf8_to_wide(get_project_name(path, settings));
    track.number = 1;
    track.index = 1;
    track.count = static_cast<int>(objdict.pos.size()) - 1;
    track.selected = true;
    tracks.push_back(std::move(track));
    return true;
}
