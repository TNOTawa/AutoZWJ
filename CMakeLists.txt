cmake_minimum_required(VERSION 3.20)
project(AutoZWJ VERSION 0.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++ -w")
endif()

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/src/thirdparty/imgui)

add_library(AutoZWJ SHARED
    src/plugin.cpp
    src/rpp/rpp_parser.cpp
    src/midi/midi_parser.cpp
    src/lrc/lrc_parser.cpp
    src/exo/object_generator.cpp
    src/effect/effect_dict.cpp
    src/ui/file_picker.cpp
    src/ui/imgui_window.cpp
    src/ui/project_state.cpp
    src/ui/ui_components.cpp
    src/ui/effect_chain_editor.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

target_include_directories(AutoZWJ PRIVATE
    ${CMAKE_SOURCE_DIR}/aviutl2_sdk
    ${CMAKE_SOURCE_DIR}/src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(AutoZWJ PRIVATE
    d3d11 d3dcompiler dxgi dwmapi
)

set_target_properties(AutoZWJ PROPERTIES
    PREFIX ""
    SUFFIX ".aux2"
)

# Disable warnings from ImGui sources
set_source_files_properties(
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    PROPERTIES COMPILE_FLAGS "-w"
)
