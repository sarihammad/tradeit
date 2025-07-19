cmake_minimum_required(VERSION 3.27)
project(tradeit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

file(GLOB_RECURSE SRC_FILES "src/*.cpp" "main.cpp")

add_executable(tradeit ${SRC_FILES})

target_include_directories(tradeit PRIVATE include)

include(FetchContent)
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(json)
target_link_libraries(tradeit PRIVATE nlohmann_json::nlohmann_json)

FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.4
)
FetchContent_MakeAvailable(catch2)

include(CTest)
include(Catch)

file(GLOB TEST_SOURCES tests/test_*.cpp)
list(REMOVE_ITEM SRC_FILES "${CMAKE_SOURCE_DIR}/main.cpp")

add_executable(all_tests ${TEST_SOURCES} ${SRC_FILES})
target_link_libraries(all_tests PRIVATE Catch2::Catch2WithMain nlohmann_json::nlohmann_json)
target_include_directories(all_tests PRIVATE include ${catch2_SOURCE_DIR}/src)

catch_discover_tests(all_tests)

configure_file(${CMAKE_SOURCE_DIR}/config.json ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.json COPYONLY)

file(COPY ${CMAKE_SOURCE_DIR}/data DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/logs)