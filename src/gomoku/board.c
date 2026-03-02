//
//  board.c
//  gomoku - Board management and coordinate utilities
//
//  Handles board creation, destruction, validation, and coordinate conversion
//

#include "board.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//===============================================================================
// BOARD MANAGEMENT FUNCTIONS
//===============================================================================

int **create_board(int size) {
  int **new_board = malloc(size * sizeof(int *));
  if (!new_board) {
    return NULL;
  }

  for (int i = 0; i < size; i++) {
    new_board[i] = malloc(size * sizeof(int));
    if (!new_board[i]) {
      // Free previously allocated rows on failure
      for (int j = 0; j < i; j++) {
        free(new_board[j]);
      }
      free(new_board);
      return NULL;
    }

    // Initialize cells to empty
    for (int j = 0; j < size; j++) {
      new_board[i][j] = AI_CELL_EMPTY;
    }
  }

  return new_board;
}

void free_board(int **board, int size) {
  if (!board) {
    return;
  }

  for (int i = 0; i < size; i++) {
    free(board[i]);
  }
  free(board);
}

int is_valid_move(int **board, int x, int y, int size) {
  return x >= 0 && x < size && y >= 0 && y < size &&
         board[x][y] == AI_CELL_EMPTY;
}

//===============================================================================
// COORDINATE UTILITIES
//===============================================================================

const char *get_coordinate_unicode(int index) {
  // Convert 0-based index to 1-based display with Unicode crosses circled
  // characters Use consistent crosses circled numbers for all positions 1-19
  static const char *coords[] = {"❶", "❷", "❸", "❹", "❺", "❻", "❼",
                                 "❽", "❾", "❿", "⓫", "⓬", "⓭", "⓮",
                                 "⓯", "⓰", "⓱", "⓲", "⓳"};

  if (index >= 0 && index < 19) {
    return coords[index];
  }
  return "?";
}

int board_to_display_coord(int board_coord) { return board_coord + 1; }

int display_to_board_coord(int display_coord) { return display_coord - 1; }

/**
 * Converts board coordinates (row x, column y) to short notation (e.g. "A2").
 * Column uses letters A–T excluding I; row is 1-based.
 */
void board_coord_to_notation(int row_x, int col_y, char *buf, size_t size) {
  if (!buf || size < 4)
    return;
  int letter_idx = (col_y < 8) ? col_y : col_y + 1; /* skip I */
  char col_letter = (char)('A' + letter_idx);
  (void)snprintf(buf, size, "%c%d", col_letter, row_x + 1);
}

/**
 * Converts short notation (e.g. "A2", "J10") to board coordinates (row_x,
 * col_y). Column: A–H = 0–7, J–T = 8–18 (I excluded). Row: 1-based in notation.
 *
 * @param notation String like "A2" or "J10"
 * @param row_x Output: 0-based row (optional, may be NULL)
 * @param col_y Output: 0-based column (optional, may be NULL)
 * @return 1 on success, 0 on parse failure
 */
int notation_to_board_coord(const char *notation, int *row_x, int *col_y) {
  if (!notation || (!row_x && !col_y))
    return 0;
  size_t len = strlen(notation);
  if (len < 2 || len > 4)
    return 0;
  char col_char = (char)toupper((unsigned char)notation[0]);
  int col_idx = -1;
  if (col_char >= 'A' && col_char <= 'H')
    col_idx = col_char - 'A';
  else if (col_char >= 'J' && col_char <= 'T')
    col_idx = col_char - 'J' + 8;
  else
    return 0;
  int row_1based = 0;
  for (size_t i = 1; i < len; i++) {
    if (notation[i] < '0' || notation[i] > '9')
      return 0;
    row_1based = row_1based * 10 + (notation[i] - '0');
  }
  if (row_1based < 1 || row_1based > 19)
    return 0;
  if (row_x)
    *row_x = row_1based - 1;
  if (col_y)
    *col_y = col_idx;
  return 1;
}
