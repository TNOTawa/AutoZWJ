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

// 用例：向上取整帧
// 物件位于 0.02s（60fps 下 = 1.2 帧，obj_fp = 2.2）：
//   四舍五入 → sf=2；向上取整 → sf=3
// 同时验证 $note.start_frame$ / $note.end_frame$ 变量与实际输出帧号一致
// （ef 受最小时长钳制：round 模式 bf=2.4→ef=2→钳制为 3；ceil 模式 ef=3→钳制为 4）。
static void test_round_up() {
    auto objdict = make_objdict();
    objdict.pos    = { -1.0, 0.02 };
    objdict.length = {  0.0, 0.02 };
    auto tracks = make_tracks();
    auto templates = make_templates();
    templates[0].source.chain = "[0.0]\neffect.name=test_effect\nmy_param=0\nmy_end=0\n\n";
    ParamBake bake;
    bake.effect_index = 0;
    bake.param_name = "my_param";
    bake.param_value = "$note.start_frame$";
    bake.value_mode = 1;
    bake.active = true;
    templates[0].bakes.push_back(bake);
    ParamBake bake_end = bake;
    bake_end.param_name = "my_end";
    bake_end.param_value = "$note.end_frame$";
    templates[0].bakes.push_back(bake_end);

    SceneInfo scene;
    OutputConfig config;

    auto res_round = generate(GenerationInput{objdict, tracks, config, templates, scene, 1, 42});
    check(res_round.objects.size() == 1, "round mode: one object expected");
    check(res_round.objects[0].sf == 2, "round mode sf should be 2 (round(2.2))");
    check(res_round.objects[0].ef == 3, "round mode ef should be 3 (clamped from 2)");
    check(res_round.objects[0].alias_chain.find("my_param=2") != std::string::npos,
          "round mode: note.start_frame var should match sf (2)");
    check(res_round.objects[0].alias_chain.find("my_end=3") != std::string::npos,
          "round mode: note.end_frame var should match ef (3)");

    config.use_round_up = true;
    auto res_ceil = generate(GenerationInput{objdict, tracks, config, templates, scene, 1, 42});
    check(res_ceil.objects.size() == 1, "ceil mode: one object expected");
    check(res_ceil.objects[0].sf == 3, "ceil mode sf should be 3 (ceil(2.2))");
    check(res_ceil.objects[0].ef == 4, "ceil mode ef should be 4 (clamped from 3)");
    check(res_ceil.objects[0].alias_chain.find("my_param=3") != std::string::npos,
          "ceil mode: note.start_frame var should match sf (3)");
    check(res_ceil.objects[0].alias_chain.find("my_end=4") != std::string::npos,
          "ceil mode: note.end_frame var should match ef (4)");
}

// 用例：gap 模式（sync_mode=3）+ 向上取整帧回归防护
// item1 结束于 31 帧，gap 起点 = 32 帧；gap 物件的 pos_sec = 31/60，
// 浮点重算 (31/60)*60 = 31.000000000000004 若按 pos_sec 重新取整会越界为 33。
// 验证 note.start_frame 变量与实际 sf=32 一致（F1 回归防护）。
static void test_gap_round_up() {
    auto objdict = make_objdict();
    objdict.pos    = { -1.0, 0.0, 1.0 };
    objdict.length = {  0.0, 0.5166666666666666, 0.1 };
    objdict.loop   = {  0,   0,   0 };
    objdict.soffs  = { 0.0, 0.0, 0.0 };
    objdict.pitch  = { 0.0, 0.0, 0.0 };
    objdict.playrate = { 1.0, 1.0, 1.0 };
    objdict.fileidx  = {  0,   0,   0 };
    objdict.filetype = { "", "wav", "wav" };
    objdict.filelist = { "dummy.wav", "test.wav", "test2.wav" };
    auto tracks = make_tracks();
    tracks[0].count = 2;
    auto templates = make_templates();
    templates[0].source.chain = "[0.0]\neffect.name=test_effect\nmy_param=0\n\n";
    ParamBake bake;
    bake.effect_index = 0;
    bake.param_name = "my_param";
    bake.param_value = "$note.start_frame$";
    bake.value_mode = 1;
    bake.active = true;
    templates[0].bakes.push_back(bake);

    SceneInfo scene;
    OutputConfig config;
    config.sync_mode = 3;
    config.use_round_up = true;

    auto res = generate(GenerationInput{objdict, tracks, config, templates, scene, 1, 42});
    check(res.objects.size() == 1, "gap mode: one gap object expected");
    check(res.objects[0].sf == 32, "gap object sf should be 32");
    check(res.objects[0].alias_chain.find("my_param=32") != std::string::npos,
          "gap mode + ceil: note.start_frame var should match sf (32)");
}

int main() {
    test_round_up();
    test_gap_round_up();

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