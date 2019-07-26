cmake_minimum_required(VERSION 3.11) # FetchContent is available in 3.11+
project(DanPaCompiler)

include_directories(src/.)

add_compile_options (-Og -g -fsanitize=undefined)

file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR} src/*.c)
file(GLOB_RECURSE HEADERS ${PROJECT_SOURCE_DIR} src/*.h)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
target_link_libraries(${PROJECT_NAME} ubsan)
