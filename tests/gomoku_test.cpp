#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>

// Include the C header in C++ context
extern "C" {
    #include "gomoku.h"
}

class GomokuTest : public testing::Test {
protected:
    int **board;
    const int BOARD_SIZE = 19;
    
    void SetUp() {
        // Create a test board
        board = (int**)malloc(BOARD_SIZE * sizeof(int*));
        for (int i = 0; i < BOARD_SIZE; i++) {
            board[i] = (int*)malloc(BOARD_SIZE * sizeof(int));
            for (int j = 0; j < BOARD_SIZE; j++) {
                board[i][j] = AI_CELL_EMPTY;
            }
        }
        populate_threat_matrix();
    }
    
    void TearDown() {
        for (int i = 0; i < BOARD_SIZE; i++) {
            free(board[i]);
        }
        free(board);
    }
};

// Test basic board initialization
TEST_F(GomokuTest, BoardInitialization) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            EXPECT_EQ(board[i][j], AI_CELL_EMPTY);
        }
    }
}

// Test winner detection - horizontal
TEST_F(GomokuTest, HorizontalWinDetection) {
    // Place 5 black stones in a row horizontally
    for (int i = 0; i < 5; i++) {
        board[7][i] = AI_CELL_BLACK;
    }
    
    EXPECT_TRUE(has_winner(board, BOARD_SIZE, AI_CELL_BLACK));
    EXPECT_FALSE(has_winner(board, BOARD_SIZE, AI_CELL_WHITE));
}

// Test winner detection - vertical
TEST_F(GomokuTest, VerticalWinDetection) {
    // Place 5 white stones in a column vertically
    for (int i = 0; i < 5; i++) {
        board[i][7] = AI_CELL_WHITE;
    }
    
    EXPECT_TRUE(has_winner(board, BOARD_SIZE, AI_CELL_WHITE));
    EXPECT_FALSE(has_winner(board, BOARD_SIZE, AI_CELL_BLACK));
}

// Test winner detection - diagonal
TEST_F(GomokuTest, DiagonalWinDetection) {
    // Place 5 black stones diagonally
    for (int i = 0; i < 5; i++) {
        board[i][i] = AI_CELL_BLACK;
    }
    
    EXPECT_TRUE(has_winner(board, BOARD_SIZE, AI_CELL_BLACK));
    EXPECT_FALSE(has_winner(board, BOARD_SIZE, AI_CELL_WHITE));
}

// Test winner detection - anti-diagonal
TEST_F(GomokuTest, AntiDiagonalWinDetection) {
    // Place 5 white stones in anti-diagonal
    for (int i = 0; i < 5; i++) {
        board[i][4-i] = AI_CELL_WHITE;
    }
    
    EXPECT_TRUE(has_winner(board, BOARD_SIZE, AI_CELL_WHITE));
    EXPECT_FALSE(has_winner(board, BOARD_SIZE, AI_CELL_BLACK));
}

// Test no winner scenario
TEST_F(GomokuTest, NoWinnerDetection) {
    // Place some stones but no 5 in a row
    board[7][7] = AI_CELL_BLACK;
    board[7][8] = AI_CELL_BLACK;
    board[8][7] = AI_CELL_WHITE;
    board[8][8] = AI_CELL_WHITE;
    
    EXPECT_FALSE(has_winner(board, BOARD_SIZE, AI_CELL_BLACK));
    EXPECT_FALSE(has_winner(board, BOARD_SIZE, AI_CELL_WHITE));
}

// Test evaluation function basics
TEST_F(GomokuTest, EvaluationFunction) {
    // Empty board should have score 0
    EXPECT_EQ(evaluate_position(board, BOARD_SIZE, AI_CELL_BLACK), 0);
    
    // Test calc_score_at for potential moves on empty board
    int score = calc_score_at(board, BOARD_SIZE, AI_CELL_BLACK, 7, 7);
    EXPECT_GE(score, 0); // Should be non-negative
    
    // Add a stone nearby and test evaluation
    board[7][6] = AI_CELL_BLACK;
    int score_with_support = calc_score_at(board, BOARD_SIZE, AI_CELL_BLACK, 7, 7);
    EXPECT_GT(score_with_support, score); // Should be higher with support
}

// Test evaluation function with winning position
TEST_F(GomokuTest, EvaluationWithWin) {
    // Place 5 black stones in a row
    for (int i = 0; i < 5; i++) {
        board[7][i] = AI_CELL_BLACK;
    }
    
    int score = evaluate_position(board, BOARD_SIZE, AI_CELL_BLACK);
    EXPECT_EQ(score, 1000000); // Should return win score
    
    // Opponent should get lose score
    int opponent_score = evaluate_position(board, BOARD_SIZE, AI_CELL_WHITE);
    EXPECT_EQ(opponent_score, -1000000);
}

// Test threat detection
TEST_F(GomokuTest, ThreatDetection) {
    // Create a 4-in-a-row threat
    board[7][3] = AI_CELL_BLACK;
    board[7][4] = AI_CELL_BLACK;
    board[7][5] = AI_CELL_BLACK;
    board[7][6] = AI_CELL_BLACK;
    // Empty at 7,2 and 7,7 - should be high threat
    
    int score = calc_score_at(board, BOARD_SIZE, AI_CELL_BLACK, 7, 3);
    EXPECT_GT(score, 1000); // Should be high threat score
}

// Test pattern recognition
TEST_F(GomokuTest, PatternRecognition) {
    // Test different patterns
    
    // Two stones with gaps - evaluate the gap position
    board[7][4] = AI_CELL_BLACK;
    board[7][6] = AI_CELL_BLACK;
    
    int score_between = calc_score_at(board, BOARD_SIZE, AI_CELL_BLACK, 7, 5);
    EXPECT_GT(score_between, 0); // Should recognize potential
    
    // Add more stones to create a stronger pattern
    board[7][3] = AI_CELL_BLACK;
    int score_stronger = calc_score_at(board, BOARD_SIZE, AI_CELL_BLACK, 7, 5);
    EXPECT_GT(score_stronger, score_between); // Should be higher threat
}

// Test other_player function
TEST_F(GomokuTest, OtherPlayerFunction) {
    EXPECT_EQ(other_player(AI_CELL_BLACK), AI_CELL_WHITE);
    EXPECT_EQ(other_player(AI_CELL_WHITE), AI_CELL_BLACK);
}

// Test minimax basic functionality
TEST_F(GomokuTest, MinimaxBasic) {
    // Place a few stones
    board[7][7] = AI_CELL_BLACK;
    board[7][8] = AI_CELL_WHITE;
    
    // Test minimax with depth 1
    int score = minimax_example(board, BOARD_SIZE, 1, -1000000, 1000000, 1, AI_CELL_WHITE);
    
    // Should return a reasonable score (not extreme values unless winning)
    EXPECT_GT(score, -1000000);
    EXPECT_LT(score, 1000000);
}

// Test minimax with winning position
TEST_F(GomokuTest, MinimaxWithWin) {
    // Create a winning position for black
    for (int i = 0; i < 5; i++) {
        board[7][i] = AI_CELL_BLACK;
    }
    
    int score = minimax_example(board, BOARD_SIZE, 1, -1000000, 1000000, 1, AI_CELL_BLACK);
    EXPECT_EQ(score, 1000000); // Should recognize the win
}

// Test corner cases
TEST_F(GomokuTest, CornerCases) {
    // Test edge positions - evaluate potential move at edge
    int edge_score = calc_score_at(board, BOARD_SIZE, AI_CELL_BLACK, 0, 0);
    EXPECT_GE(edge_score, 0); // Should handle edge positions
    
    // Test center position - evaluate potential move at center
    int center_score = calc_score_at(board, BOARD_SIZE, AI_CELL_BLACK, 7, 7);
    EXPECT_GE(center_score, 0); // Center should be non-negative
    
    // Both positions should be valid (non-negative)
    EXPECT_GE(center_score, 0);
    EXPECT_GE(edge_score, 0);
}

// Test multiple directions
TEST_F(GomokuTest, MultiDirectionThreats) {
    // Create threats in multiple directions
    board[7][7] = AI_CELL_BLACK; // Center
    board[7][6] = AI_CELL_BLACK; // Left
    board[7][8] = AI_CELL_BLACK; // Right
    board[6][7] = AI_CELL_BLACK; // Up
    board[8][7] = AI_CELL_BLACK; // Down
    
    int score = calc_score_at(board, BOARD_SIZE, AI_CELL_BLACK, 7, 7);
    EXPECT_GT(score, 100); // Should recognize multiple threats
}

// Test blocked patterns
TEST_F(GomokuTest, BlockedPatterns) {
    // Create a pattern that's blocked by opponent
    board[7][4] = AI_CELL_BLACK;
    board[7][5] = AI_CELL_BLACK;
    board[7][6] = AI_CELL_BLACK;
    board[7][3] = AI_CELL_WHITE; // Block one side
    board[7][7] = AI_CELL_WHITE; // Block other side
    
    int score = calc_score_at(board, BOARD_SIZE, AI_CELL_BLACK, 7, 5);
    
    // Should be lower than unblocked pattern
    // Clear the blocks and test
    board[7][3] = AI_CELL_EMPTY;
    board[7][7] = AI_CELL_EMPTY;
    
    int unblocked_score = calc_score_at(board, BOARD_SIZE, AI_CELL_BLACK, 7, 5);
    EXPECT_GT(unblocked_score, score);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 