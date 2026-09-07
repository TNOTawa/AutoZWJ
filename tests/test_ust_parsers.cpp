#include "parsers/ust/ust_parser.h"
#include "parsers/ustx/ustx_parser.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

static void check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static bool close_to(double lhs, double rhs, double epsilon = 1e-9) {
    return std::abs(lhs - rhs) <= epsilon;
}

static void test_ust() {
    ObjDict objdict;
    std::vector<TrackNode> tracks;
    std::vector<std::string> files;
    check(parse_ust("tests/fixtures/ust/basic.ust", objdict, tracks, files),
          "UST fixture should parse");
    check(tracks.size() == 1, "UST should produce one track");
    check(tracks[0].count == 2, "UST rest should be filtered");
    check(objdict.pos.size() == 3, "UST should contain two notes plus sentinel");
    check(close_to(objdict.pos[1], 0.0), "UST first note position mismatch");
    check(close_to(objdict.length[1], 0.4), "UST first note length mismatch");
    check(close_to(objdict.pos[2], 0.8), "UST Delta/Duration position mismatch");
    check(close_to(objdict.length[2], 0.2), "UST tempo-adjusted length mismatch");
    check(objdict.midi_note[1] == 60 && objdict.midi_note[2] == 62,
          "UST note numbers mismatch");
    check(objdict.midi_lyric[2] == u8"あ", "UST lyric encoding mismatch");
    check(close_to(objdict.midi_volume[1], 80.0), "UST velocity mismatch");
    check(objdict.tempo_map.size() == 1, "UST initial note tempo should fix legacy placeholder tempo");
    check(close_to(objdict.tempo_map[0].time_sec, 0.0), "UST initial tempo position mismatch");
    check(close_to(objdict.tempo_map[0].bpm, 150.0), "UST initial tempo value mismatch");
}

static void test_ustx() {
    ObjDict objdict;
    std::vector<TrackNode> tracks;
    std::vector<std::string> files;
    check(parse_ustx("tests/fixtures/ustx/basic.ustx", objdict, tracks, files),
          "USTX fixture should parse");
    check(tracks.size() == 2, "USTX should preserve two tracks");
    check(tracks[0].name == L"Voice A" && tracks[1].name == L"Voice B",
          "USTX track names mismatch");
    check(tracks[0].count == 2 && tracks[1].count == 1,
          "USTX track item counts mismatch");
    check(objdict.pos.size() == 5, "USTX should contain three notes and track sentinel data");
    check(close_to(objdict.pos[1], 0.0), "USTX first note position mismatch");
    check(close_to(objdict.length[1], 0.6), "USTX first note length mismatch");
    check(close_to(objdict.pos[2], 0.6), "USTX second note position mismatch");
    check(close_to(objdict.length[2], 0.6), "USTX second note length mismatch");
    check(close_to(objdict.pos[4], 1.2), "USTX Part position mismatch");
    check(close_to(objdict.length[4], 0.3), "USTX second tempo length mismatch");
    check(objdict.midi_lyric[1] == "a" && objdict.midi_lyric[4] == "u",
          "USTX lyric mismatch");
    check(close_to(objdict.midi_volume[1], 77.0), "USTX vel expression mismatch");
    check(close_to(objdict.pitch[2], 62.25 - 69.0), "USTX tuning mismatch");
    check(objdict.tempo_map.size() == 2, "USTX tempo map should contain two entries");
    check(close_to(objdict.tempo_map[0].bpm, 100.0), "USTX initial tempo mismatch");
    check(close_to(objdict.tempo_map[1].time_sec, 1.2), "USTX tempo position mismatch");
    check(close_to(objdict.tempo_map[1].bpm, 200.0), "USTX second tempo mismatch");
}

int main() {
    test_ust();
    test_ustx();
    std::printf("PASS: UST and USTX parser tests passed\n");
    return 0;
}
