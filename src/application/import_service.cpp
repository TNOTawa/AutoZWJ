#include "import_service.h"
#include "codec/codec.h"
#include "parsers/rpp/rpp_parser.h"
#include "parsers/midi/midi_parser.h"
#include "parsers/lrc/lrc_parser.h"
#include "adapters/menu_registry.h"
#include <algorithm>

ParsedProject parse_source(const std::wstring& file_path) {
    std::string path_utf8 = wide_to_utf8(file_path);
    std::string lower = path_utf8;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    ParsedProject result;

    EndWarnings warnings;
    bool ok = false;
    if (lower.find(".rpp") != std::string::npos) {
        RppParseMetadata metadata;
        ok = parse_rpp_auto(path_utf8, result.objdict, result.tracks, result.file_paths,
                            warnings, 0.0, 100000.0, &metadata);
        if (ok && feature_registry_is_enabled(kFeatureParseRppXmidiNotes, false)) {
            expand_rpp_xmidi_items(result.objdict, result.tracks, metadata);
        }
    } else if (lower.find(".mid") != std::string::npos) {
        ok = parse_midi(path_utf8, result.objdict, result.tracks, result.file_paths);
    } else if (lower.find(".lrc") != std::string::npos) {
        ok = parse_lrc(path_utf8, result.objdict, result.tracks, result.file_paths);
    }

    if (!ok) {
        result.objdict = ObjDict{};
        result.tracks.clear();
        result.file_paths.clear();
    }
    return result;
}
