#include "tools/up/up.h"
#include "tools/up/up_media_parser.h"
#include "adapters/menu_registry.h"
#include "plugin.h"
#include "i18n/i18n.h"
#include "codec/codec.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <vector>

static const char* kUpMenuLabel = u8"[AutoZWJ-UP] 导入 REAPER 剪贴板";

// AviUtl2 内部效果名键固定为日语（语言包只翻译显示文本）
static LPCWSTR effect_name_for_source(const std::string& source_type) {
    if (source_type == "VIDEO") return L"動画ファイル";
    if (source_type == "AUDIO") return L"音声ファイル";
    if (source_type == "IMAGE") return L"画像ファイル";
    return L"動画ファイル";
}

// 层分配（优化模式）：贪心首适配，语义与 generation.cpp 的 assign_layer_impl 策略 0 一致
static int assign_layer_up(double obj_fp, double bf, std::vector<double>& layer_ends) {
    for (size_t k = 0; k < layer_ends.size(); k++) {
        if (layer_ends[k] < obj_fp) {
            layer_ends[k] = bf;
            return (int)k;
        }
    }
    layer_ends.push_back(bf);
    return (int)layer_ends.size() - 1;
}

static void generate_objects(EDIT_SECTION* edit, const UpProject& proj, double fps, int base_layer) {
    if (!edit) return;

    int total_items = 0;
    int final_layer = base_layer;
    int final_frame = 0;

    for (const auto& track : proj.tracks) {
        if (track.items.empty()) continue;

        std::vector<double> layer_ends;

        for (const auto& item : track.items) {
            if (item.file_path.empty()) continue;

            // 剪贴板数据由外部应用提供，钳制异常时间值防止帧数学溢出
            double pos_sec = std::min(item.position, 1.0e6);
            double len_sec = std::min(std::max(item.length, 0.0), 1.0e6);

            int sf = (int)std::round(pos_sec * fps + 1.0);
            if (sf < 1) sf = 1;

            int frames = (int)std::round(len_sec * fps);
            if (frames < 1) frames = 1;

            int ef = sf + frames;
            int length = ef - sf;

            int layer_offset = assign_layer_up((double)sf, (double)ef, layer_ends);
            int use_layer = base_layer + layer_offset;

            std::wstring wpath = utf8_to_wide(item.file_path);
            OBJECT_HANDLE obj = edit->create_object_from_media_file(wpath.c_str(), use_layer, sf, length);

            if (!obj) {
                if (g_host.logger) {
                    g_host.logger->error(g_host.logger, utf8_to_wide(
                        tr_fmt(u8"AutoZWJ: 创建物件失败（图层 {}, 帧 {}）：{}", use_layer, sf, item.file_path)).c_str());
                }
                continue;
            }

            total_items++;
            if (use_layer > final_layer) final_layer = use_layer;
            if (ef > final_frame) final_frame = ef;

            // 物件命名：媒体文件名（截断路径）
            std::wstring obj_name = wpath;
            size_t sep = obj_name.find_last_of(L"\\/");
            if (sep != std::wstring::npos) obj_name = obj_name.substr(sep + 1);
            edit->set_object_name(obj, obj_name.c_str());

            LPCWSTR effect = effect_name_for_source(item.source_type);
            double pr = item.playrate;
            if (std::abs(pr) < 0.0001) pr = 1.0;

            double source_duration = item.length / std::abs(pr);
            double pos_start = item.soffs;
            double pos_end = item.soffs + source_duration;

            std::ostringstream pos_oss;
            pos_oss << std::fixed << std::setprecision(3)
                    << pos_start << "," << pos_end << u8",再生範囲,0";
            edit->set_object_item_value(obj, effect, L"再生位置", pos_oss.str().c_str());

            std::ostringstream speed_oss;
            speed_oss << std::fixed << std::setprecision(2) << (pr * 100.0);
            edit->set_object_item_value(obj, effect, L"再生速度", speed_oss.str().c_str());

            edit->set_object_item_value(obj, effect, L"ループ再生", item.loop ? "1" : "0");
        }

        base_layer += (int)layer_ends.size();
    }

    if (total_items > 0 && final_frame > 0) {
        edit->set_cursor_layer_frame(final_layer, final_frame);
    }

    if (g_host.logger) {
        g_host.logger->log(g_host.logger, utf8_to_wide(
            tr_fmt(u8"AutoZWJ: 已从剪贴板导入 {} 个物件（{} 条轨道）", total_items, (int)proj.tracks.size())).c_str());
    }
}

static void on_import_clipboard(EDIT_SECTION* edit) {
    if (!edit) return;

    double fps = 60.0;
    int cursor_layer = 0;
    if (edit->info) {
        double rate = (double)edit->info->rate;
        double scale = (double)edit->info->scale;
        if (scale > 0) fps = rate / scale;
        cursor_layer = edit->info->layer;
    }

    UINT cf = RegisterClipboardFormatW(L"REAPERMedia");
    if (cf == 0) {
        if (g_host.logger) g_host.logger->log(g_host.logger, utf8_to_wide(tr_str(u8"AutoZWJ: 无法注册 REAPERMedia 剪贴板格式")).c_str());
        return;
    }

    if (!OpenClipboard(nullptr)) {
        if (g_host.logger) g_host.logger->log(g_host.logger, utf8_to_wide(tr_str(u8"AutoZWJ: 无法打开剪贴板")).c_str());
        return;
    }

    HGLOBAL hMem = GetClipboardData(cf);
    if (!hMem) {
        CloseClipboard();
        if (g_host.logger) g_host.logger->log(g_host.logger, utf8_to_wide(tr_str(u8"AutoZWJ: 剪贴板中没有 REAPERMedia 数据")).c_str());
        return;
    }

    SIZE_T size = GlobalSize(hMem);
    const uint8_t* ptr = static_cast<const uint8_t*>(GlobalLock(hMem));
    if (!ptr || size == 0) {
        GlobalUnlock(hMem);
        CloseClipboard();
        if (g_host.logger) g_host.logger->log(g_host.logger, utf8_to_wide(tr_str(u8"AutoZWJ: 剪贴板数据为空")).c_str());
        return;
    }

    // 剪贴板内容由外部应用放置，拒绝超大块防止分配失败导致宿主崩溃
    const SIZE_T kMaxClipboardBytes = 64u * 1024u * 1024u;
    if (size > kMaxClipboardBytes) {
        GlobalUnlock(hMem);
        CloseClipboard();
        if (g_host.logger) {
            g_host.logger->log(g_host.logger, utf8_to_wide(
                tr_fmt(u8"AutoZWJ: 剪贴板数据过大（{} MB），已忽略", (int)(size / (1024u * 1024u)))).c_str());
        }
        return;
    }

    std::vector<uint8_t> data(ptr, ptr + size);
    GlobalUnlock(hMem);
    CloseClipboard();

    UpProject proj;
    if (!parse_reaper_media(data, proj)) {
        if (g_host.logger) g_host.logger->log(g_host.logger, utf8_to_wide(tr_str(u8"AutoZWJ: 剪贴板中未找到可导入的媒体数据")).c_str());
        return;
    }

    generate_objects(edit, proj, fps, cursor_layer);
}

void up_register_menu(HOST_APP_TABLE* host) {
    menu_registry_upsert("up.clipboard", kUpMenuLabel, true);

    for (const auto& e : menu_registry_all()) {
        if (e.id != "up.clipboard") continue;
        if (!e.enabled) break;
        static std::wstring s_label = utf8_to_wide(tr_str(kUpMenuLabel));
        host->register_layer_menu(s_label.c_str(), on_import_clipboard);
        break;
    }
}
