cmake_minimum_required(VERSION 3.15)
project(EarTrainerCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# Emit compile_commands.json for IDEs (VS Code IntelliSense)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(ear_core STATIC
    src/adaptive_catalog.cpp
    src/adaptive_catalog_builtin.cpp
    src/adaptive_drills.cpp
    src/level_inspector.cpp
    src/track_selector_builtin.cpp
    src/track_selector.cpp
    src/session_engine.cpp
    src/drill_factory.cpp
    src/chord_voicings.cpp
    src/json_bridge.cpp
    src/midi_clip.cpp
    drills/chord.cpp
    drills/chord_core.cpp
    drills/harmony.cpp
    drills/interval.cpp
    drills/melody.cpp
    drills/note.cpp
    drills/pathways.cpp
    assistance/assistance.cpp
    scoring/scoring.cpp
    controller/drill_hub.cpp
)

target_include_directories(ear_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/drills
        ${CMAKE_CURRENT_SOURCE_DIR}/assistance
        ${CMAKE_CURRENT_SOURCE_DIR}/scoring
        ${CMAKE_CURRENT_SOURCE_DIR}/controller
)

# Optional YAML support for loading level catalogs
# Try CMake config package first, prefer imported target if present.
find_package(yaml-cpp QUIET CONFIG)
if(yaml-cpp_FOUND)
    if(TARGET yaml-cpp::yaml-cpp)
        target_link_libraries(ear_core PUBLIC yaml-cpp::yaml-cpp)
    else()
        # Fall back to bare target name; some packages export 'yaml-cpp'
        target_link_libraries(ear_core PUBLIC yaml-cpp)
    endif()
else()
    message(STATUS "yaml-cpp CONFIG not found; trying manual discovery")
    find_path(YAML_CPP_INCLUDE_DIR yaml-cpp/yaml.h
        HINTS ${CMAKE_PREFIX_PATH}/include /opt/homebrew/include /usr/local/include
    )
    find_library(YAML_CPP_LIBRARY NAMES yaml-cpp
        HINTS ${CMAKE_PREFIX_PATH}/lib /opt/homebrew/lib /usr/local/lib
    )
    if (YAML_CPP_INCLUDE_DIR AND YAML_CPP_LIBRARY)
        message(STATUS "Found yaml-cpp: inc=${YAML_CPP_INCLUDE_DIR} lib=${YAML_CPP_LIBRARY}")
        target_include_directories(ear_core PUBLIC ${YAML_CPP_INCLUDE_DIR})
        target_link_libraries(ear_core PUBLIC ${YAML_CPP_LIBRARY})
    else()
        message(STATUS "yaml-cpp not found; YAML features remain disabled at link stage")
    endif()
endif()

add_executable(ear_core_tests tests/test_session_engine.cpp)
target_link_libraries(ear_core_tests PRIVATE ear_core)
target_include_directories(ear_core_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/scoring
    ${CMAKE_CURRENT_SOURCE_DIR}/controller
)

enable_testing()
add_test(NAME ear_core_tests COMMAND ear_core_tests)

# Try to locate pybind11 via CMake config first; add common Homebrew hint
list(APPEND CMAKE_PREFIX_PATH 
    /opt/homebrew 
    /usr/local 
    $ENV{HOME}/.local
)
find_package(pybind11 QUIET CONFIG)
if(pybind11_FOUND)
    pybind11_add_module(_earcore src/bindings.cpp)
    target_link_libraries(_earcore PRIVATE ear_core)
    target_include_directories(_earcore PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
else()
    message(WARNING "pybind11 not found; skipping _earcore module build")
endif()
