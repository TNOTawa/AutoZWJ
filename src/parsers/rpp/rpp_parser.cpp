#include "rpp_parser.h"
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>
#include <cstdlib>
#include <cstring>

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

static std::string to_lower(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = (char)std::tolower((unsigned char)c);
    return r;
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
};

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
               const std::vector<int>* sel_tracks) {
    auto lines = read_lines(path);
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

    while (index < num_lines) {
        auto parts = split_line(lines[index]);

        if (!parts.empty() && parts[0] == "TEMPO" && parts.size() >= 3) {
            header_bpm = std::atof(parts[1].c_str());
            header_beat = std::atoi(parts[2].c_str());
            if (header_beat <= 0) header_beat = 4;
        }

        if (!parts.empty() && parts[0] == "<TEMPOENVEX") {
            index++;
            while (index < num_lines) {
                auto tparts = split_line(lines[index]);
                if (tparts.empty() || tparts[0] == ">") break;
                if (tparts[0] == "PT" && tparts.size() >= 3) {
                    double pt_pos = std::atof(tparts[1].c_str());
                    double pt_bpm = std::atof(tparts[2].c_str());
                    if (pt_bpm > 0.0) {
                        rpp_pt_events.push_back({pt_pos, pt_bpm, header_beat});
                    }
                }
                index++;
            }
        }

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
            index++;

            while (item_lyr > 0 && index < num_lines) {
                auto iparts = split_line(lines[index]);

                if (!iparts.empty() && starts_with(iparts[0], "<")) {
                    size_t bracket = iparts[0].find("<");
                    prefix += iparts[0].substr(bracket + 1);
                    if (!prefix.empty() && prefix.back() != '/') prefix += "/";
                    item_lyr++;
                } else if (!iparts.empty() && iparts[0] == ">") {
                    size_t slash = prefix.find("/");
                    if (slash != std::string::npos && slash != prefix.size() - 1)
                        prefix = prefix.substr(slash + 1);
                    else
                        prefix = "";
                    item_lyr--;
                } else if (!iparts.empty()) {
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
            for (auto& src_pair : source_type_map) {
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
                double last_len = item_length;

                while (sec_length * sec_count - item_length < -0.001 && sec_length > 0.001) {
                    last_len = sec_length;
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

            if (itemdict.data.count("SM"))
                warnings.exist_stretch_marker.push_back("Track: " + track_name +
                    " / Position: " + std::to_string((int)(position * 1000) / 1000.0));
        }

        index++;
    }

    objdict.track_count = track_index;

    // 构建 tempo_map：工程头 TEMPO 提供初始 BPM/拍号，TEMPOENVEX 内 PT 行提供变速点
    {
        std::sort(rpp_pt_events.begin(), rpp_pt_events.end(),
            [](const TempoPoint& a, const TempoPoint& b) { return a.time_sec < b.time_sec; });

        objdict.tempo_map.push_back({0.0, header_bpm, header_beat});
        for (const auto& pt : rpp_pt_events) {
            if (!objdict.tempo_map.empty() && objdict.tempo_map.back().time_sec == pt.time_sec) {
                objdict.tempo_map.back() = pt;
            } else {
                objdict.tempo_map.push_back(pt);
            }
        }
        objdict.bpm = objdict.tempo_map[0].bpm;
    }

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
                    double start_pos, double end_pos) {
    return parse_rpp(path, objdict, tracks, file_paths, warnings, start_pos, end_pos, nullptr);
}
