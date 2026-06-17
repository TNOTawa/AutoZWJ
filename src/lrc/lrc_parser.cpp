#include "lrc/lrc_parser.h"
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <cstring>

struct LrcLine {
    double pos_sec;
    std::string lyric;
};

static bool is_json_line(const std::string& line) {
    size_t i = 0;
    while (i < line.size() && (unsigned char)line[i] <= 0x20) i++;
    return i < line.size() && line[i] == '{';
}

static bool parse_lrc_timestamp(const std::string& line, size_t start, double& out_sec) {
    if (start >= line.size() || line[start] != '[') return false;
    size_t close = line.find(']', start);
    if (close == std::string::npos) return false;

    std::string inner = line.substr(start + 1, close - start - 1);
    if (inner.empty()) return false;

    int mm = 0, ss = 0, cs = 0;

    size_t sep1 = inner.find(':');
    if (sep1 == std::string::npos) return false;

    mm = std::atoi(inner.substr(0, sep1).c_str());

    size_t sep2 = inner.find(':', sep1 + 1);
    size_t dot2 = inner.find('.', sep1 + 1);

    if (sep2 != std::string::npos) {
        ss = std::atoi(inner.substr(sep1 + 1, sep2 - sep1 - 1).c_str());
        cs = std::atoi(inner.substr(sep2 + 1).c_str());
    } else if (dot2 != std::string::npos) {
        ss = std::atoi(inner.substr(sep1 + 1, dot2 - sep1 - 1).c_str());
        cs = std::atoi(inner.substr(dot2 + 1).c_str());
    } else {
        ss = std::atoi(inner.substr(sep1 + 1).c_str());
        cs = 0;
    }

    out_sec = mm * 60.0 + ss + cs / 100.0;
    return true;
}

static std::string read_text_file(const std::string& path) {
    std::wstring wpath = utf8_to_wide(path);
    HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return "";
    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile, &size) || size.QuadPart <= 0) { CloseHandle(hFile); return ""; }
    std::string buf((size_t)size.QuadPart, '\0');
    DWORD read = 0;
    BOOL ok = ReadFile(hFile, &buf[0], (DWORD)size.QuadPart, &read, nullptr);
    CloseHandle(hFile);
    if (!ok || read != (DWORD)size.QuadPart) return "";
    buf.resize(read);
    return buf;
}

static std::vector<LrcLine> read_lrc_lines(const std::string& path) {
    std::vector<LrcLine> result;

    std::string buf = read_text_file(path);
    if (buf.empty()) return result;

    std::istringstream iss(buf);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (is_json_line(line)) continue;

        size_t i = 0;
        std::vector<double> timestamps;
        while (i < line.size()) {
            double ts = 0;
            if (parse_lrc_timestamp(line, i, ts)) {
                timestamps.push_back(ts);
                i = line.find(']', i) + 1;
            } else {
                break;
            }
        }

        if (timestamps.empty()) continue;

        std::string lyric = line.substr(i);
        size_t lyric_start = 0;
        while (lyric_start < lyric.size() && (unsigned char)lyric[lyric_start] <= 0x20) lyric_start++;
        std::string lyric_trimmed = lyric.substr(lyric_start);

        for (double ts : timestamps) {
            result.push_back({ts, lyric_trimmed});
        }
    }

    std::sort(result.begin(), result.end(), [](const LrcLine& a, const LrcLine& b) {
        return a.pos_sec < b.pos_sec;
    });

    return result;
}

bool parse_lrc(const std::string& path, ObjDict& objdict,
               std::vector<TrackNode>& tracks, std::vector<std::string>& file_paths) {
    auto lines = read_lrc_lines(path);
    if (lines.empty()) return false;

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
    file_paths.clear();
    tracks.clear();

    objdict.bpm = 0.0;
    objdict.track_count = 1;

    TrackNode node;
    node.name = L"Lyrics";
    node.number = 1;
    node.index = (int)objdict.pos.size();
    node.selected = true;

    const double DEFAULT_LAST_DURATION = 2.0;

    for (size_t i = 0; i < lines.size(); i++) {
        objdict.pos.push_back(lines[i].pos_sec);

        double duration = DEFAULT_LAST_DURATION;
        if (i + 1 < lines.size()) {
            duration = lines[i + 1].pos_sec - lines[i].pos_sec;
            if (duration <= 0.0) duration = DEFAULT_LAST_DURATION;
        }
        objdict.length.push_back(duration);

        objdict.loop.push_back(0);
        objdict.soffs.push_back(0.0);
        objdict.pitch.push_back(-99.9);
        objdict.playrate.push_back(1.0);
        objdict.fileidx.push_back(-1);
        objdict.filetype.push_back("LRC");
        objdict.midi_note.push_back(-1);
        objdict.midi_lyric.push_back(lines[i].lyric);
        objdict.midi_len.push_back(duration);
        objdict.midi_volume.push_back(-1.0);
        objdict.midi_pan.push_back(-1.0);
        objdict.midi_pitch_bend.push_back(0.0);
    }

    node.count = (int)objdict.pos.size() - node.index;
    tracks.push_back(node);

    return true;
}
