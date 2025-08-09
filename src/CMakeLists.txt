# Source files for the gomoku executable (C++23)
set(GOMOKU_SOURCES
    main.cpp
    gomoku.cpp
    board.cpp
    game.cpp
    ai.cpp
    ui.cpp
    cli.cpp
)

# Create the gomoku executable
add_executable(gomoku ${GOMOKU_SOURCES})

# Link math library
target_link_libraries(gomoku ${MATH_LIB})

# Include directories
target_include_directories(gomoku PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Set output directory to project root (same as Makefile)
set_target_properties(gomoku PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)