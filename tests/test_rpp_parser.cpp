#include "parsers/rpp/rpp_parser.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static void check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static bool close_to(double lhs, double rhs, double epsilon = 1e-9) {
    return std::abs(lhs - rhs) <= epsilon;
}

int main(int argc, char** argv) {
    const char* fixture_path = argc > 1 ? argv[1] : "tests/fixtures/rpp/markers.rpp";
    ObjDict objdict;
    std::vector<TrackNode> tracks;
    std::vector<std::string> file_paths;
    EndWarnings warnings;

    check(parse_rpp(fixture_path, objdict, tracks, file_paths, warnings),
          "RPP marker fixture should parse");

    if (argc > 1) {
        std::printf("PASS: parsed %zu RPP markers from %s\n", objdict.markers.size(), fixture_path);
        for (const auto& marker : objdict.markers) {
            std::printf("  %.9f: %s\n", marker.time_sec, marker.memo.c_str());
        }
        return 0;
    }

    check(objdict.markers.size() == 3, "RPP marker count mismatch");
    check(close_to(objdict.markers[0].time_sec, 5.75), "first marker time mismatch");
    check(objdict.markers[0].memo == "plain", "first marker memo mismatch");
    check(close_to(objdict.markers[1].time_sec, 7.0), "quoted marker time mismatch");
    check(objdict.markers[1].memo == "marker with spaces", "quoted marker memo mismatch");
    check(close_to(objdict.markers[2].time_sec, 26.375), "empty marker time mismatch");
    check(objdict.markers[2].memo.empty(), "empty marker memo mismatch");

    std::printf("PASS: RPP marker parser tests passed\n");
    return 0;
}
