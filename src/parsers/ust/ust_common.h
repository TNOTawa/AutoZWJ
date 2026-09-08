#pragma once

#include "codec/codec.h"
#include "core/project_model.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

#include <windows.h>

namespace autozwj_ust {

struct RawNote {
    int tick = 0;
    int duration = 0;
    double note_num = 60.0;
    double tuning = 0.0;
    double velocity = -1.0;
    std::string lyric;
    size_t order = 0;
};

struct TempoChange {
    int tick = 0;
    double bpm = 120.0;
    int beat = 4;
    bool has_bpm = false;
    bool has_beat = false;
    size_t order = 0;
};

struct TempoSegment {
    int tick = 0;
    double bpm = 120.0;
    int beat = 4;
};

inline std::string trim(const std::string& value) {
    size_t begin = 0;
    while (begin < value.size() && static_cast<unsigned char>(value[begin]) <= 0x20) {
        begin++;
    }
    size_t end = value.size();
    while (end > begin && static_cast<unsigned char>(value[end - 1]) <= 0x20) {
        end--;
    }
    return value.substr(begin, end - begin);
}

inline std::string lower_ascii(std::string value) {
    for (char& c : value) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return value;
}

inline bool parse_double(const std::string& text, double& value) {
    std::string s = trim(text);
    if (s.empty()) return false;

    char* end = nullptr;
    value = std::strtod(s.c_str(), &end);
    if (end == s.c_str()) return false;
    while (*end != '\0' && static_cast<unsigned char>(*end) <= 0x20) end++;
    return *end == '\0' && std::isfinite(value);
}

inline bool parse_int(const std::string& text, int& value) {
    double parsed = 0.0;
    if (!parse_double(text, parsed)) return false;
    if (parsed < static_cast<double>(std::numeric_limits<int>::min()) ||
        parsed > static_cast<double>(std::numeric_limits<int>::max())) {
        return false;
    }
    value = static_cast<int>(std::lround(parsed));
    return true;
}

inline bool read_file_bytes(const std::string& path, std::string& output) {
    std::wstring wpath = utf8_to_wide(path);
    HANDLE file = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 ||
        static_cast<unsigned long long>(size.QuadPart) > static_cast<unsigned long long>(SIZE_MAX) ||
        static_cast<unsigned long long>(size.QuadPart) > static_cast<unsigned long long>(std::numeric_limits<DWORD>::max())) {
        CloseHandle(file);
        return false;
    }

    output.assign(static_cast<size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    BOOL ok = ReadFile(file, output.data(), static_cast<DWORD>(output.size()), &read, nullptr);
    CloseHandle(file);
    if (!ok || read != output.size()) {
        output.clear();
        return false;
    }
    return true;
}

inline bool read_text_file_utf8(const std::string& path, std::string& output) {
    std::string raw;
    if (!read_file_bytes(path, raw)) return false;

    if (raw.size() >= 3 &&
        static_cast<unsigned char>(raw[0]) == 0xEF &&
        static_cast<unsigned char>(raw[1]) == 0xBB &&
        static_cast<unsigned char>(raw[2]) == 0xBF) {
        raw.erase(0, 3);
    }

    output = maybe_cp932_to_utf8(raw);
    return !output.empty();
}

inline bool is_rest_lyric(const std::string& lyric) {
    std::string normalized = lower_ascii(trim(lyric));
    return normalized == "r";
}

inline bool valid_bpm(double bpm) {
    return std::isfinite(bpm) && bpm > 0.0 && bpm <= 1000.0;
}

inline std::vector<TempoSegment> normalize_tempo_changes(
    std::vector<TempoChange> changes, double default_bpm, int default_beat) {
    if (!valid_bpm(default_bpm)) default_bpm = 120.0;
    if (default_beat <= 0) default_beat = 4;

    std::stable_sort(changes.begin(), changes.end(), [](const TempoChange& lhs, const TempoChange& rhs) {
        if (lhs.tick != rhs.tick) return lhs.tick < rhs.tick;
        return lhs.order < rhs.order;
    });

    std::vector<TempoSegment> result;
    result.push_back({0, default_bpm, default_beat});

    for (const auto& change : changes) {
        int tick = std::max(change.tick, 0);
        double bpm = result.back().bpm;
        int beat = result.back().beat;
        if (change.has_bpm && valid_bpm(change.bpm)) bpm = change.bpm;
        if (change.has_beat && change.beat > 0) beat = change.beat;

        if (tick == result.back().tick) {
            result.back().bpm = bpm;
            result.back().beat = beat;
        } else if (bpm != result.back().bpm || beat != result.back().beat) {
            result.push_back({tick, bpm, beat});
        }
    }

    return result;
}

inline double tick_to_seconds(int tick, const std::vector<TempoSegment>& segments, int resolution = 480) {
    if (segments.empty() || resolution <= 0) return 0.0;
    if (tick <= 0) return 0.0;

    double seconds = 0.0;
    int previous_tick = 0;
    double bpm = segments.front().bpm;

    for (size_t i = 1; i < segments.size(); i++) {
        const auto& segment = segments[i];
        if (segment.tick >= tick) break;
        seconds += static_cast<double>(segment.tick - previous_tick) * 60.0 /
                   (bpm * static_cast<double>(resolution));
        previous_tick = segment.tick;
        bpm = segment.bpm;
    }

    seconds += static_cast<double>(tick - previous_tick) * 60.0 /
               (bpm * static_cast<double>(resolution));
    return seconds;
}

inline void populate_tempo_map(ObjDict& objdict, const std::vector<TempoChange>& changes,
                               double default_bpm, int default_beat, int resolution = 480) {
    auto segments = normalize_tempo_changes(changes, default_bpm, default_beat);
    objdict.bpm = segments.front().bpm;
    objdict.tempo_map.clear();
    for (const auto& segment : segments) {
        objdict.tempo_map.push_back({
            tick_to_seconds(segment.tick, segments, resolution),
            segment.bpm,
            segment.beat,
        });
    }
}

inline void initialize_note_arrays(ObjDict& objdict) {
    objdict = ObjDict{};
    objdict.pos = {-1.0};
    objdict.length = {-1.0};
    objdict.loop = {-1};
    objdict.soffs = {-1.0};
    objdict.pitch = {-99.9};
    objdict.playrate = {-1.0};
    objdict.fileidx = {-1};
    objdict.filetype = {""};
    objdict.midi_note = {-1};
    objdict.midi_lyric = {""};
    objdict.midi_len = {-1.0};
    objdict.midi_volume = {-1.0};
    objdict.midi_pan = {-1.0};
    objdict.midi_pitch_bend = {0.0};
}

inline void append_note(ObjDict& objdict, const RawNote& note, double position_sec,
                        double length_sec, const char* source_type) {
    objdict.pos.push_back(position_sec);
    objdict.length.push_back(length_sec);
    objdict.loop.push_back(0);
    objdict.soffs.push_back(0.0);
    objdict.pitch.push_back(note.note_num + note.tuning / 100.0 - 69.0);
    objdict.playrate.push_back(1.0);
    objdict.fileidx.push_back(-1);
    objdict.filetype.push_back(source_type);
    objdict.midi_note.push_back(static_cast<int>(std::lround(note.note_num)));
    objdict.midi_lyric.push_back(note.lyric);
    objdict.midi_len.push_back(length_sec);
    objdict.midi_volume.push_back(note.velocity);
    objdict.midi_pan.push_back(-1.0);
    objdict.midi_pitch_bend.push_back(0.0);
}

inline void append_separator(ObjDict& objdict) {
    objdict.pos.push_back(-1.0);
    objdict.length.push_back(-1.0);
    objdict.loop.push_back(-1);
    objdict.soffs.push_back(-1.0);
    objdict.pitch.push_back(-99.9);
    objdict.playrate.push_back(0.0);
    objdict.fileidx.push_back(-1);
    objdict.filetype.push_back("");
    objdict.midi_note.push_back(-1);
    objdict.midi_lyric.push_back("");
    objdict.midi_len.push_back(-1.0);
    objdict.midi_volume.push_back(-1.0);
    objdict.midi_pan.push_back(-1.0);
    objdict.midi_pitch_bend.push_back(0.0);
}

} // namespace autozwj_ust
