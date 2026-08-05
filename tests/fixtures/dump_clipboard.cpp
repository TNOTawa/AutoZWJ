// dump_clipboard: 将剪贴板中的 REAPERMedia 格式数据原样导出为文件，
// 供 tests/test_up_parser.cpp 的 fixture 使用。
// 用法：在 REAPER 中复制带媒体的条目（Copy items with media）后运行：
//   dump_clipboard.exe <输出路径>
#include <windows.h>
#include <cstdio>

int main(int argc, char** argv) {
    const char* out_path = (argc > 1) ? argv[1] : "reaper_media.txt";

    UINT cf = RegisterClipboardFormatW(L"REAPERMedia");
    if (cf == 0) {
        std::fprintf(stderr, "failed to register REAPERMedia format\n");
        return 1;
    }
    if (!OpenClipboard(nullptr)) {
        std::fprintf(stderr, "failed to open clipboard\n");
        return 1;
    }

    HANDLE hMem = GetClipboardData(cf);
    if (!hMem) {
        std::fprintf(stderr, "no REAPERMedia data in clipboard (copy items with media in REAPER first)\n");
        CloseClipboard();
        return 1;
    }

    SIZE_T size = GlobalSize(hMem);
    const char* ptr = static_cast<const char*>(GlobalLock(hMem));
    if (!ptr || size == 0) {
        std::fprintf(stderr, "clipboard data is empty\n");
        GlobalUnlock(hMem);
        CloseClipboard();
        return 1;
    }

    FILE* f = nullptr;
    if (fopen_s(&f, out_path, "wb") != 0 || !f) {
        std::fprintf(stderr, "cannot write %s\n", out_path);
        GlobalUnlock(hMem);
        CloseClipboard();
        return 1;
    }
    fwrite(ptr, 1, size, f);
    fclose(f);

    GlobalUnlock(hMem);
    CloseClipboard();

    std::printf("saved %llu bytes to %s\n", (unsigned long long)size, out_path);
    return 0;
}
