cmake_minimum_required(VERSION 3.18)

project(io_uring_interface_suite CXX)

include(CTest)

if (NOT TARGET slogger)
    add_subdirectory(simple-logger)
endif()

enable_testing()

# Include directories
set(PUBLIC_INCLUDE_DIRECTORIES include/)
set(PRIVATE_INCLUDE_DIRECTORIES src/)

# Source files
file(GLOB_RECURSE SOURCE_FILES "src/*.cpp" "src/*.hpp")
file(GLOB_RECURSE INCLUDE_FILES "include/*.hpp")

add_library(iuring)

target_compile_features(iuring PUBLIC cxx_std_23)
target_link_libraries(iuring -luring slogger)
target_include_directories(iuring PUBLIC ${PUBLIC_INCLUDE_DIRECTORIES})
target_include_directories(iuring PRIVATE ${PRIVATE_INCLUDE_DIRECTORIES})

target_sources(iuring
    PUBLIC FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES ${INCLUDE_FILES}
    PRIVATE ${SOURCE_FILES}
)

install(TARGETS iuring FILE_SET HEADERS)


# Set target properties for public headers
set_target_properties(iuring PROPERTIES PUBLIC_HEADER "${INCLUDE_FILES}")

add_executable(ping tests/ping.cpp)
target_link_libraries(ping iuring)

add_executable(echo_server tests/echo_server.cpp)
target_link_libraries(echo_server iuring)

find_package(GTest)

if(GTest_FOUND)
add_subdirectory(tests)
endif()