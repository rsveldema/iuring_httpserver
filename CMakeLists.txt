cmake_minimum_required(VERSION 3.18)

project(io_uring_interface_suite CXX)

include(CTest)

add_subdirectory(iuring)

enable_testing()

find_library(MBEDTLS_LIBRARY NAMES libmbedtls.a HINTS /usr/lib /usr/lib64 /usr/local/lib /usr/local/lib64)
find_library(MBEDTLS_CRYPTO_LIBRARY NAMES libmbedcrypto.a HINTS /usr/lib /usr/lib64 /usr/local/lib /usr/local/lib64)
find_library(MBEDTLS_X508_LIBRARY NAMES libmbedx509.a HINTS /usr/lib /usr/lib64 /usr/local/lib /usr/local/lib64)


# Include directories
set(PUBLIC_INCLUDE_DIRECTORIES include/)
set(PRIVATE_INCLUDE_DIRECTORIES src/)

# Source files
file(GLOB_RECURSE SOURCE_FILES "src/*.cpp" "src/*.hpp")
file(GLOB_RECURSE INCLUDE_FILES "include/*.hpp")

add_library(iuring_httpserver)

target_compile_features(iuring_httpserver PUBLIC cxx_std_23)
target_link_libraries(iuring_httpserver iuring slogger     ${MBEDTLS_LIBRARY}
    ${MBEDTLS_CRYPTO_LIBRARY}
    ${MBEDTLS_X508_LIBRARY})
target_include_directories(iuring_httpserver PUBLIC ${PUBLIC_INCLUDE_DIRECTORIES})
target_include_directories(iuring_httpserver PRIVATE ${PRIVATE_INCLUDE_DIRECTORIES})

target_sources(iuring_httpserver
    PUBLIC FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES ${INCLUDE_FILES}
    PRIVATE ${SOURCE_FILES}
)

install(TARGETS iuring_httpserver FILE_SET HEADERS)

# Set target properties for public headers
set_target_properties(iuring_httpserver PROPERTIES PUBLIC_HEADER "${INCLUDE_FILES}")

find_package(GTest)

if(GTest_FOUND)
add_subdirectory(tests)
endif()