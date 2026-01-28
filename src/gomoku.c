
//
//  minimax_evaluation.c
//  gomoku - Refactored for minimax algorithm
//
//  Extracted and adapted from original heuristics.c
//  Removes move selection logic, keeps pattern evaluation
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gomoku.h"

#define NUM_DIRECTIONS 4
#define OUT_OF_BOUNDS 32
#define SEARCH_RADIUS 4
#define NEED_TO_WIN 5

// Cell values
#define AI_CELL_EMPTY 0
#define AI_CELL_CROSSES 1
#define AI_CELL_NAUGHTS -1

// Return codes
#define RT_SUCCESS 0
#define RT_FAILURE -1
#define RT_BREAK 1
#define RT_CONTINUE 0

// Threat types
#define THREAT_NOTHING 0
#define THREAT_FIVE 1
#define THREAT_STRAIGHT_FOUR 2
#define THREAT_FOUR 3
#define THREAT_THREE 4
#define THREAT_FOUR_BROKEN 5
#define THREAT_THREE_BROKEN 6
#define THREAT_TWO 7
#define THREAT_NEAR_ENEMY 8
#define THREAT_THREE_AND_FOUR 9
#define THREAT_THREE_AND_THREE 10
#define THREAT_THREE_AND_THREE_BROKEN 11

static int threat_cost[20]; 
static int threat_initialized = 0;
static int threats[NUM_DIRECTIONS];

// Function declarations
void populate_threat_matrix();
void reset_row(int *row, int size);
int other_player(int player);
int calc_score_at(int **board, int size, int player, int x, int y);
int calc_threat_in_one_dimension(int *row, int player);
int count_squares(int value, int player, int *last_square, int *hole_count, 
        int *square_count, int *contiguous_square_count, int *enemy_count);
int calc_combination_threat(int one, int two);

// Helper macros
#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

//===============================================================================
// MAIN EVALUATION FUNCTION FOR MINIMAX
//===============================================================================

/**
 * Fast incremental evaluation focusing on positions near last move
 * Much faster than full board evaluation 
 */
int evaluate_position_incremental(int **board, int size, int player, int last_x, int last_y) {
    populate_threat_matrix();

    int total_score = 0;
    int opponent = other_player(player);

    // Check for immediate win/loss first
    if (has_winner(board, size, player)) {
        return 1000000;  // Win
    }
    if (has_winner(board, size, opponent)) {
        return -1000000; // Loss
    }

    // Only evaluate positions within radius of the last move for speed
    int eval_radius = 3; // Increased radius for better accuracy
    int min_x = max(0, last_x - eval_radius);
    int max_x = min(size - 1, last_x + eval_radius);
    int min_y = max(0, last_y - eval_radius);
    int max_y = min(size - 1, last_y + eval_radius);

    for (int i = min_x; i <= max_x; i++) {
        for (int j = min_y; j <= max_y; j++) {
            if (board[i][j] == player) {
                total_score += calc_score_at(board, size, player, i, j);
            } else if (board[i][j] == opponent) {
                total_score -= calc_score_at(board, size, opponent, i, j);
            }
        }
    }

    return total_score;
}

/**
 * Original full board evaluation function - kept for fallback
 */
int evaluate_position(int **board, int size, int player) {
    populate_threat_matrix();

    int total_score = 0;
    int opponent = other_player(player);

    // Check for immediate win/loss
    if (has_winner(board, size, player)) {
        return 1000000;  // Win
    }
    if (has_winner(board, size, opponent)) {
        return -1000000; // Loss
    }

    // Evaluate all occupied positions
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            if (board[i][j] == player) {
                total_score += calc_score_at(board, size, player, i, j);
            } else if (board[i][j] == opponent) {
                total_score -= calc_score_at(board, size, opponent, i, j);
            }
        }
    }

    return total_score;
}

/**
 * Simple win detection - checks if player has 5 in a row anywhere on board
 */
int has_winner(int **board, int size, int player) {
    // Check all positions for 5 in a row
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            if (board[i][j] == player) {
                // Check all 4 directions from this position
                int directions[4][2] = {{1,0}, {0,1}, {1,1}, {1,-1}};
                for (int d = 0; d < 4; d++) {
                    int dx = directions[d][0];
                    int dy = directions[d][1];
                    int count = 1;

                    // Count in positive direction
                    int x = i + dx, y = j + dy;
                    while (x >= 0 && x < size && y >= 0 && y < size && board[x][y] == player) {
                        count++;
                        x += dx;
                        y += dy;
                    }

                    // Count in negative direction
                    x = i - dx;
                    y = j - dy;
                    while (x >= 0 && x < size && y >= 0 && y < size && board[x][y] == player) {
                        count++;
                        x -= dx;
                        y -= dy;
                    }

                    if (count == 5) {
                        return 1; // Winner found
                    }
                }
            }
        }
    }
    return 0; // No winner
}

//===============================================================================
// PATTERN ANALYSIS FUNCTIONS (CORE LOGIC FROM ORIGINAL)
//===============================================================================

int other_player(int player) {
    return -player;
}

void reset_row(int *row, int size) {
    for (int i = 0; i < size; i++) {
        row[i] = OUT_OF_BOUNDS;
    }
}

/**
 * Calculates the threat score for placing a stone at position (x,y)
 * This is the core pattern recognition function from the original code
 */
int calc_score_at(int **board, int size, int player, int x, int y) {
    int min_x = max(x - SEARCH_RADIUS, 0);
    int max_x = min(x + SEARCH_RADIUS, size - 1);
    int min_y = max(y - SEARCH_RADIUS, 0);
    int max_y = min(y + SEARCH_RADIUS, size - 1);

    int row_size = SEARCH_RADIUS * 2 + 1;
    int row[SEARCH_RADIUS * 2 + 1];
    int i, index;
    int score = 0;

    memset(threats, 0, NUM_DIRECTIONS * sizeof(int));

    // Analyze horizontal direction
    reset_row(row, row_size);
    row[SEARCH_RADIUS] = player; // Place the stone at center
    for (i = x + 1, index = SEARCH_RADIUS + 1; i <= max_x; i++, index++) {
        row[index] = board[i][y];
    }
    for (i = x - 1, index = SEARCH_RADIUS - 1; i >= min_x; i--, index--) {
        row[index] = board[i][y];
    }
    threats[0] = calc_threat_in_one_dimension(row, player);

    // Analyze vertical direction
    reset_row(row, row_size);
    row[SEARCH_RADIUS] = player; // Place the stone at center
    for (i = y + 1, index = SEARCH_RADIUS + 1; i <= max_y; i++, index++) {
        row[index] = board[x][i];
    }
    for (i = y - 1, index = SEARCH_RADIUS - 1; i >= min_y; i--, index--) {
        row[index] = board[x][i];
    }
    threats[1] = calc_threat_in_one_dimension(row, player);

    // Analyze diagonal (top-left to bottom-right)
    reset_row(row, row_size);
    row[SEARCH_RADIUS] = player; // Place the stone at center
    int j;
    for (i = x + 1, j = y + 1, index = SEARCH_RADIUS + 1; 
            i <= max_x && j <= max_y; i++, j++, index++) {
        row[index] = board[i][j];
    }
    for (i = x - 1, j = y - 1, index = SEARCH_RADIUS - 1; 
            i >= min_x && j >= min_y; i--, j--, index--) {
        row[index] = board[i][j];
    }
    threats[2] = calc_threat_in_one_dimension(row, player);

    // Analyze diagonal (bottom-left to top-right)
    reset_row(row, row_size);
    row[SEARCH_RADIUS] = player; // Place the stone at center
    for (i = x + 1, j = y - 1, index = SEARCH_RADIUS + 1; 
            i <= max_x && j >= min_y; i++, j--, index++) {
        row[index] = board[i][j];
    }
    for (i = x - 1, j = y + 1, index = SEARCH_RADIUS - 1; 
            i >= min_x && j <= max_y; i--, j++, index--) {
        row[index] = board[i][j];
    }
    threats[3] = calc_threat_in_one_dimension(row, player);

    // Calculate total score including combinations
    for (i = 0; i < NUM_DIRECTIONS; i++) {
        score += threat_cost[threats[i]];
        for (j = i + 1; j < NUM_DIRECTIONS; j++) {
            score += calc_combination_threat(threats[i], threats[j]);
        }
    }

    return score;
}

/**
 * Analyzes a single line/direction for threat patterns
 * The stone of interest is assumed to be at the center of the array
 */
int calc_threat_in_one_dimension(int *row, int player) {
    int player_square_count = 1; // Include the center stone
    int player_contiguous_square_count = 1;
    int enemy_count = 0;
    int right_hole_count = 0, left_hole_count = 0;
    int last_square;
    int i;

    // Walk from center to the right
    for (i = SEARCH_RADIUS + 1, last_square = player; 
            i <= SEARCH_RADIUS*2 && row[i] != OUT_OF_BOUNDS; i++) {
        if (count_squares(row[i], player, &last_square, &right_hole_count, 
                    &player_square_count, &player_contiguous_square_count, &enemy_count) == RT_BREAK) {
            break;
        }
    }

    // Walk from center to the left
    for (i = SEARCH_RADIUS - 1, last_square = player; 
            i >= 0 && row[i] != OUT_OF_BOUNDS; i--) {
        if (count_squares(row[i], player, &last_square, &left_hole_count, 
                    &player_square_count, &player_contiguous_square_count, &enemy_count) == RT_BREAK) {
            break;
        }
    }

    int holes = left_hole_count + right_hole_count;
    int total = holes + player_square_count;
    int threat = THREAT_NOTHING;

    // Determine threat level based on pattern
    if (player_contiguous_square_count >= NEED_TO_WIN) {
        threat = THREAT_FIVE;
    } else if (player_contiguous_square_count == 4 && right_hole_count > 0 && left_hole_count > 0) {
        threat = THREAT_STRAIGHT_FOUR;
    } else if (player_contiguous_square_count == 4 && (right_hole_count > 0 || left_hole_count > 0)) {
        threat = THREAT_FOUR;
    } else if (player_contiguous_square_count == 3 && (right_hole_count > 0 && left_hole_count > 0)) {
        threat = THREAT_THREE;
    } else if (player_square_count >= 4 && (right_hole_count > 0 || left_hole_count > 0) && total >= 5) {
        threat = THREAT_FOUR_BROKEN;
    } else if (player_square_count >= 3 && (right_hole_count > 0 || left_hole_count > 0) && total >= 5) {
        threat = THREAT_THREE_BROKEN;
    } else if (player_contiguous_square_count >= 2 && (right_hole_count > 0 || left_hole_count > 0) && total >= 4) { 
        threat = THREAT_TWO;
    } else if (player_contiguous_square_count >= 1 && (right_hole_count == 0 || left_hole_count == 0) && enemy_count > 0) { 
        threat = THREAT_NEAR_ENEMY;
    }

    return threat;
}

int count_squares(int value, int player, int *last_square, int *hole_count, 
        int *square_count, int *contiguous_square_count, int *enemy_count) {
    if (value == player) {
        (*square_count)++;
        if (*hole_count == 0) { 
            (*contiguous_square_count)++; 
        }
    } else if (value == AI_CELL_EMPTY) {
        if (*last_square == AI_CELL_EMPTY) {
            return RT_BREAK; // Two consecutive holes - stop
        }
        (*hole_count)++;
    } else if (value == -player) {
        (*enemy_count)++;
        return RT_BREAK; // Hit enemy stone - stop
    }

    *last_square = value;
    return RT_CONTINUE;
}

int calc_combination_threat(int one, int two) {
    // Ensure one <= two for simpler comparisons
    if (one > two) {
        int temp = one;
        one = two;
        two = temp;
    }

    // WINNING COMBINATIONS (opponent can only block one threat)

    // Open three + any four = winning (opponent can't block both)
    if (one == THREAT_THREE && (two == THREAT_FOUR || two == THREAT_STRAIGHT_FOUR || two == THREAT_FOUR_BROKEN)) {
        return threat_cost[THREAT_THREE_AND_FOUR];
    }

    // Two open threes = winning
    if (one == THREAT_THREE && two == THREAT_THREE) {
        return threat_cost[THREAT_THREE_AND_THREE];
    }

    // VERY STRONG COMBINATIONS

    // Broken three + any four = very strong (likely winning)
    if (one == THREAT_THREE_BROKEN && (two == THREAT_FOUR || two == THREAT_STRAIGHT_FOUR || two == THREAT_FOUR_BROKEN)) {
        return threat_cost[THREAT_THREE_AND_THREE];  // Treat as strong as double three
    }

    // Open three + broken three = strong threat
    if (one == THREAT_THREE && two == THREAT_THREE_BROKEN) {
        return threat_cost[THREAT_THREE_AND_THREE_BROKEN];
    }

    // Two broken threes with potential = moderate threat
    if (one == THREAT_THREE_BROKEN && two == THREAT_THREE_BROKEN) {
        return threat_cost[THREAT_THREE_AND_THREE_BROKEN] / 2;
    }

    // Four + Four = extremely dangerous (multiple must-block threats)
    if ((one == THREAT_FOUR || one == THREAT_FOUR_BROKEN) &&
        (two == THREAT_FOUR || two == THREAT_FOUR_BROKEN)) {
        return threat_cost[THREAT_THREE_AND_FOUR];  // Treat as winning
    }

    // Any four + two = developing threat
    if ((one == THREAT_TWO) && (two == THREAT_FOUR || two == THREAT_FOUR_BROKEN)) {
        return 500;
    }

    // Open three + two = developing threat
    if (one == THREAT_TWO && two == THREAT_THREE) {
        return 300;
    }

    return 0;
}

void populate_threat_matrix() {
    if (threat_initialized > 0) {
        return;
    }

    threat_initialized = 1;

    // Single threats - base values
    threat_cost[THREAT_NOTHING]                 = 0;
    threat_cost[THREAT_FIVE]                    = 100000;   // Winning position
    threat_cost[THREAT_STRAIGHT_FOUR]           = 50000;    // Open four = guaranteed win next turn
    threat_cost[THREAT_FOUR]                    = 10000;    // Closed four = MUST block or lose!
    threat_cost[THREAT_FOUR_BROKEN]             = 8000;     // Four with hole = still must block
    threat_cost[THREAT_THREE]                   = 1000;     // Open three = serious threat
    threat_cost[THREAT_THREE_BROKEN]            = 200;      // Three with hole = developing threat
    threat_cost[THREAT_TWO]                     = 50;       // Open two = potential
    threat_cost[THREAT_NEAR_ENEMY]              = 10;       // Positional value

    // Combination threats - these are nearly winning positions!
    // If you have an open three + any four, opponent can only block one
    threat_cost[THREAT_THREE_AND_FOUR]          = 45000;    // Nearly as good as straight four
    threat_cost[THREAT_THREE_AND_THREE]         = 40000;    // Double open three = winning
    threat_cost[THREAT_THREE_AND_THREE_BROKEN]  = 5000;     // Weaker but still dangerous
}

//===============================================================================
// EXAMPLE USAGE WITH MINIMAX
//===============================================================================

/**
 * Example of how to use this evaluation function with minimax
 */
int minimax_example(int **board, int size, int depth, int alpha, int beta, 
        int maximizing_player, int ai_player) {

    // Base case: evaluate leaf node
    if (depth == 0 || has_winner(board, size, ai_player) || has_winner(board, size, other_player(ai_player))) {
        return evaluate_position(board, size, ai_player);
    }

    int current_player = maximizing_player ? ai_player : other_player(ai_player);

    if (maximizing_player) {
        int max_eval = -1000000;

        // Try all empty cells
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size; j++) {
                if (board[i][j] == AI_CELL_EMPTY) {
                    // Make move
                    board[i][j] = current_player;

                    // Recursive call
                    int eval = minimax_example(board, size, depth - 1, alpha, beta, 0, ai_player);

                    // Undo move
                    board[i][j] = AI_CELL_EMPTY;

                    max_eval = max(max_eval, eval);
                    alpha = max(alpha, eval);

                    if (beta <= alpha) {
                        break; // Alpha-beta pruning
                    }
                }
            }
        }
        return max_eval;

    } else {
        int min_eval = 1000000;

        // Try all empty cells
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size; j++) {
                if (board[i][j] == AI_CELL_EMPTY) {
                    // Make move
                    board[i][j] = current_player;

                    // Recursive call
                    int eval = minimax_example(board, size, depth - 1, alpha, beta, 1, ai_player);

                    // Undo move
                    board[i][j] = AI_CELL_EMPTY;

                    min_eval = min(min_eval, eval);
                    beta = min(beta, eval);

                    if (beta <= alpha) {
                        break; // Alpha-beta pruning
                    }
                }
            }
        }
        return min_eval;
    }
}

