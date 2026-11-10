cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0091 NEW)

project(relaxation_reminder LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RR_TEST_MODE "Enable test-only tray actions" OFF)

add_executable(RelaxationReminder WIN32
    src/win32/main.cpp
    src/win32/reminder_window.cpp
    src/win32/character_image.cpp
    src/win32/rest_sound.cpp
    src/win32/rounded_ui.cpp
    src/win32/settings_window.cpp
    src/win32/splash_window.cpp
    src/core/config.cpp
    src/core/state_machine.cpp
    src/core/growth.cpp
    src/resources/app.rc
)

target_include_directories(RelaxationReminder PRIVATE
    src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(RelaxationReminder PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_compile_options(RelaxationReminder PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/EHsc;/utf-8;/permissive-;/experimental:deterministic;"/pathmap:${CMAKE_SOURCE_DIR}=.">
)

# Delay-load winmm so the audio stack is only mapped on the first chime.
target_link_options(RelaxationReminder PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/DELAYLOAD:winmm.dll>
)

if(RR_TEST_MODE)
    target_compile_definitions(RelaxationReminder PRIVATE RR_TEST_MODE)
endif()

target_link_libraries(RelaxationReminder PRIVATE
    user32
    gdi32
    gdiplus
    comdlg32
    comctl32
    uxtheme
    shell32
    advapi32
    shcore
    dwmapi
    winmm
    delayimp
)

set_target_properties(RelaxationReminder PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    OUTPUT_NAME "RelaxationReminder"
)
