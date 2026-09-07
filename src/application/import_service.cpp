#include "import_service.h"
#include "codec/codec.h"
#include "parsers/rpp/rpp_parser.h"
#include "parsers/midi/midi_parser.h"
#include "parsers/ust/ust_parser.h"
#include "parsers/ustx/ustx_parser.h"
#include "parsers/lrc/lrc_parser.h"
#include <algorithm>

ParsedProject parse_source(const std::wstring& file_path) {
    std::string path_utf8 = wide_to_utf8(file_path);
    std::string lower = path_utf8;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    ParsedProject result;

    EndWarnings warnings;
    bool ok = false;
    size_t dot = lower.find_last_of('.');
    std::string extension = dot == std::string::npos ? std::string() : lower.substr(dot);

    if (extension == ".rpp") {
        ok = parse_rpp_auto(path_utf8, result.objdict, result.tracks, result.file_paths, warnings);
    } else if (extension == ".mid" || extension == ".midi") {
        ok = parse_midi(path_utf8, result.objdict, result.tracks, result.file_paths);
    } else if (extension == ".ust") {
        ok = parse_ust(path_utf8, result.objdict, result.tracks, result.file_paths);
    } else if (extension == ".ustx") {
        ok = parse_ustx(path_utf8, result.objdict, result.tracks, result.file_paths);
    } else if (extension == ".lrc") {
        ok = parse_lrc(path_utf8, result.objdict, result.tracks, result.file_paths);
    }

    if (!ok) {
        result.objdict = ObjDict{};
        result.tracks.clear();
        result.file_paths.clear();
    }
    return result;
}
