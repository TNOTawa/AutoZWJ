#include "core/project_model.h"
#include "core/generation_model.h"
#include "core/effect_model.h"
#include "core/output_config.h"
#include "generation/generation.h"
#include <cstdlib>
#include <cstdio>

static ObjDict make_objdict() {
    ObjDict obj;
    obj.pos    = { -1.0, 0.0 };
    obj.length = {  0.0, 1.0 };
    obj.loop   = {    0,   0 };
    obj.soffs  = {  0.0, 0.0 };
    obj.pitch  = {  0.0, 0.0 };
    obj.playrate = { 1.0, 1.0 };
    obj.fileidx = {    0,   0 };
    obj.filetype = {"", "wav"};
    obj.filelist = {"dummy.wav", "test.wav"};
    obj.bpm = 120.0;
    obj.track_count = 1;
    return obj;
}

static std::vector<TrackNode> make_tracks() {
    TrackNode t;
    t.name = L"Test Track";
    t.number = 1;
    t.index = 0;
    t.count = 1;
    t.selected = true;
    return {t};
}

static std::vector<TemplateSession> make_templates() {
    TemplateEntry entry;
    entry.object_id = 0;
    entry.alias = "test_alias";
    entry.chain = "";
    entry.display_name = "Test Template";
    entry.layer = 1;
    entry.sf = 1.0;
    entry.ef = 60;

    TemplateSession s;
    s.source = std::move(entry);
    return {s};
}

static void check(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        std::exit(1);
    }
}

int main() {
    auto objdict = make_objdict();
    auto tracks = make_tracks();
    auto templates = make_templates();
    OutputConfig config;
    SceneInfo scene;

    GenerationInput in{
        objdict,
        tracks,
        config,
        templates,
        scene,
        1,
        42
    };

    auto result = generate(in);

    check(!result.objects.empty(), "generate() should produce at least one object");
    check(result.warnings.empty(), "no parse/bake warnings expected for simple input");

    auto& obj = result.objects[0];
    check(obj.layer >= 1, "object layer should be >= 1");
    check(obj.sf >= 1, "object start frame should be >= 1");
    check(obj.ef > obj.sf, "object end frame should be after start");
    check(!obj.alias_chain.empty(), "alias chain should not be empty");
    check(obj.name_kind == ObjNameKind::FilePath, "name kind should be FilePath for item with file path");
    check(!obj.name_text.empty(), "name text should contain the file basename");

    std::printf("PASS: all generation tests passed (%zu objects)\n", result.objects.size());
    return 0;
}