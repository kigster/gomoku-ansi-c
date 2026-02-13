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
        .board_size = BOARD_SIZE,           // 19x19 board
        .max_depth = 4,                     // AI search depth
        .move_timeout = 0,                  // No timeout
        .show_help = 0,                     // Don't show help
        .invalid_args = 0,                  // Valid arguments
        .enable_undo = 1,                   // Enable undo for testing
        .skip_welcome = 1,                  // Skip welcome screen
        .search_radius = 2,                 // Default search radius
        .json_file = "",                    // No JSON output
        .replay_file = "",                  // No replay
        .replay_wait = 0.0,                 // No replay wait
        .player_x_type = PLAYER_TYPE_HUMAN, // Default: human X
        .player_o_type = PLAYER_TYPE_AI,    // Default: AI O
        .depth_x = -1,                      // Use max_depth
        .depth_o = -1,                      // Use max_depth
        .player_x_explicit = 0,             // Not explicitly set
        .player_o_explicit = 0              // Not explicitly set
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
  // Test move interesting function (radius=2 is default)
  EXPECT_TRUE(is_move_interesting(board, 9, 9, 0, BOARD_SIZE,
                                  2)); // Center is interesting when empty

  // Place a stone and test nearby positions
  board[9][9] = AI_CELL_CROSSES;
  EXPECT_TRUE(is_move_interesting(board, 9, 10, 1, BOARD_SIZE,
                                  2)); // Adjacent is interesting
  EXPECT_FALSE(is_move_interesting(board, 0, 0, 1, BOARD_SIZE,
                                   2)); // Far away is not interesting

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

  EXPECT_TRUE(make_move(game, 9, 9, AI_CELL_CROSSES, move_time,
                        positions_evaluated, 0, 0));
  EXPECT_EQ(game->board[9][9], AI_CELL_CROSSES);
  EXPECT_EQ(game->move_history_count, 1);
  EXPECT_EQ(game->current_player, AI_CELL_NAUGHTS); // Should switch players

  // Test invalid move
  EXPECT_FALSE(make_move(game, 9, 9, AI_CELL_NAUGHTS, move_time,
                         positions_evaluated, 0, 0)); // Same position
  EXPECT_EQ(game->move_history_count, 1);             // Should not increase
}

// Test undo functionality
TEST_F(GomokuTest, UndoFunctionality) {
  // Initially cannot undo
  EXPECT_FALSE(can_undo(game));

  // Make two moves
  make_move(game, 9, 9, AI_CELL_CROSSES, 1.0, 0, 0, 0);
  make_move(game, 9, 10, AI_CELL_NAUGHTS, 1.0, 5, 0, 0);

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

//===============================================================================
// SELF-PLAY AI QUALITY TEST
//===============================================================================

// Structure to track per-player stats during a game
struct PlayerStats {
  int total_moves;
  int threats_created; // Sum of threat scores for moves made
  int blocks_made;     // Number of times player blocked opponent's winning move
  int missed_blocks; // Number of times player failed to block a winning threat
  int win_move_number; // Move number when this player won (0 if didn't win)
};

// Structure to store game result
struct GameResult {
  int game_length; // Total moves in game
  int winner; // AI_CELL_CROSSES, AI_CELL_NAUGHTS, or AI_CELL_EMPTY for draw
  PlayerStats crosses_stats;
  PlayerStats naughts_stats;
};

// Helper: Find best move for any player using minimax
// This is needed because find_best_ai_move() is hardcoded for NAUGHTS
static void find_best_move_for_player(game_state_t *game, int player,
                                      int *best_x, int *best_y) {
  *best_x = -1;
  *best_y = -1;

  int best_score = -WIN_SCORE - 1;

  // Generate moves near existing stones
  for (int i = 0; i < game->board_size; i++) {
    for (int j = 0; j < game->board_size; j++) {
      if (game->board[i][j] != AI_CELL_EMPTY)
        continue;
      if (!is_move_interesting(game->board, i, j, game->stones_on_board,
                               game->board_size, game->search_radius))
        continue;

      // Check for immediate win
      if (is_winning_move(game->board, i, j, player, game->board_size)) {
        *best_x = i;
        *best_y = j;
        return;
      }

      // Check for blocking opponent's win
      if (is_winning_move(game->board, i, j, other_player(player),
                          game->board_size)) {
        *best_x = i;
        *best_y = j;
        // Continue searching in case we have our own winning move
      }
    }
  }

  // If we found a blocking move but no winning move, use it
  if (*best_x >= 0) {
    return;
  }

  // Use minimax to find best move
  for (int i = 0; i < game->board_size; i++) {
    for (int j = 0; j < game->board_size; j++) {
      if (game->board[i][j] != AI_CELL_EMPTY)
        continue;
      if (!is_move_interesting(game->board, i, j, game->stones_on_board,
                               game->board_size, game->search_radius))
        continue;

      // Try the move
      game->board[i][j] = player;
      game->stones_on_board++;

      // Evaluate with minimax (depth-1 since we made a move)
      int score = minimax(game->board, game->board_size, game->max_depth - 1,
                          -WIN_SCORE - 1, WIN_SCORE + 1, 0, player);

      // Undo the move
      game->board[i][j] = AI_CELL_EMPTY;
      game->stones_on_board--;

      if (score > best_score) {
        best_score = score;
        *best_x = i;
        *best_y = j;
      }
    }
  }

  // Fallback: if no interesting move found, pick center or any empty cell
  if (*best_x < 0) {
    int center = game->board_size / 2;
    if (game->board[center][center] == AI_CELL_EMPTY) {
      *best_x = center;
      *best_y = center;
    } else {
      for (int i = 0; i < game->board_size && *best_x < 0; i++) {
        for (int j = 0; j < game->board_size && *best_x < 0; j++) {
          if (game->board[i][j] == AI_CELL_EMPTY) {
            *best_x = i;
            *best_y = j;
          }
        }
      }
    }
  }
}

// Helper: Count how many winning moves opponent has
static int count_opponent_winning_moves(int **board, int board_size,
                                        int opponent) {
  int count = 0;
  for (int i = 0; i < board_size; i++) {
    for (int j = 0; j < board_size; j++) {
      if (board[i][j] == AI_CELL_EMPTY &&
          is_winning_move(board, i, j, opponent, board_size)) {
        count++;
      }
    }
  }
  return count;
}

// Run a single self-play game and return the result
static GameResult run_self_play_game(int board_size, int max_depth) {
  GameResult result = {};
  result.winner = AI_CELL_EMPTY; // Draw by default

  cli_config_t config = {.board_size = board_size,
                         .max_depth = max_depth,
                         .move_timeout = 0,
                         .show_help = 0,
                         .invalid_args = 0,
                         .enable_undo = 0,
                         .skip_welcome = 1,
                         .search_radius = 2,
                         .json_file = "",
                         .replay_file = "",
                         .replay_wait = 0.0,
                         .player_x_type = PLAYER_TYPE_HUMAN,
                         .player_o_type = PLAYER_TYPE_AI,
                         .depth_x = -1,
                         .depth_o = -1,
                         .player_x_explicit = 0,
                         .player_o_explicit = 0};

  game_state_t *game = init_game(config);
  if (!game)
    return result;

  int current_player = AI_CELL_CROSSES;
  int max_moves = board_size * board_size;

  for (int move_num = 1; move_num <= max_moves; move_num++) {
    PlayerStats *current_stats = (current_player == AI_CELL_CROSSES)
                                     ? &result.crosses_stats
                                     : &result.naughts_stats;
    int opponent = other_player(current_player);

    // Count opponent's winning moves before our move (for defense tracking)
    int opp_winning_before =
        count_opponent_winning_moves(game->board, board_size, opponent);

    // Find best move
    int best_x, best_y;
    find_best_move_for_player(game, current_player, &best_x, &best_y);

    if (best_x < 0 || best_y < 0) {
      // No valid move - this shouldn't happen
      break;
    }

    // Track if we blocked an opponent's winning move
    if (opp_winning_before > 0 &&
        is_winning_move(game->board, best_x, best_y, opponent, board_size)) {
      current_stats->blocks_made++;
    } else if (opp_winning_before > 0) {
      // We had winning moves to block but didn't block any
      current_stats->missed_blocks++;
    }

    // Calculate threat score for our move (offense metric)
    int threat = evaluate_threat_fast(game->board, best_x, best_y,
                                      current_player, board_size);
    current_stats->threats_created += threat;

    // Make the move
    game->board[best_x][best_y] = current_player;
    game->stones_on_board++;
    current_stats->total_moves++;
    result.game_length++;

    // Check for win
    if (has_winner(game->board, board_size, current_player)) {
      result.winner = current_player;
      current_stats->win_move_number = move_num;
      break;
    }

    // Switch players
    current_player = opponent;
  }

  cleanup_game(game);
  return result;
}

// Test: AI self-play quality assessment
TEST_F(GomokuTest, SelfPlayQuality) {
  const int NUM_GAMES = 3;        // Run 3 games for reasonable CI time
  const int TEST_BOARD_SIZE = 15; // Use smaller board for faster tests
  const int TEST_DEPTH = 2;       // Lower depth for faster tests

  int total_game_length = 0;
  int decisive_games = 0; // Games that ended with a winner
  int total_blocks = 0;
  int total_missed = 0;
  int total_threats = 0;
  int total_moves = 0;

  std::cout << "\n=== AI Self-Play Quality Test ===" << std::endl;
  std::cout << "Running " << NUM_GAMES << " games at depth " << TEST_DEPTH
            << " on " << TEST_BOARD_SIZE << "x" << TEST_BOARD_SIZE << " board"
            << std::endl;

  for (int g = 0; g < NUM_GAMES; g++) {
    GameResult result = run_self_play_game(TEST_BOARD_SIZE, TEST_DEPTH);

    total_game_length += result.game_length;
    if (result.winner != AI_CELL_EMPTY) {
      decisive_games++;
    }

    // Aggregate stats from both players
    total_blocks +=
        result.crosses_stats.blocks_made + result.naughts_stats.blocks_made;
    total_missed +=
        result.crosses_stats.missed_blocks + result.naughts_stats.missed_blocks;
    total_threats += result.crosses_stats.threats_created +
                     result.naughts_stats.threats_created;
    total_moves +=
        result.crosses_stats.total_moves + result.naughts_stats.total_moves;

    const char *winner_str = (result.winner == AI_CELL_CROSSES)   ? "X"
                             : (result.winner == AI_CELL_NAUGHTS) ? "O"
                                                                  : "Draw";
    std::cout << "Game " << (g + 1) << ": " << result.game_length
              << " moves, Winner: " << winner_str << std::endl;
  }

  // Calculate metrics
  double avg_game_length = (double)total_game_length / NUM_GAMES;
  double avg_threat_per_move =
      (total_moves > 0) ? (double)total_threats / total_moves : 0;
  double defense_rate =
      (total_blocks + total_missed > 0)
          ? (double)total_blocks / (total_blocks + total_missed) * 100.0
          : 100.0;

  std::cout << "\n=== Quality Metrics ===" << std::endl;
  std::cout << "Average game length: " << avg_game_length << " moves"
            << std::endl;
  std::cout << "Decisive games: " << decisive_games << "/" << NUM_GAMES
            << std::endl;
  std::cout << "Defense rate: " << defense_rate << "% (" << total_blocks
            << " blocks, " << total_missed << " missed)" << std::endl;
  std::cout << "Avg threat per move: " << avg_threat_per_move << std::endl;

  // Quality assertions
  //
  // Key insight: At low search depth (2), a well-balanced AI will often draw
  // because neither side can see deep enough to create winning forcing
  // sequences. Draws indicate GOOD defensive play, not poor play.

  // Longer games indicate better defense (AI blocks threats effectively)
  // At depth 2, games filling the entire board (225 moves on 15x15) is expected
  EXPECT_GE(avg_game_length, 9.0)
      << "Games too short - AI may not be playing well";

  // Defense rate should be high - the AI should block most winning threats
  EXPECT_GE(defense_rate, 50.0)
      << "Defense rate too low - AI not blocking threats";

  // Offensive output should show the AI creates meaningful threats
  // A score > 100 per move indicates the AI is building patterns, not just
  // random play
  EXPECT_GT(avg_threat_per_move, 100.0)
      << "Avg threat too low - AI may be too passive";

  // If we have decisive games, the winner should have played reasonably
  // (but draws are perfectly acceptable at low depth - they indicate good
  // defense)
}

// Test: AI vs AI mode with new player configuration system
TEST_F(GomokuTest, AIvsAI_CompletesSuccessfully) {
  std::cout << "\n=== AI vs AI Mode Test ===" << std::endl;
  std::cout << "Testing new player configuration system" << std::endl;

  // Create configuration for AI vs AI mode
  cli_config_t config = {.board_size = 15,
                         .max_depth = 2,
                         .move_timeout = 0,
                         .show_help = 0,
                         .invalid_args = 0,
                         .enable_undo = 0,
                         .skip_welcome = 1,
                         .search_radius = 2,
                         .json_file = "",
                         .replay_file = "",
                         .replay_wait = 0.0,
                         .player_x_type = PLAYER_TYPE_AI,
                         .player_o_type = PLAYER_TYPE_AI,
                         .depth_x = 2,
                         .depth_o = 2,
                         .player_x_explicit = 0,
                         .player_o_explicit = 0};

  game_state_t *ai_game = init_game(config);
  ASSERT_NE(ai_game, nullptr);
  EXPECT_EQ(ai_game->player_type[0], PLAYER_TYPE_AI) << "Player X should be AI";
  EXPECT_EQ(ai_game->player_type[1], PLAYER_TYPE_AI) << "Player O should be AI";
  EXPECT_EQ(ai_game->depth_for_player[0], 2) << "Player X depth should be 2";
  EXPECT_EQ(ai_game->depth_for_player[1], 2) << "Player O depth should be 2";

  populate_threat_matrix();

  // Play game until completion (max 225 moves for 15x15 board)
  int moves = 0;
  int max_moves = config.board_size * config.board_size;

  while (ai_game->game_state == GAME_RUNNING && moves < max_moves) {
    int x, y;

    // Temporarily set max_depth to current player's depth
    int current_index = (ai_game->current_player == AI_CELL_CROSSES) ? 0 : 1;
    ai_game->max_depth = ai_game->depth_for_player[current_index];

    find_best_ai_move(ai_game, &x, &y, NULL);

    ASSERT_GE(x, 0) << "AI should find a valid move";
    ASSERT_GE(y, 0) << "AI should find a valid move";
    ASSERT_LT(x, config.board_size) << "Move should be within board";
    ASSERT_LT(y, config.board_size) << "Move should be within board";

    make_move(ai_game, x, y, ai_game->current_player, 0.0, 1, 0, 0);
    moves++;
  }

  std::cout << "Game completed in " << moves << " moves" << std::endl;

  // Verify game ended properly (not stuck in RUNNING state)
  EXPECT_TRUE(ai_game->game_state == GAME_HUMAN_WIN ||
              ai_game->game_state == GAME_AI_WIN ||
              ai_game->game_state == GAME_DRAW)
      << "Game should end with win or draw, not hang";

  // Verify move count is reasonable
  EXPECT_GT(moves, 0) << "Game should have at least one move";
  EXPECT_LE(moves, max_moves) << "Game should not exceed board capacity";

  const char *result_str = (ai_game->game_state == GAME_HUMAN_WIN) ? "X wins"
                           : (ai_game->game_state == GAME_AI_WIN)  ? "O wins"
                                                                   : "Draw";
  std::cout << "Game result: " << result_str << std::endl;

  cleanup_game(ai_game);
}

// Test: AI vs AI mode with asymmetric depths
TEST_F(GomokuTest, AIvsAI_AsymmetricDepths) {
  std::cout << "\n=== AI vs AI Asymmetric Depths Test ===" << std::endl;

  // Create configuration with different depths for X and O
  cli_config_t config = {.board_size = 15,
                         .max_depth = 4, // Max of both
                         .move_timeout = 0,
                         .show_help = 0,
                         .invalid_args = 0,
                         .enable_undo = 0,
                         .skip_welcome = 1,
                         .search_radius = 2,
                         .json_file = "",
                         .replay_file = "",
                         .replay_wait = 0.0,
                         .player_x_type = PLAYER_TYPE_AI,
                         .player_o_type = PLAYER_TYPE_AI,
                         .depth_x = 2,
                         .depth_o = 4,
                         .player_x_explicit = 0,
                         .player_o_explicit = 0};

  game_state_t *ai_game = init_game(config);
  ASSERT_NE(ai_game, nullptr);
  EXPECT_EQ(ai_game->depth_for_player[0], 2) << "Player X depth should be 2";
  EXPECT_EQ(ai_game->depth_for_player[1], 4) << "Player O depth should be 4";

  std::cout << "X plays at depth " << ai_game->depth_for_player[0] << std::endl;
  std::cout << "O plays at depth " << ai_game->depth_for_player[1] << std::endl;

  populate_threat_matrix();

  // Play a few moves to verify depths are honored
  for (int i = 0; i < 6 && ai_game->game_state == GAME_RUNNING; i++) {
    int x, y;
    int current_index = (ai_game->current_player == AI_CELL_CROSSES) ? 0 : 1;
    int expected_depth = ai_game->depth_for_player[current_index];

    ai_game->max_depth = expected_depth;
    find_best_ai_move(ai_game, &x, &y, NULL);

    ASSERT_GE(x, 0) << "AI should find valid move at move " << (i + 1);
    ASSERT_GE(y, 0) << "AI should find valid move at move " << (i + 1);

    make_move(ai_game, x, y, ai_game->current_player, 0.0, 1, 0, 0);

    char player_symbol =
        (ai_game->current_player == AI_CELL_CROSSES) ? 'O' : 'X'; // Just moved
    std::cout << "Move " << (i + 1) << ": " << player_symbol << " moved to ["
              << x << "," << y << "] at depth " << expected_depth << std::endl;
  }

  cleanup_game(ai_game);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
