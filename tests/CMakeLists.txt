# Find or build Google Test
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    # Check if Google Test is available in the googletest subdirectory
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/googletest)
        message(STATUS "Using local Google Test from tests/googletest")
        
        # Add Google Test subdirectory but don't install it
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
        
        add_subdirectory(googletest EXCLUDE_FROM_ALL)
        
        # Create aliases for consistency
        add_library(GTest::gtest ALIAS gtest)
        add_library(GTest::gtest_main ALIAS gtest_main)
    else()
        message(FATAL_ERROR "Google Test not found. Please run 'git submodule update --init' to download it.")
    endif()
endif()

# Source files for the test (excluding main.cpp and ui.cpp which are not needed for testing)
set(TEST_SOURCES
    gomoku_test.cpp
    ../src/gomoku.cpp
    ../src/board.cpp
    ../src/game.cpp
    ../src/ai.cpp
)

# Create the test executable
add_executable(test_gomoku ${TEST_SOURCES})

# Link with Google Test and math library
target_link_libraries(test_gomoku 
    GTest::gtest 
    GTest::gtest_main
    ${MATH_LIB}
)

# Include directories
target_include_directories(test_gomoku PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# Set output directory to project root (same as Makefile)
set_target_properties(test_gomoku PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Add test to CTest
add_test(NAME gomoku_tests COMMAND test_gomoku)