//
//  board.c
//  gomoku - Board management and coordinate utilities
//
//  Handles board creation, destruction, validation, and coordinate conversion
//

#include <stdio.h>
#include <stdlib.h>
#include "board.h"

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
    return x >= 0 && x < size && y >= 0 && y < size && board[x][y] == AI_CELL_EMPTY;
}

//===============================================================================
// COORDINATE UTILITIES
//===============================================================================

const char* get_coordinate_unicode(int index) {
    // Convert 0-based index to 1-based display with Unicode black circled characters
    // Use consistent black circled numbers for all positions 1-19
    static const char* coords[] = {
        "❶", "❷", "❸", "❹", "❺", "❻", "❼", "❽", "❾", "❿",
        "⓫", "⓬", "⓭", "⓮", "⓯", "⓰", "⓱", "⓲", "⓳"
    };
    
    if (index >= 0 && index < 19) {
        return coords[index];
    }
    return "?";
}

int board_to_display_coord(int board_coord) {
    return board_coord + 1;
}

int display_to_board_coord(int display_coord) {
    return display_coord - 1;
} 