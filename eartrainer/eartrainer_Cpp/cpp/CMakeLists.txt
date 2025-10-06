cmake_minimum_required(VERSION 3.15)
project(EarTrainerCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(ear_core STATIC
    src/adaptive_catalog.cpp
    src/adaptive_drills.cpp
    src/session_engine.cpp
    src/drill_factory.cpp
    src/json_bridge.cpp
    drills/chord.cpp
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

find_package(pybind11 QUIET)
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
