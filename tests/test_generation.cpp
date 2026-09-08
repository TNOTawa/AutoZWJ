#include "core/project_model.h"
#include "core/generation_model.h"
#include "core/effect_model.h"
#include "core/output_config.h"
#include "chain/motion_value.h"
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
    config.use_round_up = false;

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
    config.sync_mode = SYNC_MODE_GAP;
    config.use_round_up = true;

    auto res = generate(GenerationInput{objdict, tracks, config, templates, scene, 1, 42});
    check(res.objects.size() == 1, "gap mode: one gap object expected");
    check(res.objects[0].sf == 32, "gap object sf should be 32");
    check(res.objects[0].alias_chain.find("my_param=32") != std::string::npos,
          "gap mode + ceil: note.start_frame var should match sf (32)");
}

// 用例：拉伸到下一音符时，同一输出帧的和弦音符必须共同拉伸到下一组音符。
static void test_stretch_next_chord() {
    auto objdict = make_objdict();
    objdict.pos    = { -1.0, 0.0, 0.0, 0.5 };
    objdict.length = {  0.0, 0.1, 0.1, 0.1 };
    objdict.loop   = {    0,   0,   0,   0 };
    objdict.soffs  = {  0.0, 0.0, 0.0, 0.0 };
    objdict.pitch  = {  0.0, 0.0, 4.0, 7.0 };
    objdict.playrate = { 1.0, 1.0, 1.0, 1.0 };
    objdict.fileidx = { 0, 0, 0, 0 };
    objdict.filetype = { "", "wav", "wav", "wav" };
    objdict.filelist = { "dummy.wav", "test.wav" };

    auto tracks = make_tracks();
    tracks[0].count = 3;
    auto templates = make_templates();
    SceneInfo scene;
    OutputConfig config;
    config.sync_mode = SYNC_MODE_NEXT;
    config.use_round_up = true;

    auto res = generate(GenerationInput{objdict, tracks, config, templates, scene, 1, 42});
    check(res.objects.size() == 3, "stretch-next chord: three objects expected");
    check(res.objects[0].sf == 1 && res.objects[1].sf == 1,
          "stretch-next chord: first two notes should share a start frame");
    check(res.objects[0].ef == 30 && res.objects[1].ef == 30,
          "stretch-next chord: all chord notes should stretch to the next note group");
    check(res.objects[2].sf == 31, "stretch-next chord: next group start frame mismatch");
}

static void test_motion_value_compatibility() {
    std::string start;
    std::string end;
    std::string rest;
    check(chain_parse_motion_value("0,100,1", &start, &end, &rest),
          "motion values: legacy three-field editor format should remain recognized");
    check(chain_parse_motion_value("0,100,1,0", &start, &end, &rest, true),
          "motion values: strict four-field format should be recognized for freeze mode");
    check(!chain_parse_motion_value("255,0,0,255", &start, &end, &rest, true),
          "motion values: RGBA values should not be treated as motion");
}

static void test_animation_sequence() {
    auto objdict = make_objdict();
    objdict.pos = { -1.0, 0.0 };
    objdict.length = { 0.0, 2.0 };
    objdict.fileidx = { -1, -1 };
    auto tracks = make_tracks();
    auto templates = make_templates();
    templates[0].source.sf = 10.0;
    templates[0].source.ef = 19;
    templates[0].source.layer = 3;
    templates[0].source.chain = "[0.0]\neffect.name=first\n\n";

    TemplateSession second = templates[0];
    second.source.sf = 25.0;
    second.source.ef = 39;
    second.source.layer = 5;
    second.source.chain = "[0.0]\neffect.name=second\n\n";
    templates.push_back(second);

    OutputConfig config;
    config.mapping_strategy = MAPPING_STRATEGY_ANIMATION_SEQUENCE;
    config.sync_mode = SYNC_MODE_NOTE;
    config.animation_sequence_allow_stretch = true;
    SceneInfo scene;
    auto res = generate(GenerationInput{objdict, tracks, config, templates, scene, 1, 42});

    check(res.objects.size() == 2, "animation sequence: two child objects expected");
    check(res.objects[0].sf == 1 && res.objects[0].ef == 38,
          "animation sequence: first child should preserve relative duration");
    check(res.objects[1].sf == 63 && res.objects[1].ef == 120,
          "animation sequence: last child should end at the unified note range");
    check(res.objects[1].sf - res.objects[0].ef - 1 == 24,
          "animation sequence: gap should be scaled with the sequence");
    check(res.objects[0].layer == 1 && res.objects[1].layer == 3,
          "animation sequence: relative template layer offset should be preserved");
    check(res.objects[0].name_kind == ObjNameKind::Item &&
          res.objects[0].name_index == 0 && res.objects[0].name_sub_index == 0,
          "animation sequence: first child should use the logical item name");
    check(res.objects[1].name_kind == ObjNameKind::Item &&
          res.objects[1].name_index == 0 && res.objects[1].name_sub_index == 1,
          "animation sequence: second child should use Item x.1 naming");

    config.sync_mode = SYNC_MODE_FIXED;
    config.fixed_duration_frames = 30;
    auto fixed = generate(GenerationInput{objdict, tracks, config, templates, scene, 1, 42});
    check(fixed.objects.size() == 2 && fixed.objects[0].sf == 1 && fixed.objects[1].ef == 30,
          "animation sequence: fixed sync should resize the whole sequence range");
}

static void test_animation_sequence_without_stretch() {
    auto objdict = make_objdict();
    objdict.pos = { -1.0, 0.0 };
    objdict.length = { 0.0, 1.25 };
    objdict.fileidx = { -1, -1 };
    auto tracks = make_tracks();
    auto templates = make_templates();
    templates[0].source.sf = 1.0;
    templates[0].source.ef = 60;
    TemplateSession second = templates[0];
    second.source.sf = 61.0;
    second.source.ef = 120;
    second.source.layer = 2;
    templates.push_back(second);

    OutputConfig config;
    config.mapping_strategy = MAPPING_STRATEGY_ANIMATION_SEQUENCE;
    config.sync_mode = SYNC_MODE_NEXT;
    config.animation_sequence_allow_stretch = false;
    SceneInfo scene;
    auto result = generate(GenerationInput{objdict, tracks, config, templates, scene, 1, 42});

    check(result.objects.size() == 2,
          "animation sequence without stretch: both sequence objects should remain");
    check(result.objects[0].sf == 1 && result.objects[0].ef == 60,
          "animation sequence without stretch: first object keeps its absolute duration");
    check(result.objects[1].sf == 61 && result.objects[1].ef == 75,
          "animation sequence without stretch: second object is clipped to the remaining range");
}

static void test_stretch_hold_last_frame() {
    auto objdict = make_objdict();
    objdict.pos    = { -1.0, 0.0, 3.0 };
    objdict.length = {  0.0, 2.0, 0.25 };
    objdict.loop   = {    0,   0,   0 };
    objdict.soffs  = {  0.0, 0.0, 0.0 };
    objdict.pitch  = {  0.0, 0.0, 0.0 };
    objdict.playrate = { 1.0, 1.0, 1.0 };
    objdict.fileidx = { -1, -1, -1 };
    objdict.filetype = { "", "wav", "wav" };
    objdict.filelist = { "dummy.wav", "short.wav", "long.wav" };
    auto tracks = make_tracks();
    tracks[0].count = 2;
    auto templates = make_templates();
    templates[0].source.chain = "[0.0]\neffect.name=test_effect\n颜色=255,0,0\n位置=0,100,1,0\nnote_idx=0\n\n";
    ParamBake index_bake;
    index_bake.effect_index = 0;
    index_bake.param_name = "note_idx";
    index_bake.param_value = "$note.index$";
    index_bake.value_mode = 1;
    index_bake.active = true;
    templates[0].bakes.push_back(index_bake);

    SceneInfo scene;
    OutputConfig config;
    config.sync_mode = SYNC_MODE_STRETCH_HOLD_LAST;
    config.fixed_duration_frames = 30;
    config.use_round_up = true;
    config.alt_flip = true;
    config.flip_type = FLIP_HORIZONTAL;
    config.stretch_hold_last_to_next = true;

    auto result = generate(GenerationInput{objdict, tracks, config, templates, scene, 1, 42});
    check(result.objects.size() == 3, "hold-last mode: short note plus long note continuation expected");
    check(result.objects[0].sf == 1 && result.objects[0].ef == 30,
          "hold-last mode: long note prefix should use fixed duration");
    check(result.objects[1].sf == 31 && result.objects[1].ef == 180,
          "hold-last mode: continuation should stretch to the next note");
    check(result.objects[0].alias_chain.find("位置=0,100,1,0") != std::string::npos,
          "hold-last mode: prefix should keep template motion");
    check(result.objects[0].name_kind == ObjNameKind::Item &&
          result.objects[0].name_index == 0 && result.objects[0].name_sub_index == 0,
          "hold-last mode: prefix should use the logical item number");
    check(result.objects[1].name_kind == ObjNameKind::Item &&
          result.objects[1].name_index == 0 && result.objects[1].name_sub_index == 1,
          "hold-last mode: continuation should use Item x.1 numbering");
    check(result.objects[2].name_kind == ObjNameKind::Item &&
          result.objects[2].name_index == 1 && result.objects[2].name_sub_index == 0,
          "hold-last mode: following note should keep its logical item number");
    check(result.objects[0].alias_chain.find("note_idx=1") != std::string::npos &&
          result.objects[1].alias_chain.find("note_idx=1") != std::string::npos &&
          result.objects[2].alias_chain.find("note_idx=2") != std::string::npos,
          "hold-last mode: scripts should see the continuation as the same note");
    check(result.objects[0].alias_chain.find("左右反転=0") != std::string::npos &&
          result.objects[1].alias_chain.find("左右反転=0") != std::string::npos &&
          result.objects[2].alias_chain.find("左右反転=1") != std::string::npos,
          "hold-last mode: alternating flip should count continuation with its prefix");
    check(result.objects[1].alias_chain.find("位置=100") != std::string::npos,
          "hold-last mode: continuation should hold the motion end value");
    check(result.objects[1].alias_chain.find("颜色=255,0,0") != std::string::npos,
          "hold-last mode: continuation should preserve non-motion color values");
    check(result.objects[1].alias_chain.find("位置=0,100,1,0") == std::string::npos,
          "hold-last mode: continuation should remove motion rules");
    check(result.objects[2].sf == 181 && result.objects[2].ef == 195,
          "hold-last mode: short following note should stay aligned to its note");
}

static void test_stretch_hold_last_frame_chord_boundary() {
    auto objdict = make_objdict();
    objdict.pos = { -1.0, 0.0, 0.0 };
    objdict.length = { 0.0, 2.0, 0.1 };
    objdict.loop = { 0, 0, 0 };
    objdict.soffs = { 0.0, 0.0, 0.0 };
    objdict.pitch = { 0.0, 0.0, 4.0 };
    objdict.playrate = { 1.0, 1.0, 1.0 };
    objdict.fileidx = { -1, -1, -1 };
    objdict.filetype = { "", "wav", "wav" };
    auto tracks = make_tracks();
    tracks[0].count = 2;
    auto templates = make_templates();

    OutputConfig config;
    config.sync_mode = SYNC_MODE_STRETCH_HOLD_LAST;
    config.fixed_duration_frames = 30;
    SceneInfo scene;
    auto result = generate(GenerationInput{objdict, tracks, config, templates, scene, 1, 42});

    check(result.objects.size() == 3,
          "hold-last chord boundary: long note prefix, tail, and chord note expected");
    check(result.objects[0].sf == 1 && result.objects[0].ef == 30,
          "hold-last chord boundary: long note prefix should be retained");
    check(result.objects[1].sf == 31 && result.objects[1].ef == 120,
          "hold-last chord boundary: long note tail should remain valid");
    check(result.objects[2].sf == 1 && result.objects[2].ef > result.objects[2].sf,
          "hold-last chord boundary: same-frame chord note should remain present");
}

static void test_stretch_hold_last_frame_options() {
    auto objdict = make_objdict();
    objdict.pos = { -1.0, 0.0, 1.0 };
    objdict.length = { 0.0, 0.1, 0.1 };
    objdict.loop = { 0, 0, 0 };
    objdict.soffs = { 0.0, 0.0, 0.0 };
    objdict.pitch = { 0.0, 0.0, 0.0 };
    objdict.playrate = { 1.0, 1.0, 1.0 };
    objdict.fileidx = { -1, -1, -1 };
    objdict.filetype = { "", "wav", "wav" };
    auto tracks = make_tracks();
    tracks[0].count = 2;
    auto templates = make_templates();
    SceneInfo scene;

    OutputConfig aligned;
    aligned.sync_mode = SYNC_MODE_STRETCH_HOLD_LAST;
    aligned.fixed_duration_frames = 30;
    aligned.stretch_hold_last_to_next = false;
    auto aligned_result = generate(GenerationInput{objdict, tracks, aligned, templates, scene, 1, 42});
    check(aligned_result.objects.size() == 2 && aligned_result.objects[0].ef == 6,
          "hold-last options: unchecked short note should remain aligned");

    OutputConfig stretched = aligned;
    stretched.stretch_hold_last_to_next = true;
    auto stretched_result = generate(GenerationInput{objdict, tracks, stretched, templates, scene, 1, 42});
    check(stretched_result.objects.size() == 2 && stretched_result.objects[0].ef == 60,
          "hold-last options: checked short note should stretch to the next note");

    ObjDict compressed = objdict;
    compressed.pos = { -1.0, 0.0, 0.2 };
    compressed.length = { 0.0, 2.0, 0.1 };
    auto compressed_result = generate(GenerationInput{compressed, tracks, aligned, templates, scene, 1, 42});
    check(compressed_result.objects.size() == 2,
          "hold-last options: compressed prefix should not lose the following note");
    check(compressed_result.objects[0].sf == 1 && compressed_result.objects[0].ef == 12,
          "hold-last options: prefix should compress to the next note boundary");
    check(compressed_result.objects[1].sf == 13,
          "hold-last options: following note should start immediately after compressed prefix");
}

int main() {
    test_round_up();
    test_motion_value_compatibility();
    test_gap_round_up();
    test_stretch_next_chord();
    test_animation_sequence();
    test_animation_sequence_without_stretch();
    test_stretch_hold_last_frame();
    test_stretch_hold_last_frame_chord_boundary();
    test_stretch_hold_last_frame_options();

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
