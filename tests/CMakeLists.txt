
find_package(GTest REQUIRED)

add_executable(iuring_http_unittests tests.cpp)
target_include_directories(iuring_http_unittests PUBLIC ${GTEST_INCLUDE_DIRS})
target_link_libraries(iuring_http_unittests iuring_httpserver  -lgtest -lgmock -lgtest_main )


add_test(NAME googletest
         COMMAND iuring_http_unittests)