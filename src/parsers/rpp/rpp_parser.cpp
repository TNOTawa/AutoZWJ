#include "rpp_parser.h"
#include "host/host_context.h"
#include "codec/codec.h"
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>

#ifdef ERROR
#undef ERROR
#endif

static std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && (unsigned char)s[start] <= 0x20) start++;
    size_t end = s.size();
    while (end > start && (unsigned char)s[end - 1] <= 0x20) end--;
    return s.substr(start, end - start);
}

static bool starts_with(const std::string& s, const char* prefix) {
    size_t len = strlen(prefix);
    return s.size() >= len && s.compare(0, len, prefix) == 0;
}

static bool ends_with_str(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static std::vector<std::string> split_line(const std::string& line) {
    std::vector<std::string> parts;
    std::istringstream iss(line);
    std::string part;
    while (iss >> part) parts.push_back(part);
    return parts;
}

static std::string dirname(const std::string& path) {
    size_t pos = path.find_last_of("\\/");
    if (pos == std::string::npos) return "";
    return path.substr(0, pos);
}

static std::string make_absolute(const std::string& rpp_dir, const std::string& src) {
    if (src.size() >= 2 && src[1] == ':') return src;
    if (src.size() >= 2 && src[0] == '\\' && src[1] == '\\') return src;
    return rpp_dir + "\\" + src;
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

static std::vector<std::string> read_lines(const std::string& path) {
    std::vector<std::string> lines;
    std::string buf = read_text_file(path);
    if (buf.empty()) return lines;
    std::istringstream iss(buf);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

static std::map<std::string, std::string> source_type_map = {
    {"VIDEO", "VIDEO"},
    {"WAVE", "AUDIO"},
    {"MP3", "AUDIO"},
    {"VORBIS", "AUDIO"},
    {"FLAC", "AUDIO"},
    {"MIDI", "XMIDI"},
};

struct ItemDict {
    std::map<std::string, std::vector<std::string>> data;
    RppXmidiSource midi;
};

struct RppMidiNote {
    uint64_t start_tick = 0;
    uint64_t end_tick = 0;
    size_t order = 0;
    bool has_end = false;
    int note = 0;
    double velocity = 0.0;
    double volume = -1.0;
    double pan = -1.0;
    double pitch_bend = 0.0;
    double pos_sec = 0.0;
    double length_sec = 0.0;
};

struct PendingRppMidiNote {
    uint64_t start_tick = 0;
    size_t order = 0;
    uint8_t velocity = 0;
    double volume = -1.0;
    double pan = -1.0;
    double pitch_bend = 0.0;
};

static bool parse_unsigned_token(const std::string& token, int base, uint64_t& value) {
    if (token.empty()) return false;
    char* end = nullptr;
    unsigned long long parsed = std::strtoull(token.c_str(), &end, base);
    if (end == token.c_str() || *end != '\0') return false;
    value = static_cast<uint64_t>(parsed);
    return true;
}

static void parse_rpp_midi_line(const std::vector<std::string>& parts,
                                RppXmidiSource& source, uint64_t& current_tick) {
    if (parts.empty()) return;

    if (parts[0] == "HASDATA" && parts.size() >= 3) {
        uint64_t ppq = 0;
        if (parse_unsigned_token(parts[2], 10, ppq) && ppq > 0 && ppq <= 1000000) {
            source.ppq = static_cast<uint32_t>(ppq);
        }
        return;
    }

    if (parts[0] == "IGNTEMPO" && parts.size() >= 3) {
        source.ignore_tempo = std::atoi(parts[1].c_str()) != 0;
        source.tempo_bpm = std::atof(parts[2].c_str());
        if (parts.size() >= 4) source.tempo_beat = std::atoi(parts[3].c_str());
        if (source.tempo_bpm <= 0.0) source.tempo_bpm = 120.0;
        if (source.tempo_beat <= 0) source.tempo_beat = 4;
        return;
    }

    if (parts[0] != "E" && parts[0] != "e") return;
    if (parts.size() < 5) return;

    uint64_t delta = 0;
    uint64_t status = 0;
    uint64_t data1 = 0;
    uint64_t data2 = 0;
    if (!parse_unsigned_token(parts[1], 10, delta) ||
        !parse_unsigned_token(parts[2], 16, status) ||
        !parse_unsigned_token(parts[3], 16, data1) ||
        !parse_unsigned_token(parts[4], 16, data2) ||
        status > 0xFF || data1 > 0xFF || data2 > 0xFF) {
        return;
    }

    current_tick += delta;
    source.events.push_back({current_tick,
        static_cast<uint8_t>(status), static_cast<uint8_t>(data1), static_cast<uint8_t>(data2)});
}

static void scan_rpp_tempo(const std::vector<std::string>& lines,
                           double& header_bpm, int& header_beat,
                           std::vector<TempoPoint>& pt_events) {
    for (size_t i = 0; i < lines.size(); i++) {
        auto parts = split_line(lines[i]);
        if (!parts.empty() && parts[0] == "TEMPO" && parts.size() >= 3) {
            header_bpm = std::atof(parts[1].c_str());
            header_beat = std::atoi(parts[2].c_str());
            if (header_beat <= 0) header_beat = 4;
        }

        if (!parts.empty() && parts[0] == "<TEMPOENVEX") {
            for (size_t j = i + 1; j < lines.size(); j++) {
                auto tparts = split_line(lines[j]);
                if (tparts.empty() || tparts[0] == ">") {
                    i = j;
                    break;
                }
                if (tparts[0] == "PT" && tparts.size() >= 3) {
                    double pt_pos = std::atof(tparts[1].c_str());
                    double pt_bpm = std::atof(tparts[2].c_str());
                    if (pt_bpm > 0.0) {
                        pt_events.push_back({pt_pos, pt_bpm, header_beat});
                    }
                }
            }
        }
    }
}

static std::vector<TempoPoint> build_rpp_tempo_map(double header_bpm, int header_beat,
                                                   std::vector<TempoPoint> pt_events) {
    std::sort(pt_events.begin(), pt_events.end(),
        [](const TempoPoint& a, const TempoPoint& b) { return a.time_sec < b.time_sec; });

    std::vector<TempoPoint> result;
    result.push_back({0.0, header_bpm > 0.0 ? header_bpm : 120.0, header_beat > 0 ? header_beat : 4});
    for (const auto& pt : pt_events) {
        if (!result.empty() && result.back().time_sec == pt.time_sec) {
            result.back() = pt;
        } else {
            result.push_back(pt);
        }
    }
    return result;
}

static double seconds_after_beats(double start_sec, double beats,
                                  const std::vector<TempoPoint>& tempo_map) {
    if (beats <= 0.0) return start_sec;

    double current_sec = start_sec;
    double remaining_beats = beats;
    size_t segment = 0;
    while (segment + 1 < tempo_map.size() && tempo_map[segment + 1].time_sec <= current_sec) {
        segment++;
    }

    while (remaining_beats > 0.0) {
        double bpm = (segment < tempo_map.size() && tempo_map[segment].bpm > 0.0)
            ? tempo_map[segment].bpm : 120.0;
        double seconds_per_beat = 60.0 / bpm;
        double next_change = (segment + 1 < tempo_map.size())
            ? tempo_map[segment + 1].time_sec : std::numeric_limits<double>::infinity();
        double available_sec = next_change - current_sec;
        double available_beats = available_sec > 0.0
            ? available_sec / seconds_per_beat : 0.0;

        if (segment + 1 >= tempo_map.size() || available_beats >= remaining_beats) {
            return current_sec + remaining_beats * seconds_per_beat;
        }

        remaining_beats -= available_beats;
        current_sec = next_change;
        segment++;
    }
    return current_sec;
}

static bool get_item_scalar(const ItemDict& itemdict, const char* name, double& value) {
    auto exact = itemdict.data.find(name);
    if (exact != itemdict.data.end() && !exact->second.empty()) {
        value = std::atof(exact->second[0].c_str());
        return true;
    }

    std::string suffix = "/" + std::string(name);
    for (const auto& entry : itemdict.data) {
        if (entry.first.size() >= suffix.size() &&
            entry.first.compare(entry.first.size() - suffix.size(), suffix.size(), suffix) == 0 &&
            !entry.second.empty()) {
            value = std::atof(entry.second[0].c_str());
            return true;
        }
    }
    return false;
}

static std::vector<RppMidiNote> decode_rpp_midi_notes(
    const RppXmidiSource& source, double item_position, double item_length,
    double source_offset_sec, double playrate,
    const std::vector<TempoPoint>& tempo_map) {
    std::array<std::array<std::deque<PendingRppMidiNote>, 128>, 16> pending;
    double volume[16];
    double pan[16];
    double pitch_bend[16];
    for (int channel = 0; channel < 16; channel++) {
        volume[channel] = -1.0;
        pan[channel] = -1.0;
        pitch_bend[channel] = 0.0;
    }

    std::vector<RppMidiNote> notes;
    size_t order = 0;
    for (const auto& event : source.events) {
        uint8_t type = static_cast<uint8_t>((event.status >> 4) & 0x0F);
        uint8_t channel = static_cast<uint8_t>(event.status & 0x0F);

        if (type == 0x9 && event.data2 > 0) {
            pending[channel][event.data1].push_back({
                event.tick, order++, event.data2,
                volume[channel], pan[channel], pitch_bend[channel]});
        } else if (type == 0x8 || (type == 0x9 && event.data2 == 0)) {
            auto& queue = pending[channel][event.data1];
            if (!queue.empty()) {
                auto note = queue.front();
                queue.pop_front();
                notes.push_back({note.start_tick, event.tick, note.order, true,
                    static_cast<int>(event.data1), static_cast<double>(note.velocity),
                    note.volume, note.pan, note.pitch_bend});
            }
        } else if (type == 0xB) {
            if (event.data1 == 7) volume[channel] = static_cast<double>(event.data2);
            else if (event.data1 == 10) pan[channel] = static_cast<double>(event.data2);
        } else if (type == 0xE) {
            int raw = (static_cast<int>(event.data2) << 7) | static_cast<int>(event.data1);
            pitch_bend[channel] = static_cast<double>(raw - 8192);
        }
    }

    for (int channel = 0; channel < 16; channel++) {
        for (int note_number = 0; note_number < 128; note_number++) {
            auto& queue = pending[channel][note_number];
            while (!queue.empty()) {
                auto note = queue.front();
                queue.pop_front();
                notes.push_back({note.start_tick, 0, note.order, false,
                    note_number, static_cast<double>(note.velocity),
                    note.volume, note.pan, note.pitch_bend});
            }
        }
    }

    std::stable_sort(notes.begin(), notes.end(), [](const RppMidiNote& a, const RppMidiNote& b) {
        return a.order < b.order;
    });

    std::vector<TempoPoint> local_tempo_map;
    const std::vector<TempoPoint>* active_tempo_map = &tempo_map;
    if (source.ignore_tempo) {
        local_tempo_map.push_back({0.0, source.tempo_bpm, source.tempo_beat});
        active_tempo_map = &local_tempo_map;
    }

    double rate = std::abs(playrate) > 1e-9 ? std::abs(playrate) : 1.0;
    double item_end = item_position + std::max(item_length, 0.0);
    double source_item_end = source_offset_sec +
        std::max(item_length, 0.0) * rate;
    for (auto& note : notes) {
        double start_source_sec = seconds_after_beats(
            0.0, static_cast<double>(note.start_tick) / source.ppq, *active_tempo_map);
        double end_source_sec = note.has_end
            ? seconds_after_beats(0.0, static_cast<double>(note.end_tick) / source.ppq, *active_tempo_map)
            : source_item_end;

        note.pos_sec = item_position +
            (start_source_sec - source_offset_sec) / rate;
        double end_sec = item_position +
            (end_source_sec - source_offset_sec) / rate;
        if (end_sec <= item_position || note.pos_sec >= item_end) {
            note.length_sec = 0.0;
            continue;
        }
        if (note.pos_sec < item_position) note.pos_sec = item_position;
        if (end_sec > item_end) end_sec = item_end;
        note.length_sec = end_sec - note.pos_sec;
        if (note.length_sec <= 0.0) note.length_sec = 0.5;
    }

    std::stable_sort(notes.begin(), notes.end(), [](const RppMidiNote& a, const RppMidiNote& b) {
        if (a.pos_sec != b.pos_sec) return a.pos_sec < b.pos_sec;
        return a.order < b.order;
    });
    return notes;
}

static int find_file_index(const std::string& path, std::vector<std::string>& file_paths) {
    for (size_t i = 0; i < file_paths.size(); i++) {
        if (file_paths[i] == path) return (int)i;
    }
    file_paths.push_back(path);
    return (int)file_paths.size() - 1;
}

bool parse_rpp(const std::string& path, ObjDict& objdict,
               std::vector<TrackNode>& tracks, std::vector<std::string>& file_paths,
               EndWarnings& warnings,
               double start_pos, double end_pos,
               const std::vector<int>* sel_tracks,
               RppParseMetadata* metadata) {
    auto lines = read_lines(path);
    if (lines.empty()) return false;

    if (metadata) {
        metadata->start_pos_sec = start_pos;
        metadata->xmidi_items.clear();
    }

    objdict = ObjDict{};
    objdict.pos = {-1.0};
    objdict.length = {-1.0};
    objdict.loop = {-1};
    objdict.soffs = {-1.0};
    objdict.pitch = {-99.9};
    objdict.playrate = {-1.0};
    objdict.fileidx = {-1};
    objdict.filetype = {""};
    file_paths.clear();
    tracks.clear();

    std::string rpp_dir = dirname(path);

    int track_index = 0;
    std::string track_name;
    int index = 0;
    int num_lines = (int)lines.size();
    int current_depth = 0;

    double header_bpm = 120.0;
    int header_beat = 4;
    std::vector<TempoPoint> rpp_pt_events;
    scan_rpp_tempo(lines, header_bpm, header_beat, rpp_pt_events);
    const std::vector<TempoPoint> rpp_tempo_map = build_rpp_tempo_map(
        header_bpm, header_beat, rpp_pt_events);

    while (index < num_lines) {
        auto parts = split_line(lines[index]);

        if (!parts.empty() && parts[0] == "<TRACK") {
            if (index + 1 < num_lines) {
                std::string name_line = trim(lines[index + 1]);
                if (starts_with(name_line, "NAME")) {
                    track_name = trim(name_line.substr(4));
                    if (!track_name.empty() && track_name.front() == '"' && track_name.back() == '"')
                        track_name = track_name.substr(1, track_name.size() - 2);
                }
            }
            track_index++;

            if (track_index > 1 || objdict.pos.back() != -1.0) {
                objdict.pos.push_back(-1.0);
                objdict.length.push_back(-1.0);
                objdict.loop.push_back(-1);
                objdict.soffs.push_back(-1.0);
                objdict.pitch.push_back(-99.9);
                objdict.playrate.push_back(0);
                objdict.fileidx.push_back(-1);
                objdict.filetype.push_back("");
            }

            TrackNode node;
            node.name = utf8_to_wide(maybe_cp932_to_utf8(track_name));
            node.number = track_index;
            node.index = (int)objdict.pos.size();
            node.selected = true;
            node.depth = current_depth;

            bool is_folder_start = false;
            bool is_folder_end = false;
            for (int k = index + 2; k < index + 14 && k < num_lines; k++) {
                auto sp = split_line(lines[k]);
                if (sp.size() >= 2 && sp[0] == "ISBUS") {
                    if (sp[1] == "1") { is_folder_start = true; }
                    else if (sp[1] == "2") { is_folder_end = true; }
                    break;
                }
            }
            node.is_bus = is_folder_start;

            if (is_folder_start) {
                current_depth++;
            } else if (is_folder_end && current_depth > 0) {
                current_depth--;
            }

            if (sel_tracks && !sel_tracks->empty()) {
                node.selected = std::find(sel_tracks->begin(), sel_tracks->end(), track_index) != sel_tracks->end();
            }
            tracks.push_back(node);
        }

        bool track_selected = !sel_tracks || sel_tracks->empty() ||
            std::find(sel_tracks->begin(), sel_tracks->end(), track_index) != sel_tracks->end();

        if (track_selected && !parts.empty() && parts[0] == "<ITEM") {
            ItemDict itemdict;
            std::string prefix;
            int item_lyr = 1;
            int midi_source_depth = -1;
            uint64_t midi_tick = 0;
            index++;

            while (item_lyr > 0 && index < num_lines) {
                auto iparts = split_line(lines[index]);

                if (!iparts.empty() && starts_with(iparts[0], "<")) {
                    size_t bracket = iparts[0].find("<");
                    prefix += iparts[0].substr(bracket + 1);
                    if (!prefix.empty() && prefix.back() != '/') prefix += "/";
                    item_lyr++;
                    if (iparts[0] == "<SOURCE" && iparts.size() >= 2 && iparts[1] == "MIDI") {
                        itemdict.midi.present = true;
                        midi_source_depth = item_lyr;
                        midi_tick = 0;
                    }
                } else if (!iparts.empty() && iparts[0] == ">") {
                    if (midi_source_depth == item_lyr) {
                        midi_source_depth = -1;
                    }
                    size_t slash = prefix.find("/");
                    if (slash != std::string::npos && slash != prefix.size() - 1)
                        prefix = prefix.substr(slash + 1);
                    else
                        prefix = "";
                    item_lyr--;
                } else if (!iparts.empty()) {
                    if (midi_source_depth >= 0) {
                        parse_rpp_midi_line(iparts, itemdict.midi, midi_tick);
                    }
                    std::string key = prefix + iparts[0];

                    std::vector<std::string> values(iparts.begin() + 1, iparts.end());

                    if (ends_with_str(key, "FILE")) {
                        size_t pos = lines[index].find("FILE");
                        if (pos != std::string::npos) {
                            std::string file_path = lines[index].substr(pos + 4);
                            file_path = trim(file_path);
                            if (!file_path.empty() && file_path.front() == '"' && file_path.back() == '"')
                                file_path = file_path.substr(1, file_path.size() - 2);
                            if (ends_with_str(file_path, " 1"))
                                file_path = file_path.substr(0, file_path.size() - 2);
                            values = {file_path};
                        }
                    }

                    if (ends_with_str(key, "RESOURCEFN")) {
                        size_t pos = lines[index].find("RESOURCEFN");
                        if (pos != std::string::npos) {
                            std::string fn = lines[index].substr(pos + 10);
                            fn = trim(fn);
                            if (!fn.empty() && fn.front() == '"' && fn.back() == '"')
                                fn = fn.substr(1, fn.size() - 2);
                            values = {fn};
                        }
                    }

                    if (starts_with(key, "NOTES/|")) {
                        key = "NOTES";
                        std::string existing = itemdict.data.count("NOTES") && !itemdict.data["NOTES"].empty()
                            ? itemdict.data["NOTES"][0] : "";
                        size_t pipe = lines[index].find("|");
                        if (pipe != std::string::npos)
                            existing += lines[index].substr(pipe + 1);
                        values = {existing};
                    }

                    itemdict.data[key] = values;
                }

                index++;
            }

            index -= 2;

            if (!itemdict.data.count("POSITION") || itemdict.data["POSITION"].empty())
                continue;
            double position = std::atof(itemdict.data["POSITION"][0].c_str());
            if (position < start_pos || position >= end_pos)
                continue;

            size_t object_begin = objdict.pos.size();
            objdict.pos.push_back(position - start_pos);

            if (itemdict.data.count("LENGTH") && !itemdict.data["LENGTH"].empty())
                objdict.length.push_back(std::atof(itemdict.data["LENGTH"][0].c_str()));
            else
                objdict.length.push_back(0);

            if (itemdict.data.count("PLAYRATE") && itemdict.data["PLAYRATE"].size() >= 3)
                objdict.pitch.push_back(std::atof(itemdict.data["PLAYRATE"][2].c_str()));
            else
                objdict.pitch.push_back(-99.9);

            if (itemdict.data.count("LOOP") && !itemdict.data["LOOP"].empty())
                objdict.loop.push_back(std::atoi(itemdict.data["LOOP"][0].c_str()));
            else
                objdict.loop.push_back(0);

            if (itemdict.data.count("SOFFS") && !itemdict.data["SOFFS"].empty())
                objdict.soffs.push_back(std::atof(itemdict.data["SOFFS"][0].c_str()));
            else
                objdict.soffs.push_back(0.0);

            double playrate_val = 1.0;
            if (itemdict.data.count("PLAYRATE") && itemdict.data["PLAYRATE"].size() >= 1)
                playrate_val = std::atof(itemdict.data["PLAYRATE"][0].c_str());

            bool reverse = false;
            if (itemdict.data.count("SOURCE SECTION/MODE") && !itemdict.data["SOURCE SECTION/MODE"].empty()) {
                int mode = std::atoi(itemdict.data["SOURCE SECTION/MODE"][0].c_str());
                if (mode >= 2) reverse = true;
            }
            if (reverse) playrate_val = -std::abs(playrate_val);
            objdict.playrate.push_back(playrate_val);

            bool source_found = false;
            if (itemdict.midi.present) {
                objdict.fileidx.push_back(-1);
                objdict.filetype.push_back("XMIDI");
                source_found = true;
            }
            for (auto& src_pair : source_type_map) {
                if (source_found) break;
                const std::string& src_name = src_pair.first;
                const std::string& src_type = src_pair.second;

                std::string sec_key = "SOURCE SECTION/SOURCE " + src_name + "/FILE";
                std::string plain_key = "SOURCE " + src_name + "/FILE";

                if (itemdict.data.count(sec_key) && !itemdict.data[sec_key].empty()) {
                    std::string fpath = make_absolute(rpp_dir, itemdict.data[sec_key].back());
                    objdict.fileidx.push_back(find_file_index(fpath, file_paths));
                    objdict.filetype.push_back(src_type);
                    source_found = true;
                    break;
                } else if (itemdict.data.count(plain_key) && !itemdict.data[plain_key].empty()) {
                    std::string fpath = make_absolute(rpp_dir, itemdict.data[plain_key].back());
                    objdict.fileidx.push_back(find_file_index(fpath, file_paths));
                    objdict.filetype.push_back(src_type);
                    source_found = true;
                    break;
                }
            }

            if (!source_found) {
                if (itemdict.data.count("NOTES") && !itemdict.data["NOTES"].empty()) {
                    objdict.fileidx.push_back(-1);
                    objdict.filetype.push_back("TEXT:" + itemdict.data["NOTES"][0]);
                } else if (itemdict.data.count("RESOURCEFN") && !itemdict.data["RESOURCEFN"].empty()) {
                    std::string fpath = make_absolute(rpp_dir, itemdict.data["RESOURCEFN"].back());
                    objdict.fileidx.push_back(find_file_index(fpath, file_paths));
                    objdict.filetype.push_back("IMAGE");
                } else {
                    objdict.fileidx.push_back(-1);
                    objdict.filetype.push_back("OTHER");
                }
            }

            bool is_loop = itemdict.data.count("LOOP") && !itemdict.data["LOOP"].empty() &&
                           itemdict.data["LOOP"][0] == "1";
            bool has_section = itemdict.data.count("SOURCE SECTION/LENGTH") && !itemdict.data["SOURCE SECTION/LENGTH"].empty();
            bool is_mode3 = itemdict.data.count("SOURCE SECTION/MODE") && !itemdict.data["SOURCE SECTION/MODE"].empty() &&
                            itemdict.data["SOURCE SECTION/MODE"][0] == "3";

            if (is_loop && has_section && !is_mode3) {
                double item_length = objdict.length.back();
                double sec_length = std::atof(itemdict.data["SOURCE SECTION/LENGTH"][0].c_str()) / std::abs(playrate_val);
                double startpos = 0.0;
                if (itemdict.data.count("SOURCE SECTION/STARTPOS") && !itemdict.data["SOURCE SECTION/STARTPOS"].empty())
                    startpos = std::atof(itemdict.data["SOURCE SECTION/STARTPOS"][0].c_str());

                objdict.soffs.back() = startpos;
                objdict.loop.back() = 0;

                int sec_count = 1;

                while (sec_length * sec_count - item_length < -0.001 && sec_length > 0.001) {
                    if (sec_count > 1) {
                        objdict.length.back() = sec_length;
                        objdict.pos.push_back(objdict.pos.back() + sec_length);
                        objdict.length.push_back(-1);
                        objdict.loop.push_back(0);
                        objdict.soffs.push_back(startpos);
                        objdict.pitch.push_back(objdict.pitch.back());
                        objdict.playrate.push_back(playrate_val);
                        objdict.fileidx.push_back(objdict.fileidx.back());
                        objdict.filetype.push_back(objdict.filetype.back());
                    }
                    sec_count++;
                }

                objdict.length.back() = sec_length * sec_count - item_length;
            }

            if (metadata && itemdict.midi.present) {
                double source_offset_sec = 0.0;
                get_item_scalar(itemdict, "SOFFS", source_offset_sec);
                for (size_t object_index = object_begin;
                     object_index < objdict.pos.size(); object_index++) {
                    if (objdict.pos[object_index] < -0.5) continue;
                    RppXmidiItem xmidi_item;
                    xmidi_item.object_index = object_index;
                    xmidi_item.item_position_sec = objdict.pos[object_index] + start_pos;
                    xmidi_item.source_offset_sec = source_offset_sec;
                    xmidi_item.source = itemdict.midi;
                    metadata->xmidi_items.push_back(std::move(xmidi_item));
                }
            }

            if (itemdict.data.count("SM"))
                warnings.exist_stretch_marker.push_back("Track: " + track_name +
                    " / Position: " + std::to_string((int)(position * 1000) / 1000.0));
        }

        index++;
    }

    objdict.track_count = track_index;

    // 构建 tempo_map：工程头 TEMPO 提供初始 BPM/拍号，TEMPOENVEX 内 PT 行提供变速点
    objdict.tempo_map = rpp_tempo_map;
    objdict.bpm = objdict.tempo_map[0].bpm;

    if (!tracks.empty()) {
        int current_track = 0;
        for (size_t i = 1; i < objdict.pos.size(); i++) {
            if (objdict.pos[i] == -1.0) {
                tracks[current_track].count = (int)i - tracks[current_track].index;
                current_track++;
                if (current_track >= (int)tracks.size()) break;
                tracks[current_track].index = (int)i + 1;
            }
        }
        if (current_track < (int)tracks.size()) {
            tracks[current_track].count = (int)objdict.pos.size() - tracks[current_track].index;
        }

        // 空轨保留，由 UI 灰色显示、生成逻辑静默跳过
    }

    return true;
}

bool parse_rpp_auto(const std::string& path, ObjDict& objdict,
                    std::vector<TrackNode>& tracks, std::vector<std::string>& file_paths,
                    EndWarnings& warnings,
                    double start_pos, double end_pos,
                    RppParseMetadata* metadata) {
    return parse_rpp(path, objdict, tracks, file_paths, warnings,
                     start_pos, end_pos, nullptr, metadata);
}

static double item_double_at(const std::vector<double>& values, size_t index, double fallback) {
    return index < values.size() ? values[index] : fallback;
}

static int item_int_at(const std::vector<int>& values, size_t index, int fallback) {
    return index < values.size() ? values[index] : fallback;
}

static std::string item_string_at(const std::vector<std::string>& values,
                                  size_t index, const std::string& fallback) {
    return index < values.size() ? values[index] : fallback;
}

static void append_objdict_sentinel(ObjDict& objdict) {
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

static void append_objdict_item(const ObjDict& source, size_t index, ObjDict& target) {
    target.pos.push_back(item_double_at(source.pos, index, -1.0));
    target.length.push_back(item_double_at(source.length, index, 0.0));
    target.loop.push_back(item_int_at(source.loop, index, 0));
    target.soffs.push_back(item_double_at(source.soffs, index, 0.0));
    target.pitch.push_back(item_double_at(source.pitch, index, -99.9));
    target.playrate.push_back(item_double_at(source.playrate, index, 1.0));
    target.fileidx.push_back(item_int_at(source.fileidx, index, -1));
    target.filetype.push_back(item_string_at(source.filetype, index, "OTHER"));
    target.midi_note.push_back(item_int_at(source.midi_note, index, -1));
    target.midi_lyric.push_back(item_string_at(source.midi_lyric, index, ""));
    target.midi_len.push_back(item_double_at(source.midi_len, index, -1.0));
    target.midi_volume.push_back(item_double_at(source.midi_volume, index, -1.0));
    target.midi_pan.push_back(item_double_at(source.midi_pan, index, -1.0));
    target.midi_pitch_bend.push_back(item_double_at(source.midi_pitch_bend, index, 0.0));
}

static void append_rpp_note_item(const RppMidiNote& note, double output_offset_sec,
                                 ObjDict& target) {
    target.pos.push_back(note.pos_sec - output_offset_sec);
    target.length.push_back(note.length_sec);
    target.loop.push_back(0);
    target.soffs.push_back(0.0);
    target.pitch.push_back(static_cast<double>(note.note - 69));
    target.playrate.push_back(1.0);
    target.fileidx.push_back(-1);
    target.filetype.push_back("XMIDI");
    target.midi_note.push_back(note.note);
    target.midi_lyric.push_back("");
    target.midi_len.push_back(note.length_sec);
    target.midi_volume.push_back(note.volume);
    target.midi_pan.push_back(note.pan);
    target.midi_pitch_bend.push_back(note.pitch_bend);
}

void expand_rpp_xmidi_items(ObjDict& objdict, std::vector<TrackNode>& tracks,
                            const RppParseMetadata& metadata) {
    if (metadata.xmidi_items.empty() || tracks.empty()) return;

    std::map<size_t, const RppXmidiItem*> xmidi_by_index;
    for (const auto& item : metadata.xmidi_items) {
        xmidi_by_index[item.object_index] = &item;
    }

    ObjDict expanded;
    expanded.filelist = objdict.filelist;
    expanded.bpm = objdict.bpm;
    expanded.tempo_map = objdict.tempo_map;
    expanded.track_count = objdict.track_count;
    append_objdict_sentinel(expanded);

    std::vector<TrackNode> expanded_tracks;
    expanded_tracks.reserve(tracks.size());

    for (size_t ti = 0; ti < tracks.size(); ti++) {
        if (ti > 0) append_objdict_sentinel(expanded);

        TrackNode node = tracks[ti];
        node.index = static_cast<int>(expanded.pos.size());
        node.count = 0;

        size_t begin = tracks[ti].index;
        size_t end = std::min(objdict.pos.size(), begin + static_cast<size_t>(std::max(tracks[ti].count, 0)));
        for (size_t index = begin; index < end; index++) {
            auto xmidi_it = xmidi_by_index.find(index);
            bool expanded_item = false;

            if (xmidi_it != xmidi_by_index.end()) {
                const auto& xmidi_item = *xmidi_it->second;
                double item_length = item_double_at(objdict.length, index, 0.0);
                double playrate = item_double_at(objdict.playrate, index, 1.0);
                auto notes = decode_rpp_midi_notes(
                    xmidi_item.source, xmidi_item.item_position_sec, item_length,
                    xmidi_item.source_offset_sec, playrate, objdict.tempo_map);

                int appended = 0;
                for (const auto& note : notes) {
                    if (note.length_sec <= 0.0) continue;
                    append_rpp_note_item(note, metadata.start_pos_sec, expanded);
                    appended++;
                }
                if (appended > 0) {
                    node.count += appended;
                    expanded_item = true;
                }
            }

            if (!expanded_item) {
                append_objdict_item(objdict, index, expanded);
                node.count++;
            }
        }

        expanded_tracks.push_back(std::move(node));
    }

    objdict = std::move(expanded);
    tracks = std::move(expanded_tracks);
}
