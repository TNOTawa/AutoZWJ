#include "parsers/rpp/rpp_parser.h"
#include <string>
#include <vector>
#include "codec/codec.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

static void check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static bool close_to(double a, double b, double epsilon = 1e-9) {
    return std::abs(a - b) <= epsilon;
}

static std::filesystem::path write_fixture() {
    auto path = std::filesystem::temp_directory_path() / "autozwj_rpp_xmidi_test.rpp";
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    check(file.good(), "fixture file should be writable");
    file << R"RPP(<REAPER_PROJECT 0.1 "7.0" 0
TEMPO 120 4 4
MARKER 1 1.25 "XMIDI marker" 0
<TRACK
NAME "MIDI"
ISBUS 0
<ITEM
POSITION 1
LENGTH 2
PLAYRATE 1 0 0
<SOURCE MIDI
HASDATA 1 480 QN
E 480 c0 01
E 0 90 ff 64
E 0 90 3c 64
E 480 80 3c 00
E 240 90 40 7f
E 240 80 40 00
>
>
<ITEM
POSITION 4
LENGTH 1
PLAYRATE 1 0 0
SOFFS 0.25
<SOURCE MIDI
HASDATA 1 480 QN
E 240 90 43 40
E 240 80 43 00
>
>
<ITEM
POSITION 6
LENGTH 0.5
NOTES plain
>
>
>
<TRACK
NAME "MIDI 2"
ISBUS 0
<ITEM
POSITION 2
LENGTH 1
PLAYRATE 1 0 0
<SOURCE MIDI
HASDATA 1 480 QN
IGNTEMPO 1 60 4 4
E 240 90 48 50
E 240 80 48 00
>
>
>
>
)RPP";
    file.close();
    return path;
}

static void test_midi_block_mode(const std::string& path) {
    ObjDict objdict;
    std::vector<TrackNode> tracks;
    std::vector<std::string> file_paths;
    EndWarnings warnings;

    check(parse_rpp(path, objdict, tracks, file_paths, warnings,
                    0.0, 100000.0, nullptr, nullptr),
          "RPP should parse with XMIDI block mode");
    check(tracks.size() == 2, "block mode should keep both tracks");
    check(tracks[0].count == 3 && tracks[1].count == 1,
          "block mode should keep the original item counts");
    check(objdict.pos.size() == 6, "block mode should keep four items and one separator");
    check(objdict.filetype[1] == "XMIDI", "block mode item should be XMIDI");
    check(close_to(objdict.length[1], 2.0), "block mode should keep MIDI item length");
    check(objdict.midi_note.empty(), "block mode should preserve the original MIDI metadata arrays");
}

static void test_note_mode(const std::string& path) {
    ObjDict objdict;
    std::vector<TrackNode> tracks;
    std::vector<std::string> file_paths;
    EndWarnings warnings;
    RppParseMetadata metadata;

    check(parse_rpp(path, objdict, tracks, file_paths, warnings,
                    0.0, 100000.0, nullptr, &metadata),
          "RPP should parse with XMIDI note mode");
    expand_rpp_xmidi_items(objdict, tracks, metadata);
    check(objdict.markers.size() == 1, "note expansion should preserve markers");
    check(close_to(objdict.markers[0].time_sec, 1.25), "expanded marker time mismatch");
    check(objdict.markers[0].memo == "XMIDI marker", "expanded marker memo mismatch");
    check(tracks.size() == 2, "note mode should keep both tracks");
    check(tracks[0].count == 4 && tracks[1].count == 1,
          "note mode should expand notes without losing track boundaries");
    check(objdict.pos.size() == 7, "note mode should produce five items and one separator");
    check(close_to(objdict.pos[1], 1.5), "short event tick handling mismatch");
    check(close_to(objdict.length[1], 0.5), "first note length mismatch");
    check(close_to(objdict.pos[2], 2.25), "second note position mismatch");
    check(close_to(objdict.length[2], 0.25), "second note length mismatch");
    check(objdict.midi_note[1] == 60, "first note number mismatch");
    check(objdict.midi_note[2] == 64, "second note number mismatch");
    check(close_to(objdict.pos[3], 4.0), "third note position mismatch");
    check(objdict.midi_note[3] == 67, "third note number mismatch");
    check(close_to(objdict.length[3], 0.25), "SOFFS-adjusted note length mismatch");
    check(objdict.filetype[4] == "TEXT:plain" && close_to(objdict.pos[4], 6.0),
          "non-XMIDI items should be copied unchanged");
    check(objdict.pos[5] == -1.0, "track separator should remain in the flattened data");
    check(close_to(objdict.pos[6], 2.5), "IGNTEMPO-adjusted note position mismatch");
    check(close_to(objdict.length[6], 0.5), "IGNTEMPO-adjusted note length mismatch");
    check(objdict.midi_note[6] == 72, "second track note number mismatch");
    check(objdict.filetype[1] == "XMIDI" && objdict.filetype[2] == "XMIDI",
          "note items should be XMIDI");
    check(close_to(objdict.tempo_map[0].bpm, 120.0), "tempo map BPM mismatch");
}

static int inspect_project(const std::string& path) {
    ObjDict objdict;
    std::vector<TrackNode> tracks;
    std::vector<std::string> file_paths;
    EndWarnings warnings;
    RppParseMetadata metadata;

    check(parse_rpp(path, objdict, tracks, file_paths, warnings,
                    0.0, 100000.0, nullptr, &metadata),
          "inspection RPP should parse");

    std::printf("BASE tracks=%zu objects=%zu tempo=%.6f xmidi=%zu\n",
                tracks.size(), objdict.pos.size(), objdict.bpm, metadata.xmidi_items.size());
    for (const auto& item : metadata.xmidi_items) {
        uint64_t first_tick = item.source.events.empty() ? 0 : item.source.events.front().tick;
        uint64_t last_tick = item.source.events.empty() ? 0 : item.source.events.back().tick;
        std::printf("XMIDI index=%zu pos=%.9f offset=%.9f ppq=%u events=%zu first_tick=%llu last_tick=%llu\n",
                    item.object_index, item.item_position_sec, item.source_offset_sec,
                    item.source.ppq, item.source.events.size(),
                    static_cast<unsigned long long>(first_tick),
                    static_cast<unsigned long long>(last_tick));
        int shown_note_on = 0;
        for (const auto& event : item.source.events) {
            if (((event.status >> 4) & 0x0F) != 0x9 || event.data2 == 0) continue;
            std::printf("  NOTE_ON tick=%llu note=%u velocity=%u\n",
                        static_cast<unsigned long long>(event.tick), event.data1, event.data2);
            if (++shown_note_on >= 8) break;
        }
        if (shown_note_on == 0) std::printf("  NOTE_ON none\n");
    }

    expand_rpp_xmidi_items(objdict, tracks, metadata);
    std::printf("EXPANDED tracks=%zu objects=%zu\n", tracks.size(), objdict.pos.size());
    for (size_t ti = 0; ti < tracks.size(); ti++) {
        const auto& track = tracks[ti];
        std::string track_name = wide_to_utf8(track.name);
        std::printf("TRACK %zu index=%d count=%d name=%s\n",
                    ti, track.index, track.count, track_name.c_str());
        size_t end = std::min(objdict.pos.size(), static_cast<size_t>(track.index + track.count));
        size_t shown = 0;
        for (size_t i = track.index; i < end; i++) {
            if (objdict.pos[i] < -0.5) continue;
            if (shown >= 8 && i + 3 < end) continue;
            int note = i < objdict.midi_note.size() ? objdict.midi_note[i] : -1;
            std::printf("  ITEM %zu pos=%.9f len=%.9f pitch=%.3f note=%d type=%s\n",
                        i, objdict.pos[i], objdict.length[i], objdict.pitch[i], note,
                        i < objdict.filetype.size() ? objdict.filetype[i].c_str() : "");
            shown++;
        }
    }
    return 0;
}

static void test_duida_midi2item_equivalence(const std::string& path) {
    ObjDict objdict;
    std::vector<TrackNode> tracks;
    std::vector<std::string> file_paths;
    EndWarnings warnings;
    RppParseMetadata metadata;

    check(parse_rpp(path, objdict, tracks, file_paths, warnings,
                    0.0, 100000.0, nullptr, &metadata),
          "duida project should parse");
    check(!metadata.xmidi_items.empty(), "duida project should contain XMIDI items");

    size_t source_track = tracks.size();
    const size_t source_index = metadata.xmidi_items.front().object_index;
    for (size_t ti = 0; ti < tracks.size(); ti++) {
        size_t begin = tracks[ti].index;
        size_t end = begin + static_cast<size_t>(std::max(tracks[ti].count, 0));
        if (source_index >= begin && source_index < end) {
            source_track = ti;
            break;
        }
    }
    check(source_track + 1 < tracks.size(), "duida MIDI2Item comparison track should exist");

    expand_rpp_xmidi_items(objdict, tracks, metadata);

    const auto& xmidi_track = tracks[source_track];
    const auto& converted_track = tracks[source_track + 1];
    std::vector<size_t> xmidi_indices;
    for (size_t i = xmidi_track.index;
         i < static_cast<size_t>(xmidi_track.index + xmidi_track.count); i++) {
        if (objdict.pos[i] >= -0.5 && objdict.midi_note[i] >= 0)
            xmidi_indices.push_back(i);
    }

    struct NoteGroup {
        double pos = 0.0;
        double length = 0.0;
        int highest_note = -1;
    };
    std::vector<NoteGroup> groups;
    for (size_t index : xmidi_indices) {
        if (groups.empty() || std::abs(groups.back().pos - objdict.pos[index]) > 1e-8) {
            groups.push_back({objdict.pos[index], objdict.length[index], objdict.midi_note[index]});
        } else {
            groups.back().highest_note = std::max(groups.back().highest_note, objdict.midi_note[index]);
            check(std::abs(groups.back().length - objdict.length[index]) <= 1e-8,
                  "duida chord notes should have matching lengths");
        }
    }

    check(!groups.empty(), "duida XMIDI groups should not be empty");
    check(converted_track.count == static_cast<int>(groups.size()),
          "duida MIDI2Item track should contain one item per XMIDI note group");

    size_t converted_begin = converted_track.index;
    int base_note = groups.front().highest_note;
    for (size_t i = 0; i < groups.size(); i++) {
        size_t converted_index = converted_begin + i;
        check(std::abs(objdict.pos[converted_index] - groups[i].pos) <= 1e-8,
              "duida MIDI2Item position mismatch");
        check(std::abs(objdict.length[converted_index] - groups[i].length) <= 1e-8,
              "duida MIDI2Item length mismatch");
        check(std::abs(objdict.pitch[converted_index] -
                       static_cast<double>(groups[i].highest_note - base_note)) <= 1e-8,
              "duida MIDI2Item relative pitch mismatch");
    }
}

static void test_markers() {
    ObjDict objdict;
    std::vector<TrackNode> tracks;
    std::vector<std::string> file_paths;
    EndWarnings warnings;
    check(parse_rpp("tests/fixtures/rpp/markers.rpp", objdict, tracks, file_paths, warnings),
          "RPP marker fixture should parse");
    check(objdict.markers.size() == 3, "RPP marker count mismatch");
    check(close_to(objdict.markers[0].time_sec, 5.75), "first marker time mismatch");
    check(objdict.markers[0].memo == "plain", "first marker memo mismatch");
    check(close_to(objdict.markers[1].time_sec, 7.0), "quoted marker time mismatch");
    check(objdict.markers[1].memo == "marker with spaces", "quoted marker memo mismatch");
    check(close_to(objdict.markers[2].time_sec, 26.375), "empty marker time mismatch");
    check(objdict.markers[2].memo.empty(), "empty marker memo mismatch");
    std::printf("PASS: RPP marker parser tests passed\n");
}

int main(int argc, char** argv) {
    if (argc > 1) return inspect_project(argv[1]);

    auto path = write_fixture();
    std::string path_utf8 = path.string();

    test_midi_block_mode(path_utf8);
    test_note_mode(path_utf8);
    test_markers();

    const std::string duida_path = "G:\\KOOK_BOT\\aul2\\test_duida.rpp";
    if (std::filesystem::exists(duida_path)) {
        test_duida_midi2item_equivalence(duida_path);
        std::printf("PASS: duida MIDI2Item equivalence test passed\n");
    } else {
        std::printf("SKIP: duida project is not available at %s\n", duida_path.c_str());
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::printf("PASS: RPP XMIDI parser tests passed\n");
    return 0;
}
