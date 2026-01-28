#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>

// Include the C headers in C++ context
extern "C" {
#include "ai.h"
#include "board.h"
#include "cli.h"
#include "game.h"
#include "gomoku.h"
}

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
        .board_size = BOARD_SIZE, // 19x19 board
        .max_depth = 4,           // AI search depth
        .move_timeout = 0,        // No timeout
        .show_help = 0,           // Don't show help
        .invalid_args = 0,        // Valid arguments
        .enable_undo = 1          // Enable undo for testing
    };

    // Create a test game state
    game = init_game(config);
    ASSERT_NE(game, nullptr);

    populate_threat_matrix();
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
      EXPECT_EQ(board[i][j], AI_CELL_EMPTY);
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
  const char *coord = get_coordinate_unicode(0);
  EXPECT_NE(coord, nullptr);
  EXPECT_STREQ(coord, "❶");
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
  board[9][9] = AI_CELL_CROSSES;
  EXPECT_FALSE(is_valid_move(board, 9, 9, BOARD_SIZE));
}

// Test game state initialization
TEST_F(GomokuTest, GameStateInitialization) {
  EXPECT_EQ(game->board_size, BOARD_SIZE);
  EXPECT_EQ(game->current_player, AI_CELL_CROSSES);
  EXPECT_EQ(game->game_state, GAME_RUNNING);
  EXPECT_EQ(game->max_depth, 4);
  EXPECT_EQ(game->move_timeout, 0);
  EXPECT_EQ(game->move_history_count, 0);
  EXPECT_EQ(game->ai_history_count, 0);
}

// Test winner detection - horizontal
TEST_F(GomokuTest, HorizontalWinDetection) {
  // Place 5 crosses stones in a row horizontally
  for (int i = 0; i < 5; i++) {
    board[7][i] = AI_CELL_CROSSES;
  }

  EXPECT_TRUE(has_winner(board, BOARD_SIZE, AI_CELL_CROSSES));
  EXPECT_FALSE(has_winner(board, BOARD_SIZE, AI_CELL_NAUGHTS));
}

// Test winner detection - vertical
TEST_F(GomokuTest, VerticalWinDetection) {
  // Place 5 naughts stones in a column vertically
  for (int i = 0; i < 5; i++) {
    board[i][7] = AI_CELL_NAUGHTS;
  }

  EXPECT_TRUE(has_winner(board, BOARD_SIZE, AI_CELL_NAUGHTS));
  EXPECT_FALSE(has_winner(board, BOARD_SIZE, AI_CELL_CROSSES));
}

// Test winner detection - diagonal
TEST_F(GomokuTest, DiagonalWinDetection) {
  // Place 5 crosses stones diagonally
  for (int i = 0; i < 5; i++) {
    board[i][i] = AI_CELL_CROSSES;
  }

  EXPECT_TRUE(has_winner(board, BOARD_SIZE, AI_CELL_CROSSES));
  EXPECT_FALSE(has_winner(board, BOARD_SIZE, AI_CELL_NAUGHTS));
}

// Test winner detection - anti-diagonal
TEST_F(GomokuTest, AntiDiagonalWinDetection) {
  // Place 5 naughts stones in anti-diagonal
  for (int i = 0; i < 5; i++) {
    board[i][4 - i] = AI_CELL_NAUGHTS;
  }

  EXPECT_TRUE(has_winner(board, BOARD_SIZE, AI_CELL_NAUGHTS));
  EXPECT_FALSE(has_winner(board, BOARD_SIZE, AI_CELL_CROSSES));
}

// Test no winner scenario
TEST_F(GomokuTest, NoWinnerDetection) {
  // Place some stones but no 5 in a row
  board[7][7] = AI_CELL_CROSSES;
  board[7][8] = AI_CELL_CROSSES;
  board[8][7] = AI_CELL_NAUGHTS;
  board[8][8] = AI_CELL_NAUGHTS;

  EXPECT_FALSE(has_winner(board, BOARD_SIZE, AI_CELL_CROSSES));
  EXPECT_FALSE(has_winner(board, BOARD_SIZE, AI_CELL_NAUGHTS));
}

// Test evaluation function basics
TEST_F(GomokuTest, EvaluationFunction) {
  // Empty board should have score 0
  EXPECT_EQ(evaluate_position(board, BOARD_SIZE, AI_CELL_CROSSES), 0);

  // Test calc_score_at for potential moves on empty board
  int score = calc_score_at(board, BOARD_SIZE, AI_CELL_CROSSES, 7, 7);
  EXPECT_GE(score, 0); // Should be non-negative

  // Add a stone nearby and test evaluation
  board[7][6] = AI_CELL_CROSSES;
  int score_with_support =
      calc_score_at(board, BOARD_SIZE, AI_CELL_CROSSES, 7, 7);
  EXPECT_GT(score_with_support, score); // Should be higher with support
}

// Test evaluation function with winning position
TEST_F(GomokuTest, EvaluationWithWin) {
  // Place 5 crosses stones in a row
  for (int i = 0; i < 5; i++) {
    board[7][i] = AI_CELL_CROSSES;
  }

  int score = evaluate_position(board, BOARD_SIZE, AI_CELL_CROSSES);
  EXPECT_EQ(score, 1000000); // Should return win score

  // Opponent should get lose score
  int opponent_score = evaluate_position(board, BOARD_SIZE, AI_CELL_NAUGHTS);
  EXPECT_EQ(opponent_score, -1000000);
}

// Test AI move evaluation
TEST_F(GomokuTest, AIMoveEvaluation) {
  // Test move interesting function
  EXPECT_TRUE(is_move_interesting(
      board, 9, 9, 0, BOARD_SIZE)); // Center is interesting when empty

  // Place a stone and test nearby positions
  board[9][9] = AI_CELL_CROSSES;
  EXPECT_TRUE(is_move_interesting(board, 9, 10, 1,
                                  BOARD_SIZE)); // Adjacent is interesting
  EXPECT_FALSE(is_move_interesting(board, 0, 0, 1,
                                   BOARD_SIZE)); // Far away is not interesting

  // Test winning move detection
  for (int i = 0; i < 4; i++) {
    board[7][i] = AI_CELL_CROSSES;
  }
  EXPECT_TRUE(is_winning_move(board, 7, 4, AI_CELL_CROSSES,
                              BOARD_SIZE)); // Completes 5 in a row
  EXPECT_FALSE(is_winning_move(board, 8, 4, AI_CELL_CROSSES,
                               BOARD_SIZE)); // Doesn't complete
}

// Test game logic functions
TEST_F(GomokuTest, GameLogicFunctions) {
  // Test making a move
  double move_time = 1.5;
  int positions_evaluated = 10;

  EXPECT_TRUE(
      make_move(game, 9, 9, AI_CELL_CROSSES, move_time, positions_evaluated));
  EXPECT_EQ(game->board[9][9], AI_CELL_CROSSES);
  EXPECT_EQ(game->move_history_count, 1);
  EXPECT_EQ(game->current_player, AI_CELL_NAUGHTS); // Should switch players

  // Test invalid move
  EXPECT_FALSE(make_move(game, 9, 9, AI_CELL_NAUGHTS, move_time,
                         positions_evaluated)); // Same position
  EXPECT_EQ(game->move_history_count, 1);       // Should not increase
}

// Test undo functionality
TEST_F(GomokuTest, UndoFunctionality) {
  // Initially cannot undo
  EXPECT_FALSE(can_undo(game));

  // Make two moves
  make_move(game, 9, 9, AI_CELL_CROSSES, 1.0, 0);
  make_move(game, 9, 10, AI_CELL_NAUGHTS, 1.0, 5);

  // Capture timing totals before undo
  double human_time = game->total_human_time;
  double ai_time = game->total_ai_time;

  // Now can undo
  EXPECT_TRUE(can_undo(game));
  EXPECT_EQ(game->move_history_count, 2);
  EXPECT_GT(human_time, 0.0);
  EXPECT_GT(ai_time, 0.0);

  // Undo moves
  undo_last_moves(game);
  EXPECT_EQ(game->move_history_count, 0);
  EXPECT_EQ(game->board[9][9], AI_CELL_EMPTY);
  EXPECT_EQ(game->board[9][10], AI_CELL_EMPTY);
  EXPECT_EQ(game->current_player, AI_CELL_CROSSES);
  EXPECT_DOUBLE_EQ(game->total_human_time, 0.0);
  EXPECT_DOUBLE_EQ(game->total_ai_time, 0.0);
}

// Test other_player function
TEST_F(GomokuTest, OtherPlayerFunction) {
  EXPECT_EQ(other_player(AI_CELL_CROSSES), AI_CELL_NAUGHTS);
  EXPECT_EQ(other_player(AI_CELL_NAUGHTS), AI_CELL_CROSSES);
}

// Test minimax basic functionality
TEST_F(GomokuTest, MinimaxBasic) {
  // Place a few stones
  board[7][7] = AI_CELL_CROSSES;
  board[7][8] = AI_CELL_NAUGHTS;

  // Test minimax with depth 1
  int score =
      minimax(board, BOARD_SIZE, 1, -1000000, 1000000, 1, AI_CELL_NAUGHTS);

  // Should return a reasonable score (not extreme values unless winning)
  EXPECT_GT(score, -1000000);
  EXPECT_LT(score, 1000000);
}

// Test minimax with winning position
TEST_F(GomokuTest, MinimaxWithWin) {
  // Create a winning position for crosses
  for (int i = 0; i < 5; i++) {
    board[7][i] = AI_CELL_CROSSES;
  }

  int score =
      minimax(board, BOARD_SIZE, 1, -1000000, 1000000, 1, AI_CELL_CROSSES);
  EXPECT_EQ(
      score,
      1000001); // Should recognize the win (WIN_SCORE + depth for faster wins)
}

// Ensure minimax works correctly on boards smaller than 19x19
TEST_F(GomokuTest, MinimaxDifferentBoardSize) {
  const int SMALL_SIZE = 15;
  int **small_board = create_board(SMALL_SIZE);
  ASSERT_NE(small_board, nullptr);

  small_board[7][7] = AI_CELL_CROSSES;
  small_board[7][8] = AI_CELL_NAUGHTS;

  int score = minimax(small_board, SMALL_SIZE, 1, -1000000, 1000000, 1,
                      AI_CELL_NAUGHTS);
  EXPECT_GT(score, -1000000);
  EXPECT_LT(score, 1000000);

  free_board(small_board, SMALL_SIZE);
}

// Test corner cases
TEST_F(GomokuTest, CornerCases) {
  // Test edge positions - evaluate potential move at edge
  int edge_score = calc_score_at(board, BOARD_SIZE, AI_CELL_CROSSES, 0, 0);
  EXPECT_GE(edge_score, 0); // Should handle edge positions

  // Test center position - evaluate potential move at center
  int center_score = calc_score_at(board, BOARD_SIZE, AI_CELL_CROSSES, 9, 9);
  EXPECT_GE(center_score, 0); // Center should be non-negative
}

// Test multiple directions
TEST_F(GomokuTest, MultiDirectionThreats) {
  // Create threats in multiple directions
  board[7][7] = AI_CELL_CROSSES; // Center
  board[7][6] = AI_CELL_CROSSES; // Left
  board[7][8] = AI_CELL_CROSSES; // Right
  board[6][7] = AI_CELL_CROSSES; // Up
  board[8][7] = AI_CELL_CROSSES; // Down

  int score = calc_score_at(board, BOARD_SIZE, AI_CELL_CROSSES, 7, 7);
  EXPECT_GT(score, 100); // Should recognize multiple threats
}

// Test blocked patterns
TEST_F(GomokuTest, BlockedPatterns) {
  // Create a pattern that's blocked by opponent
  board[7][4] = AI_CELL_CROSSES;
  board[7][5] = AI_CELL_CROSSES;
  board[7][6] = AI_CELL_CROSSES;
  board[7][3] = AI_CELL_NAUGHTS; // Block one side
  board[7][7] = AI_CELL_NAUGHTS; // Block other side

  int score = calc_score_at(board, BOARD_SIZE, AI_CELL_CROSSES, 7, 5);

  // Should be lower than unblocked pattern
  // Clear the blocks and test
  board[7][3] = AI_CELL_EMPTY;
  board[7][7] = AI_CELL_EMPTY;

  int unblocked_score = calc_score_at(board, BOARD_SIZE, AI_CELL_CROSSES, 7, 5);
  EXPECT_GT(unblocked_score, score);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
