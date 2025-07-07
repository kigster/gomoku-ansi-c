//
//  board.h
//  gomoku - Board management and coordinate utilities
//
//  Handles board creation, destruction, validation, and coordinate conversion
//

#ifndef BOARD_H
#define BOARD_H

#include "gomoku.h"

//===============================================================================
// BOARD MANAGEMENT FUNCTIONS
//===============================================================================

/**
 * Creates a new game board with the specified size.
 * 
 * @param size The size of the square board (e.g., 15 or 19)
 * @return 2D array representing the board, or NULL on failure
 */
int **create_board(int size);

/**
 * Frees the memory allocated for a game board.
 * 
 * @param board The board to free
 * @param size The size of the board
 */
void free_board(int **board, int size);

/**
 * Checks if a move is valid at the given position.
 * 
 * @param board The game board
 * @param x Row coordinate
 * @param y Column coordinate
 * @param size Board size
 * @return 1 if valid, 0 if invalid
 */
int is_valid_move(int **board, int x, int y, int size);

//===============================================================================
// COORDINATE UTILITIES
//===============================================================================

/**
 * Converts a 0-based index to Unicode coordinate display.
 * 
 * @param index 0-based coordinate index
 * @return Unicode string representation of the coordinate
 */
const char* get_coordinate_unicode(int index);

/**
 * Converts board coordinates to display coordinates (1-based).
 * 
 * @param board_coord 0-based board coordinate
 * @return 1-based display coordinate
 */
int board_to_display_coord(int board_coord);

/**
 * Converts display coordinates to board coordinates (0-based).
 * 
 * @param display_coord 1-based display coordinate
 * @return 0-based board coordinate
 */
int display_to_board_coord(int display_coord);

#endif // BOARD_H 