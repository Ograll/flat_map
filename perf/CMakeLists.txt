cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0054 NEW)

#find_package(Boost)
set(Boost_INCLUDE_DIRS /home/cdannemiller-d/Downloads/boost_1_81_0/boost)
include_directories(${Boost_INCLUDE_DIRS})

set(CXX_STD 17 CACHE STRING "Set to 17, 2a, or 20.")

add_executable(perf_test ${CMAKE_SOURCE_DIR}/map_insertion_perf_tests.cpp)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_link_libraries(perf_test c++)
endif ()

find_package(PythonInterp)

set(perf_test_output
    boost_flat_map.py
    std_map.py
)

add_custom_command(
    OUTPUT
        ${perf_test_output}
    COMMAND
        perf_test
    DEPENDS
        perf_test
    COMMENT
        "-- Running perf tests..."
)

add_custom_target(
    make_data
    DEPENDS
        ${perf_test_output}
    COMMENT
        "-- Filling in LaTex graphs..."
)

add_custom_target(
    make_tex_file_from_data
    ALL
    COMMAND
        ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/make_tex.py
    DEPENDS
        make_data
        ${CMAKE_CURRENT_SOURCE_DIR}/make_tex.py
        ${CMAKE_SOURCE_DIR}/../paper/motivation_and_scope.in.tex
)
