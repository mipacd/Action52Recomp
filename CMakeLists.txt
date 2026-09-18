cmake_minimum_required(VERSION 3.24)
project(action52_native VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(A52_FETCH_SDL "Fetch SDL3 when no installed package is available" ON)
option(A52_BUILD_TESTS "Build ROM-independent tests" ON)
set(A52_ROM "" CACHE FILEPATH "Path to the verified Action 52 (USA) ROM")

find_package(SDL3 CONFIG QUIET)
if(NOT SDL3_FOUND AND A52_FETCH_SDL)
  include(FetchContent)
  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.12
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(SDL3)
endif()

add_library(a52_core
  src/app.cpp
  src/asset_manifest.cpp
  src/audio.cpp
  src/cheetahmen.cpp
  src/config.cpp
  src/fix_registry.cpp
  src/hash.cpp
  src/rollback.cpp
  src/ui_assets.cpp
)
target_include_directories(a52_core PUBLIC include)
target_compile_features(a52_core PUBLIC cxx_std_20)

add_executable(a52_extract
  tools/extractor/main.cpp
  tools/extractor/extractor.cpp
  tools/extractor/png_writer.cpp
)
target_include_directories(a52_extract PRIVATE include tools/extractor)
target_link_libraries(a52_extract PRIVATE a52_core)

if(TARGET SDL3::SDL3)
  add_executable(action52 src/main.cpp src/sdl_frontend.cpp)
  target_link_libraries(action52 PRIVATE a52_core SDL3::SDL3)
else()
  message(WARNING "SDL3 was not found; the extractor and tests can still be built")
endif()

if(A52_ROM)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(A52_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated-assets")
  add_custom_command(
    OUTPUT "${A52_GENERATED_DIR}/manifest.json"
    COMMAND a52_extract --rom "${A52_ROM}" --out "${A52_GENERATED_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/extract_cheetahmen.py"
            --rom "${A52_ROM}" --out "${A52_GENERATED_DIR}"
    DEPENDS a52_extract "${A52_ROM}"
            "${CMAKE_SOURCE_DIR}/tools/extract_cheetahmen.py"
    COMMENT "Extracting local Action 52 assets"
    VERBATIM)
  add_custom_target(a52_assets ALL DEPENDS "${A52_GENERATED_DIR}/manifest.json")
  if(TARGET action52)
    add_dependencies(action52 a52_assets)
    target_compile_definitions(action52 PRIVATE A52_DEFAULT_ASSET_DIR="${A52_GENERATED_DIR}")
  endif()
endif()

if(A52_BUILD_TESTS)
  enable_testing()
  add_executable(a52_tests tests/test_main.cpp tools/extractor/png_writer.cpp)
  target_include_directories(a52_tests PRIVATE include tools/extractor)
  target_link_libraries(a52_tests PRIVATE a52_core)
  add_test(NAME a52_tests COMMAND a52_tests)
endif()
