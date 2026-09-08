cmake_minimum_required(VERSION 3.20)
project(AutoZWJ VERSION 0.3.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static -Wall -Wextra -fno-char8_t")
endif()

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/src/thirdparty/imgui)

file(READ "${CMAKE_SOURCE_DIR}/src/i18n/lang/zh-CN.ini" ZH_CN_INI)
file(READ "${CMAKE_SOURCE_DIR}/src/i18n/lang/en.ini" EN_INI)
file(READ "${CMAKE_SOURCE_DIR}/src/i18n/lang/ja.ini" JA_INI)
configure_file(
    "${CMAKE_SOURCE_DIR}/src/i18n/i18n_embedded.h.in"
    "${CMAKE_SOURCE_DIR}/src/i18n/i18n_embedded.h"
    @ONLY
)

# ---- 产物元数据：版本号/作者写入 DLL 文件属性 (VERSIONINFO) ----
# 版本单一来源为 project(AutoZWJ VERSION ...)，此处仅做拆分注入
set(AUTOZWJ_VERSION_FILE
    "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
set(AUTOZWJ_VERSION_STR
    "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
configure_file(
    "${CMAKE_SOURCE_DIR}/src/version.rc.in"
    "${CMAKE_BINARY_DIR}/version.rc"
    @ONLY
)

# ---- autozwj_core: SDK-free static library ----
add_library(autozwj_core STATIC
    src/generation/generation.cpp
    src/chain/template_chain.cpp
    src/effect/effect_dict.cpp
    src/codec/codec.cpp
    src/tools/up/up_media_parser.cpp
)
target_include_directories(autozwj_core PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# ---- autozwj_ui: ImGui presentation layer ----
add_library(autozwj_ui STATIC
    src/ui/file_picker.cpp
    src/ui/imgui_window.cpp
    src/ui/ui_components.cpp
    src/ui/effect_chain_editor.cpp
)
target_include_directories(autozwj_ui PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/aviutl2_sdk/include/aviutl2_sdk
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# ---- AutoZWJ: shared plugin (.aux2) ----
add_library(AutoZWJ SHARED
    src/plugin.cpp
    src/i18n/i18n.cpp
    src/parsers/rpp/rpp_parser.cpp
    src/parsers/midi/midi_parser.cpp
    src/parsers/ust/ust_parser.cpp
    src/parsers/ustx/ustx_parser.cpp
    src/parsers/lrc/lrc_parser.cpp
    src/tools/tempo/tempo_apply.cpp
    src/tools/up/up.cpp
    src/adapters/menu_registry.cpp
    src/application/import_service.cpp
    src/application/project_controller.cpp
    src/adapters/project_file_store.cpp
    src/autozwj.rc
    ${CMAKE_BINARY_DIR}/version.rc
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(AutoZWJ PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/aviutl2_sdk/include/aviutl2_sdk
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(AutoZWJ PRIVATE
    autozwj_core
    autozwj_ui
    d3d11 d3dcompiler dxgi dwmapi
)
set_target_properties(AutoZWJ PROPERTIES
    PREFIX ""
    SUFFIX ".aux2"
)
set_source_files_properties(
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    PROPERTIES COMPILE_FLAGS "-w"
)

# ---- autozwj_test: CLI test runner (links only autozwj_core) ----
# 测试可执行文件，需要时取消注释
# add_executable(autozwj_test
#     tests/test_generation.cpp
# )
# target_include_directories(autozwj_test PRIVATE
#     ${CMAKE_SOURCE_DIR}/src
# )
# target_link_libraries(autozwj_test PRIVATE
#     autozwj_core
# )

# ---- autozwj_test_up: UP 解析器 CLI 测试 ----
# add_executable(autozwj_test_up
#     tests/test_up_parser.cpp
# )
# target_include_directories(autozwj_test_up PRIVATE
#     ${CMAKE_SOURCE_DIR}/src
# )
# target_link_libraries(autozwj_test_up PRIVATE
#     autozwj_core
# )

# ---- autozwj_test_ust: UST/USTX 解析器 CLI 测试 ----
add_executable(autozwj_test_ust
    tests/test_ust_parsers.cpp
    src/parsers/ust/ust_parser.cpp
    src/parsers/ustx/ustx_parser.cpp
)
target_include_directories(autozwj_test_ust PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(autozwj_test_ust PRIVATE
    autozwj_core
)

# ---- autozwj_test_rpp: RPP marker parser CLI tests ----
add_executable(autozwj_test_rpp
    tests/test_rpp_parser.cpp
    src/parsers/rpp/rpp_parser.cpp
)
target_include_directories(autozwj_test_rpp PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/aviutl2_sdk/include/aviutl2_sdk
)
target_link_libraries(autozwj_test_rpp PRIVATE
    autozwj_core
)

# ---- dump_clipboard: 剪贴板 REAPERMedia 数据导出工具（测试夹具采集用） ----
# add_executable(dump_clipboard
#     tests/fixtures/dump_clipboard.cpp
# )

option(AUTOZWJ_BUILD_TESTS "Build parser and generation CLI tests" OFF)
if(AUTOZWJ_BUILD_TESTS)
    enable_testing()

    add_test(NAME autozwj_test_rpp COMMAND autozwj_test_rpp)
    add_test(NAME autozwj_test_ust COMMAND autozwj_test_ust)
    set_tests_properties(autozwj_test_rpp autozwj_test_ust PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_executable(autozwj_test_generation
        tests/test_generation.cpp
    )
    target_include_directories(autozwj_test_generation PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(autozwj_test_generation PRIVATE autozwj_core)
    add_test(NAME autozwj_test_generation COMMAND autozwj_test_generation)
endif()
