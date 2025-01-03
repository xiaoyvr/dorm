# Create the test executable
add_executable(dorm_tests dorm_tests.cc)

# Link Google Test libraries
find_package(GTest REQUIRED)
target_link_libraries(dorm_tests PRIVATE GTest::gtest GTest::gtest_main)

target_include_directories(dorm_tests PRIVATE
    ../src
)

# Add the test executable to CTest
enable_testing()

include(GoogleTest)
gtest_discover_tests(dorm_tests)


# add_test(NAME RunAllTests COMMAND hello_test)