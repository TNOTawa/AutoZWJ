#include "tools/up/up_media_parser.h"
#include "codec/codec.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <sstream>

static std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && (unsigned char)s[start] <= 0x20) start++;
    size_t end = s.size();
    while (end > start && (unsigned char)s[end - 1] <= 0x20) end--;
    return s.substr(start, end - start);
}

static std::vector<std::string> split_line(const std::string& line) {
    std::vector<std::string> parts;
    std::istringstream iss(line);
    std::string part;
    while (iss >> part) parts.push_back(part);
    return parts;
}

static std::string upper(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = (char)std::toupper((unsigned char)c);
    return r;
}

static std::string parse_path_string(const std::vector<std::string>& tokens) {
    if (tokens.size() <= 1) return "";
    std::string full;
    for (size_t i = 1; i < tokens.size(); i++) {
        if (i > 1) full += " ";
        full += tokens[i];
    }
    if (full.size() >= 2 && full.front() == '"' && full.back() == '"')
        full = full.substr(1, full.size() - 2);
    return full;
}

static double parse_double_token(const std::string& s) {
    return std::atof(s.c_str());
}

static int parse_int_token(const std::string& s) {
    return std::atoi(s.c_str());
}

struct Block {
    std::vector<std::string> lines;
    Block* parent = nullptr;
    std::vector<std::unique_ptr<Block>> children;
    std::string type;

    Block* add_child() {
        auto child = std::make_unique<Block>();
        child->parent = this;
        Block* raw = child.get();
        children.push_back(std::move(child));
        return raw;
    }

    void compute_type() {
        type.clear();
        if (lines.empty()) return;
        const std::string& first = lines[0];
        if (first.empty() || first[0] != '<') return;
        size_t sep = first.find(' ');
        std::string token = (sep != std::string::npos) ? first.substr(0, sep) : first;
        if (!token.empty() && token[0] == '<')
            type = token.substr(1);
        for (auto& c : type) c = (char)std::toupper((unsigned char)c);
    }
};

static std::vector<std::string> split_lines(const std::vector<uint8_t>& data) {
    std::vector<std::string> lines;
    size_t start = 0;
    for (size_t i = 0; i < data.size(); i++) {
        if (data[i] == 0x00) {
            if (i > start) {
                std::string line(data.begin() + start, data.begin() + i);
                line = trim(line);
                if (!line.empty()) lines.push_back(line);
            }
            start = i + 1;
        } else if (data[i] == 0x0D && i + 1 < data.size() && data[i + 1] == 0x0A) {
            if (i > start) {
                std::string line(data.begin() + start, data.begin() + i);
                line = trim(line);
                if (!line.empty()) lines.push_back(line);
            }
            start = i + 2;
        }
    }
    if (start < data.size()) {
        std::string line(data.begin() + start, data.end());
        line = trim(line);
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}

struct SourceInfo {
    std::string file_path;
    std::string source_type;
    double section_length = 0.0;
    double section_startpos = 0.0;
    int section_mode = 0;
    bool has_section = false;
};

static const struct { std::string tag; std::string type; } source_type_map[] = {
    {"VIDEO", "VIDEO"},
    {"WAVE", "AUDIO"},
    {"MP3", "AUDIO"},
    {"VORBIS", "AUDIO"},
    {"FLAC", "AUDIO"},
};

static const int kMaxSourceDepth = 16;

static bool parse_source_block(Block* block, SourceInfo& info, int depth = 0) {
    if (block->type != "SOURCE") return false;
    // 限制嵌套深度，防止构造数据导致栈溢出
    if (depth >= kMaxSourceDepth) return false;

    info = SourceInfo{};
    std::string source_kind;

    for (const auto& line : block->lines) {
        auto tokens = split_line(line);
        if (tokens.empty()) continue;
        std::string key = upper(tokens[0]);

        if (key == "<SOURCE" && tokens.size() > 1) {
            source_kind = upper(tokens[1]);
            if (source_kind == "SECTION") {
                info.has_section = true;
            }
        } else if (key == "FILE") {
            info.file_path = maybe_cp932_to_utf8(parse_path_string(tokens));
        } else if (key == "LENGTH" && tokens.size() >= 2) {
            info.section_length = parse_double_token(tokens[1]);
        } else if (key == "MODE" && tokens.size() >= 2) {
            info.section_mode = parse_int_token(tokens[1]);
        } else if (key == "STARTPOS" && tokens.size() >= 2) {
            info.section_startpos = parse_double_token(tokens[1]);
        }
    }

    for (const auto& st : source_type_map) {
        if (source_kind == st.tag) {
            info.source_type = st.type;
            break;
        }
    }

    if (info.file_path.empty()) {
        for (const auto& child : block->children) {
            SourceInfo nested;
            if (parse_source_block(child.get(), nested, depth + 1)) {
                info.file_path = nested.file_path;
                if (info.source_type.empty()) info.source_type = nested.source_type;
                break;
            }
        }
    }

    return !info.file_path.empty();
}

static bool parse_item_block(Block* block, UpItem& item) {
    if (block->type != "ITEM") return false;

    item = UpItem{};

    for (const auto& line : block->lines) {
        auto tokens = split_line(line);
        if (tokens.empty()) continue;
        std::string key = upper(tokens[0]);

        if (key == "POSITION" && tokens.size() >= 2)
            item.position = parse_double_token(tokens[1]);
        else if (key == "LENGTH" && tokens.size() >= 2)
            item.length = parse_double_token(tokens[1]);
        else if (key == "LOOP" && tokens.size() >= 2)
            item.loop = (parse_int_token(tokens[1]) == 1);
        else if (key == "SOFFS" && tokens.size() >= 2)
            item.soffs = parse_double_token(tokens[1]);
        else if (key == "PLAYRATE" && tokens.size() >= 2)
            item.playrate = parse_double_token(tokens[1]);
    }

    SourceInfo src_info;
    for (const auto& child : block->children) {
        child->compute_type();
        if (child->type == "SOURCE") {
            if (parse_source_block(child.get(), src_info)) break;
        }
    }

    if (src_info.file_path.empty()) return false;

    item.file_path = src_info.file_path;
    item.source_type = src_info.source_type;

    // SECTION mode >= 2 表示反向播放（播放速率取负）
    if (src_info.has_section && src_info.section_mode >= 2)
        item.playrate = -std::abs(item.playrate);

    if (item.position < 0) return false;

    return true;
}

static void parse_track_block(Block* block, UpTrack& track) {
    track = UpTrack{};

    for (const auto& line : block->lines) {
        auto tokens = split_line(line);
        if (tokens.empty()) continue;
        std::string key = upper(tokens[0]);

        if (key == "NAME") {
            std::string name = maybe_cp932_to_utf8(parse_path_string(tokens));
            track.name = utf8_to_wide(name);
        }
    }

    for (const auto& child : block->children) {
        child->compute_type();
        if (child->type == "ITEM") {
            UpItem item;
            if (parse_item_block(child.get(), item)) {
                track.items.push_back(item);
            }
        }
    }
}

bool parse_reaper_media(const std::vector<uint8_t>& data, UpProject& proj) {
    auto lines = split_lines(data);
    if (lines.empty()) return false;

    proj = UpProject{};

    Block root;
    Block* current = &root;

    for (const auto& raw_line : lines) {
        if (raw_line.empty()) continue;

        std::string line = raw_line;
        auto parts = split_line(line);
        std::string upper_first;
        if (!parts.empty()) upper_first = upper(parts[0]);

        // TRACKSKIP 行不进入嵌套结构
        if (upper_first == "TRACKSKIP") {
            Block* child = current->add_child();
            child->lines.push_back(line);
            continue;
        }

        if (!line.empty() && line[0] == '<') {
            current = current->add_child();
        }

        current->lines.push_back(line);

        if (!line.empty() && line[0] == '>') {
            if (current->parent) {
                current = current->parent;
            }
        }
    }

    for (const auto& child : root.children) {
        child->compute_type();
    }

    UpTrack current_track;
    bool in_track = false;

    for (const auto& child : root.children) {
        child->compute_type();

        if (child->type == "TRACK") {
            if (in_track && !current_track.items.empty()) {
                proj.tracks.push_back(current_track);
            }
            parse_track_block(child.get(), current_track);
            in_track = true;
            continue;
        }

        if (child->type == "ITEM") {
            if (!in_track) {
                current_track = UpTrack{};
                in_track = true;
            }
            UpItem item;
            if (parse_item_block(child.get(), item)) {
                current_track.items.push_back(item);
            }
            continue;
        }

        // TRACKSKIP 作为轨道分隔标记
        if (!child->lines.empty()) {
            auto tokens = split_line(child->lines[0]);
            if (!tokens.empty() && upper(tokens[0]) == "TRACKSKIP") {
                if (in_track && !current_track.items.empty()) {
                    proj.tracks.push_back(current_track);
                }
                current_track = UpTrack{};
                in_track = true;
            }
        }
    }

    if (in_track && !current_track.items.empty()) {
        proj.tracks.push_back(current_track);
    }

    // 兜底：轨道嵌套在根级 REAPER_PROJECT 块内的情况
    if (proj.tracks.empty()) {
        for (const auto& child : root.children) {
            if (child->type.empty() || child->type == "REAPER_PROJECT") {
                for (const auto& nested : child->children) {
                    nested->compute_type();
                    current_track = UpTrack{};
                    in_track = false;
                    if (nested->type == "TRACK") {
                        parse_track_block(nested.get(), current_track);
                        in_track = true;
                    }
                    if (in_track && !current_track.items.empty()) {
                        proj.tracks.push_back(current_track);
                    }
                }
            }
        }
    }

    return !proj.tracks.empty();
}
