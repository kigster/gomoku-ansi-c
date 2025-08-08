#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>

// Include C-compatible headers for testing
#include "gomoku.hpp"
#include "game.h"
#include "ai.h"

class GomokuTest : public testing::Test {
protected:
    int **board;
    game_state_t *game;
    const int BOARD_SIZE = 19;
    
    void SetUp() {
        // Create a test board
        board = create_board(BOARD_SIZE);
        ASSERT_NE(board, nullptr);
        
        // Create a test configuration
        cli_config_t config = {
            .board_size = BOARD_SIZE,      // 19x19 board
            .max_depth = 4,                // AI search depth
            .move_timeout = 0,             // No timeout
            .show_help = 0,                // Don't show help
            .invalid_args = 0,             // Valid arguments
            .enable_undo = 1               // Enable undo for testing
        };
        
        // Create a test game state
        game = init_game(config);
        ASSERT_NE(game, nullptr);
        
        gomoku::populate_threat_matrix();
    }
    
    void TearDown() {
        if (board) {
            free_board(board, BOARD_SIZE);
        }
        if (game) {
            cleanup_game(game);
        }
    }
};

// Test basic board management
TEST_F(GomokuTest, BoardCreation) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            EXPECT_EQ(board[i][j], static_cast<int>(gomoku::Player::Empty));
        }
    }
}

// Test board coordinate utilities
TEST_F(GomokuTest, CoordinateUtilities) {
    EXPECT_EQ(board_to_display_coord(0), 1);
    EXPECT_EQ(board_to_display_coord(18), 19);
    EXPECT_EQ(display_to_board_coord(1), 0);
    EXPECT_EQ(display_to_board_coord(19), 18);
    
    // Test Unicode coordinate conversion
    auto coord = gomoku::get_coordinate_unicode(0);
    EXPECT_FALSE(coord.empty());
    EXPECT_EQ(coord, "❶");
}

// Test move validation
TEST_F(GomokuTest, MoveValidation) {
    // Valid moves on empty board
    EXPECT_TRUE(is_valid_move(board, 0, 0, BOARD_SIZE));
    EXPECT_TRUE(is_valid_move(board, 9, 9, BOARD_SIZE));
    EXPECT_TRUE(is_valid_move(board, 18, 18, BOARD_SIZE));
    
    // Invalid moves (out of bounds)
    EXPECT_FALSE(is_valid_move(board, -1, 0, BOARD_SIZE));
    EXPECT_FALSE(is_valid_move(board, 0, -1, BOARD_SIZE));
    EXPECT_FALSE(is_valid_move(board, 19, 0, BOARD_SIZE));
    EXPECT_FALSE(is_valid_move(board, 0, 19, BOARD_SIZE));
    
    // Invalid move on occupied cell
    board[9][9] = static_cast<int>(gomoku::Player::Cross);
    EXPECT_FALSE(is_valid_move(board, 9, 9, BOARD_SIZE));
}

// Test game state initialization
TEST_F(GomokuTest, GameStateInitialization) {
    EXPECT_EQ(game->board_size, BOARD_SIZE);
    EXPECT_EQ(game->current_player, static_cast<int>(gomoku::Player::Cross));
    EXPECT_EQ(game->game_state, static_cast<int>(gomoku::GameState::Running));
    EXPECT_EQ(game->max_depth, 4);
    EXPECT_EQ(game->move_timeout, 0);
    EXPECT_EQ(game->move_history_count, 0);
    EXPECT_EQ(game->ai_history_count, 0);
}

// Test winner detection - horizontal
TEST_F(GomokuTest, HorizontalWinDetection) {
    // Place 5 crosses stones in a row horizontally
    for (int i = 0; i < 5; i++) {
        board[7][i] = static_cast<int>(gomoku::Player::Cross);
    }
    
    EXPECT_TRUE(has_winner(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Cross)));
    EXPECT_FALSE(has_winner(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Naught)));
}

// Test winner detection - vertical
TEST_F(GomokuTest, VerticalWinDetection) {
    // Place 5 naughts stones in a column vertically
    for (int i = 0; i < 5; i++) {
        board[i][7] = static_cast<int>(gomoku::Player::Naught);
    }
    
    EXPECT_TRUE(has_winner(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Naught)));
    EXPECT_FALSE(has_winner(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Cross)));
}

// Test winner detection - diagonal
TEST_F(GomokuTest, DiagonalWinDetection) {
    // Place 5 crosses stones diagonally
    for (int i = 0; i < 5; i++) {
        board[i][i] = static_cast<int>(gomoku::Player::Cross);
    }
    
    EXPECT_TRUE(has_winner(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Cross)));
    EXPECT_FALSE(has_winner(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Naught)));
}

// Test winner detection - anti-diagonal
TEST_F(GomokuTest, AntiDiagonalWinDetection) {
    // Place 5 naughts stones in anti-diagonal
    for (int i = 0; i < 5; i++) {
        board[i][4-i] = static_cast<int>(gomoku::Player::Naught);
    }
    
    EXPECT_TRUE(has_winner(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Naught)));
    EXPECT_FALSE(has_winner(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Cross)));
}

// Test no winner scenario
TEST_F(GomokuTest, NoWinnerDetection) {
    // Place some stones but no 5 in a row
    board[7][7] = static_cast<int>(gomoku::Player::Cross);
    board[7][8] = static_cast<int>(gomoku::Player::Cross);
    board[8][7] = static_cast<int>(gomoku::Player::Naught);
    board[8][8] = static_cast<int>(gomoku::Player::Naught);
    
    EXPECT_FALSE(has_winner(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Cross)));
    EXPECT_FALSE(has_winner(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Naught)));
}

// Test evaluation function basics
TEST_F(GomokuTest, EvaluationFunction) {
    // Empty board should have score 0
    EXPECT_EQ(evaluate_position(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Cross)), 0);
    
    // Test calc_score_at for potential moves on empty board
    int score = calc_score_at(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Cross), 7, 7);
    EXPECT_GE(score, 0); // Should be non-negative
    
    // Add a stone nearby and test evaluation
    board[7][6] = static_cast<int>(gomoku::Player::Cross);
    int score_with_support = calc_score_at(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Cross), 7, 7);
    EXPECT_GT(score_with_support, score); // Should be higher with support
}

// Test evaluation function with winning position
TEST_F(GomokuTest, EvaluationWithWin) {
    // Place 5 crosses stones in a row
    for (int i = 0; i < 5; i++) {
        board[7][i] = static_cast<int>(gomoku::Player::Cross);
    }
    
    int score = evaluate_position(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Cross));
    EXPECT_EQ(score, 1000000); // Should return win score
    
    // Opponent should get lose score
    int opponent_score = evaluate_position(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Naught));
    EXPECT_EQ(opponent_score, -1000000);
}

// Test AI move evaluation
TEST_F(GomokuTest, AIMoveEvaluation) {
    // Test move interesting function
    EXPECT_TRUE(is_move_interesting(board, 9, 9, 0, BOARD_SIZE)); // Center is interesting when empty
    
    // Place a stone and test nearby positions
    board[9][9] = static_cast<int>(gomoku::Player::Cross);
    EXPECT_TRUE(is_move_interesting(board, 9, 10, 1, BOARD_SIZE)); // Adjacent is interesting
    EXPECT_FALSE(is_move_interesting(board, 0, 0, 1, BOARD_SIZE)); // Far away is not interesting
    
    // Test winning move detection
    for (int i = 0; i < 4; i++) {
        board[7][i] = static_cast<int>(gomoku::Player::Cross);
    }
    EXPECT_TRUE(is_winning_move(board, 7, 4, static_cast<int>(gomoku::Player::Cross), BOARD_SIZE)); // Completes 5 in a row
    EXPECT_FALSE(is_winning_move(board, 8, 4, static_cast<int>(gomoku::Player::Cross), BOARD_SIZE)); // Doesn't complete
}

// Test game logic functions
TEST_F(GomokuTest, GameLogicFunctions) {
    // Test making a move
    double move_time = 1.5;
    int positions_evaluated = 10;
    
    EXPECT_TRUE(make_move(game, 9, 9, static_cast<int>(gomoku::Player::Cross), move_time, positions_evaluated));
    EXPECT_EQ(game->board[9][9], static_cast<int>(gomoku::Player::Cross));
    EXPECT_EQ(game->move_history_count, 1);
    EXPECT_EQ(game->current_player, static_cast<int>(gomoku::Player::Naught)); // Should switch players
    
    // Test invalid move
    EXPECT_FALSE(make_move(game, 9, 9, static_cast<int>(gomoku::Player::Naught), move_time, positions_evaluated)); // Same position
    EXPECT_EQ(game->move_history_count, 1); // Should not increase
}

// Test undo functionality
TEST_F(GomokuTest, UndoFunctionality) {
    // Initially cannot undo
    EXPECT_FALSE(can_undo(game));
    
    // Make two moves
    make_move(game, 9, 9, static_cast<int>(gomoku::Player::Cross), 1.0, 0);
    make_move(game, 9, 10, static_cast<int>(gomoku::Player::Naught), 1.0, 5);
    
    // Now can undo
    EXPECT_TRUE(can_undo(game));
    EXPECT_EQ(game->move_history_count, 2);
    
    // Undo moves
    undo_last_moves(game);
    EXPECT_EQ(game->move_history_count, 0);
    EXPECT_EQ(game->board[9][9], static_cast<int>(gomoku::Player::Empty));
    EXPECT_EQ(game->board[9][10], static_cast<int>(gomoku::Player::Empty));
    EXPECT_EQ(game->current_player, static_cast<int>(gomoku::Player::Cross));
}

// Test other_player function
TEST_F(GomokuTest, OtherPlayerFunction) {
    EXPECT_EQ(other_player(static_cast<int>(gomoku::Player::Cross)), static_cast<int>(gomoku::Player::Naught));
    EXPECT_EQ(other_player(static_cast<int>(gomoku::Player::Naught)), static_cast<int>(gomoku::Player::Cross));
}

// Test minimax basic functionality
TEST_F(GomokuTest, MinimaxBasic) {
    // Place a few stones
    board[7][7] = static_cast<int>(gomoku::Player::Cross);
    board[7][8] = static_cast<int>(gomoku::Player::Naught);
    
    // Test minimax with depth 1
    int score = minimax(board, 1, -1000000, 1000000, 1, static_cast<int>(gomoku::Player::Naught));
    
    // Should return a reasonable score (not extreme values unless winning)
    EXPECT_GT(score, -1000000);
    EXPECT_LT(score, 1000000);
}

// Test minimax with winning position
TEST_F(GomokuTest, MinimaxWithWin) {
    // Create a winning position for crosses
    for (int i = 0; i < 5; i++) {
        board[7][i] = static_cast<int>(gomoku::Player::Cross);
    }
    
    int score = minimax(board, 1, -1000000, 1000000, 1, static_cast<int>(gomoku::Player::Cross));
    EXPECT_EQ(score, 1000001); // Should recognize the win (WIN_SCORE + depth for faster wins)
}

// Test corner cases
TEST_F(GomokuTest, CornerCases) {
    // Test edge positions - evaluate potential move at edge
    int edge_score = calc_score_at(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Cross), 0, 0);
    EXPECT_GE(edge_score, 0); // Should handle edge positions
    
    // Test center position - evaluate potential move at center
    int center_score = calc_score_at(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Cross), 9, 9);
    EXPECT_GE(center_score, 0); // Center should be non-negative
}

// Test multiple directions
TEST_F(GomokuTest, MultiDirectionThreats) {
    // Create threats in multiple directions
    board[7][7] = static_cast<int>(gomoku::Player::Cross); // Center
    board[7][6] = static_cast<int>(gomoku::Player::Cross); // Left
    board[7][8] = static_cast<int>(gomoku::Player::Cross); // Right
    board[6][7] = static_cast<int>(gomoku::Player::Cross); // Up
    board[8][7] = static_cast<int>(gomoku::Player::Cross); // Down
    
    int score = calc_score_at(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Cross), 7, 7);
    EXPECT_GT(score, 100); // Should recognize multiple threats
}

// Test blocked patterns
TEST_F(GomokuTest, BlockedPatterns) {
    // Create a pattern that's blocked by opponent
    board[7][4] = static_cast<int>(gomoku::Player::Cross);
    board[7][5] = static_cast<int>(gomoku::Player::Cross);
    board[7][6] = static_cast<int>(gomoku::Player::Cross);
    board[7][3] = static_cast<int>(gomoku::Player::Naught); // Block one side
    board[7][7] = static_cast<int>(gomoku::Player::Naught); // Block other side
    
    int score = calc_score_at(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Cross), 7, 5);
    
    // Should be lower than unblocked pattern
    // Clear the blocks and test
    board[7][3] = static_cast<int>(gomoku::Player::Empty);
    board[7][7] = static_cast<int>(gomoku::Player::Empty);
    
    int unblocked_score = calc_score_at(board, BOARD_SIZE, static_cast<int>(gomoku::Player::Cross), 7, 5);
    EXPECT_GT(unblocked_score, score);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 
