//
//  ai.c
//  gomoku - AI module for minimax search and move finding
//
//  Handles AI move finding, minimax algorithm, and move prioritization
//

#include "ai.h"
#include "ansi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//===============================================================================
// AI CONSTANTS AND STRUCTURES
//===============================================================================

#define MAX_RADIUS 2
#define WIN_SCORE 1000000

//===============================================================================
// OPTIMIZED MOVE GENERATION
//===============================================================================

/**
 * Move generation that scans the board for candidates near occupied cells.
 *
 * NOTE: We cannot use the cached interesting_moves during minimax search
 * because the cache only reflects the root position. During search, temporary
 * moves are placed/removed on the board, and we need to find candidates near
 * ALL occupied cells (including temporary ones) to properly evaluate defensive
 * moves.
 */
int generate_moves_optimized(game_state_t *game, int **board, move_t *moves,
                             int current_player, int depth_remaining) {
  int size = game->board_size;
  int move_count = 0;

  // Quick check: count stones on board to detect empty board
  int stones = 0;
  for (int i = 0; i < size && stones == 0; i++) {
    for (int j = 0; j < size && stones == 0; j++) {
      if (board[i][j] != AI_CELL_EMPTY) {
        stones = 1; // Found at least one stone
      }
    }
  }

  if (stones == 0) {
    // Board is empty, play center
    moves[0].x = size / 2;
    moves[0].y = size / 2;
    moves[0].priority = 1000;
    return 1;
  }

  // Scan the board for candidates near all occupied cells
  // This must scan the actual board (not cache) to find moves near temporary
  // stones
  unsigned char candidate[19][19];
  memset(candidate, 0, sizeof(candidate));

  int radius = game->search_radius;
  for (int x = 0; x < size; x++) {
    for (int y = 0; y < size; y++) {
      if (board[x][y] == AI_CELL_EMPTY) {
        continue;
      }
      // Mark all empty cells within radius as candidates
      for (int dx = -radius; dx <= radius; dx++) {
        for (int dy = -radius; dy <= radius; dy++) {
          int nx = x + dx;
          int ny = y + dy;
          if (nx < 0 || nx >= size || ny < 0 || ny >= size) {
            continue;
          }
          if (board[nx][ny] != AI_CELL_EMPTY) {
            continue;
          }
          candidate[nx][ny] = 1;
        }
      }
    }
  }

  // Collect candidates and compute priorities
  for (int x = 0; x < size; x++) {
    for (int y = 0; y < size; y++) {
      if (!candidate[x][y]) {
        continue;
      }
      moves[move_count].x = x;
      moves[move_count].y = y;
      moves[move_count].priority = get_move_priority_optimized(
          game, board, x, y, current_player, depth_remaining);
      move_count++;
    }
  }

  return move_count;
}

/**
 * Optimized move prioritization without temporary placements.
 * This is used only for move ordering (not pruning).
 */
int get_move_priority_optimized(game_state_t *game, int **board, int x, int y,
                                int player, int depth_remaining) {
  int center = game->board_size / 2;
  int priority = 0;

  // Center bias - closer to center is better early on.
  int center_dist = abs(x - center) + abs(y - center);
  priority += max(0, game->board_size - center_dist);

  int my_threat = evaluate_threat_fast(board, x, y, player, game->board_size);
  int opp_threat =
      evaluate_threat_fast(board, x, y, other_player(player), game->board_size);

  // Winning move should come first.
  if (my_threat >= 100000) {
    return 2000000000;
  }
  // Blocking opponent's winning move is second priority.
  if (opp_threat >= 100000) {
    return 1500000000;
  }
  // Creating a compound threat (four+three, double-three) is very high
  // priority.
  if (my_threat >= 40000) {
    return 1200000000 + my_threat;
  }
  // Blocking opponent's compound threat is also high priority.
  if (opp_threat >= 40000) {
    return 1100000000 + opp_threat;
  }

  // Killer move bonus (depth-local).
  if (is_killer_move(game, depth_remaining, x, y)) {
    priority += 1000000;
  }

  // Weight our threats and opponent's threats for move ordering
  // Favor offense over defense - only block when opponent has real threats
  // Opponent's closed threats (< 1500) don't require urgent blocking
  if (opp_threat >= 1500) {
    // Opponent has open three or higher - blocking is important
    priority += my_threat * 10;
    priority += opp_threat * 12;
  } else {
    // Opponent has no urgent threats - prioritize offense
    priority += my_threat * 15;
    priority += opp_threat * 5;
  }

  return priority;
}

/**
 * Analyzes one direction and returns threat info for combining.
 * Counts stones, holes, and checks if ends are open.
 */
typedef struct {
  int contiguous; // Contiguous stones from the placed stone
  int total;      // Total stones including those after a hole
  int open_end;   // Is the far end open (0 or 1)
  int holes;      // Number of holes (gaps) in the pattern
} direction_info_t;

static direction_info_t analyze_direction(int **board, int x, int y, int dx,
                                          int dy, int player, int board_size) {
  direction_info_t info = {0, 0, 0, 0};
  int nx = x + dx, ny = y + dy;
  int found_hole = 0;

  while (nx >= 0 && nx < board_size && ny >= 0 && ny < board_size) {
    if (board[nx][ny] == player) {
      if (!found_hole) {
        info.contiguous++;
      }
      info.total++;
    } else if (board[nx][ny] == AI_CELL_EMPTY) {
      if (found_hole) {
        // Second gap - end of pattern, but the end is open
        info.open_end = 1;
        break;
      }
      // First gap - check if there are more stones after
      found_hole = 1;
      info.holes++;
      // Look one more step to see if there's a stone after the gap
      int nnx = nx + dx, nny = ny + dy;
      if (nnx >= 0 && nnx < board_size && nny >= 0 && nny < board_size &&
          board[nnx][nny] == player) {
        // Continue scanning
      } else {
        // No stone after gap - end is open
        info.open_end = 1;
        break;
      }
    } else {
      // Hit opponent or out of bounds - closed end
      info.open_end = 0;
      break;
    }
    nx += dx;
    ny += dy;
  }

  // If we exited the loop without breaking, check if we're still in bounds
  if (nx < 0 || nx >= board_size || ny < 0 || ny >= board_size) {
    info.open_end = 0; // Board edge is a closed end
  }

  return info;
}

int evaluate_threat_fast(int **board, int x, int y, int player,
                         int board_size) {
  // Check all 4 directions
  int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

  int dir_threats[4] = {0, 0, 0, 0};       // Threat in each direction
  int dir_is_four[4] = {0, 0, 0, 0};       // Is this direction a four?
  int dir_is_open_three[4] = {0, 0, 0, 0}; // Is this direction an open three?
  int dir_is_three[4] = {0, 0, 0, 0};    // Is this direction any kind of three?
  int dir_is_open_two[4] = {0, 0, 0, 0}; // Is this direction an open two?

  for (int d = 0; d < 4; d++) {
    int dx = directions[d][0];
    int dy = directions[d][1];

    // Analyze both directions from the placed stone
    direction_info_t pos =
        analyze_direction(board, x, y, dx, dy, player, board_size);
    direction_info_t neg =
        analyze_direction(board, x, y, -dx, -dy, player, board_size);

    int contiguous = 1 + pos.contiguous + neg.contiguous; // +1 for placed stone
    int total = 1 + pos.total + neg.total;
    int holes = pos.holes + neg.holes;
    int open_ends = pos.open_end + neg.open_end; // 0, 1, or 2 open ends

    // Evaluate threat level with consideration for holes and openness
    int threat = 0;

    // Only contiguous >= 5 is a true win. total >= 5 with holes is NOT a win.
    if (contiguous >= 5) {
      threat = 100000; // Win
    } else if (contiguous == 4) {
      if (open_ends >= 2) {
        threat = 50000; // Open four - guaranteed win
      } else if (open_ends == 1) {
        threat = 10000; // Closed four - must block
      }
      dir_is_four[d] = 1;
    } else if (total >= 4 && holes <= 1) {
      // Four with a hole (like XX_XX or X_XXX)
      threat = 8000;
      dir_is_four[d] = 1;
    } else if (contiguous == 3) {
      if (open_ends >= 2) {
        threat = 1500; // Open three - serious threat
        dir_is_open_three[d] = 1;
      } else if (open_ends == 1) {
        threat = 500; // Closed three
      }
      dir_is_three[d] = 1;
    } else if (total >= 3 && holes <= 1) {
      // Three with a hole (like X_XX or XX_X)
      if (open_ends >= 1) {
        threat = 400; // Broken three with open end
        dir_is_three[d] = 1;
      }
    } else if (contiguous == 2 && open_ends >= 2) {
      threat = 100; // Open two
      dir_is_open_two[d] = 1;
    }

    dir_threats[d] = threat;
  }

  // Calculate maximum single threat
  int max_threat = 0;
  for (int d = 0; d < 4; d++) {
    if (dir_threats[d] > max_threat) {
      max_threat = dir_threats[d];
    }
  }

  // Check for compound threats (combinations across directions)
  int num_fours = 0;
  int num_open_threes = 0;
  int num_threes = 0;
  int num_open_twos = 0;

  for (int d = 0; d < 4; d++) {
    if (dir_is_four[d])
      num_fours++;
    if (dir_is_open_three[d])
      num_open_threes++;
    if (dir_is_three[d])
      num_threes++;
    if (dir_is_open_two[d])
      num_open_twos++;
  }

  // Compound threat bonuses - these are nearly winning!
  // Four + Three = opponent can only block one
  if (num_fours >= 1 && num_threes >= 1) {
    max_threat = max(max_threat, 45000); // Nearly winning
  }
  // Two open threes = opponent can only block one
  if (num_open_threes >= 2) {
    max_threat = max(max_threat, 40000); // Winning combination
  }
  // Two fours = opponent can only block one
  if (num_fours >= 2) {
    max_threat = max(max_threat, 48000); // Very winning
  }
  // Open three + another three = dangerous
  if (num_open_threes >= 1 && num_threes >= 2) {
    max_threat = max(max_threat, 30000);
  }
  // Two or more intersecting open twos = creates double open three when played
  // This is a serious developing threat that should be valued highly
  if (num_open_twos >= 2) {
    max_threat = max(max_threat, 2000); // Strong developing position
  }
  // Open two + open three = developing into compound threat
  if (num_open_twos >= 1 && num_open_threes >= 1) {
    max_threat = max(max_threat, 3000);
  }

  return max_threat;
}

//===============================================================================
// MOVE EVALUATION AND ORDERING
//===============================================================================

int is_move_interesting(int **board, int x, int y, int stones_on_board,
                        int board_size, int radius) {
  // If there are no stones on board, only center area is interesting
  if (stones_on_board == 0) {
    int center = board_size / 2;
    return (abs(x - center) <= 2 && abs(y - center) <= 2);
  }

  // Check if within radius cells of any existing stone
  for (int i = max(0, x - radius); i <= min(board_size - 1, x + radius); i++) {
    for (int j = max(0, y - radius); j <= min(board_size - 1, y + radius);
         j++) {
      if (board[i][j] != AI_CELL_EMPTY) {
        return 1; // Found a stone within radius
      }
    }
  }

  return 0; // No stones nearby, not interesting
}

int is_winning_move(int **board, int x, int y, int player, int board_size) {
  board[x][y] = player;
  int is_win = has_winner(board, board_size, player);
  board[x][y] = AI_CELL_EMPTY;
  return is_win;
}

int get_move_priority(int **board, int x, int y, int player, int board_size) {
  int center = board_size / 2;
  int priority = 0;

  // Immediate win gets highest priority
  if (is_winning_move(board, x, y, player, board_size)) {
    return 100000;
  }

  // Blocking opponent's win gets second highest priority
  if (is_winning_move(board, x, y, other_player(player), board_size)) {
    return 50000;
  }

  // Center bias - closer to center is better
  int center_dist = abs(x - center) + abs(y - center);
  priority += max(0, board_size - center_dist);

  // Check for immediate threats/opportunities
  board[x][y] = player; // Temporarily place the move
  int my_score = calc_score_at(board, board_size, player, x, y);
  board[x][y] = other_player(player); // Check opponent's response
  int opp_score = calc_score_at(board, board_size, other_player(player), x, y);
  board[x][y] = AI_CELL_EMPTY; // Restore empty

  // Prioritize offensive and defensive moves
  priority += my_score / 10; // Our opportunities
  priority += opp_score / 5; // Blocking opponent

  return priority;
}

int compare_moves(const void *a, const void *b) {
  move_t *move_a = (move_t *)a;
  move_t *move_b = (move_t *)b;
  return move_b->priority - move_a->priority; // Higher priority first
}

//===============================================================================
// VCT (VICTORY BY CONTINUOUS THREATS) SEARCH
//===============================================================================

int find_block_cell(int **board, int x, int y, int player, int board_size,
                    int *block_x, int *block_y) {
  // After placing a stone at (x,y) that creates a four, find the ONE cell
  // the opponent must play to prevent five-in-a-row.
  // Scan empty cells within distance 5 of the placed stone in all directions.
  int found_count = 0;
  *block_x = -1;
  *block_y = -1;

  int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
  for (int d = 0; d < 4; d++) {
    int dx = directions[d][0];
    int dy = directions[d][1];

    // Scan in both directions along this line
    for (int sign = -1; sign <= 1; sign += 2) {
      for (int dist = 1; dist <= 5; dist++) {
        int nx = x + sign * dx * dist;
        int ny = y + sign * dy * dist;
        if (nx < 0 || nx >= board_size || ny < 0 || ny >= board_size) break;
        if (board[nx][ny] == AI_CELL_EMPTY) {
          // Check if placing player's stone here would win
          if (is_winning_move(board, nx, ny, player, board_size)) {
            if (found_count == 0) {
              *block_x = nx;
              *block_y = ny;
            }
            found_count++;
            if (found_count >= 2) {
              // Open four: two ways to complete = already unstoppable
              return 0;
            }
          }
          break; // Stop after first empty cell in this direction
        } else if (board[nx][ny] != player) {
          break; // Hit opponent stone
        }
      }
    }
  }

  return (found_count == 1) ? 1 : 0;
}

static int find_forced_win_recursive(game_state_t *game, int **board,
                                     int player, int max_depth,
                                     int *result_x, int *result_y,
                                     int sequence[][2], int *seq_len) {
  int board_size = game->board_size;
  int opponent = other_player(player);

  // Check if we already have an unstoppable compound threat
  // Generate candidate moves near existing stones
  move_t moves[361];
  int move_count = generate_moves_optimized(game, board, moves, player,
                                            game->max_depth);

  // Check for immediate compound win (>= 40000)
  for (int i = 0; i < move_count; i++) {
    int threat = evaluate_threat_fast(board, moves[i].x, moves[i].y,
                                      player, board_size);
    if (threat >= 40000) {
      // This move creates an unstoppable compound threat
      *result_x = moves[i].x;
      *result_y = moves[i].y;
      if (sequence && seq_len) {
        sequence[*seq_len][0] = moves[i].x;
        sequence[*seq_len][1] = moves[i].y;
        (*seq_len)++;
      }
      return 1;
    }
  }

  if (max_depth <= 0) return 0;

  // Try each move that creates a four (threat >= 8000)
  for (int i = 0; i < move_count; i++) {
    int threat = evaluate_threat_fast(board, moves[i].x, moves[i].y,
                                      player, board_size);
    if (threat < 8000) continue; // Only consider four-creating moves

    int mx = moves[i].x, my = moves[i].y;

    // Place our stone
    board[mx][my] = player;

    // Check if this creates an immediate unstoppable win
    int post_threat = evaluate_threat_fast(board, mx, my, player, board_size);
    if (post_threat >= 100000) {
      // Direct win
      board[mx][my] = AI_CELL_EMPTY;
      *result_x = mx;
      *result_y = my;
      if (sequence && seq_len) {
        sequence[*seq_len][0] = mx;
        sequence[*seq_len][1] = my;
        (*seq_len)++;
      }
      return 1;
    }

    // Re-evaluate: does this create a compound threat now that stone is placed?
    // Check all empty positions for new compound threats by us
    int creates_compound = 0;
    for (int j = 0; j < move_count; j++) {
      if (board[moves[j].x][moves[j].y] != AI_CELL_EMPTY) continue;
      int new_threat = evaluate_threat_fast(board, moves[j].x, moves[j].y,
                                            player, board_size);
      if (new_threat >= 40000) {
        creates_compound = 1;
        break;
      }
    }
    if (creates_compound) {
      board[mx][my] = AI_CELL_EMPTY;
      *result_x = mx;
      *result_y = my;
      if (sequence && seq_len) {
        sequence[*seq_len][0] = mx;
        sequence[*seq_len][1] = my;
        (*seq_len)++;
      }
      return 1;
    }

    // Find the one cell opponent must block
    int bx, by;
    int found_block = find_block_cell(board, mx, my, player, board_size,
                                      &bx, &by);
    if (!found_block) {
      // Either open four (already winning) or no block found
      board[mx][my] = AI_CELL_EMPTY;
      // If it's an open four, that's a win
      if (evaluate_threat_fast(board, mx, my, player, board_size) >= 50000) {
        *result_x = mx;
        *result_y = my;
        if (sequence && seq_len) {
          sequence[*seq_len][0] = mx;
          sequence[*seq_len][1] = my;
          (*seq_len)++;
        }
        return 1;
      }
      continue;
    }

    // Check: does opponent's block give THEM a four or better?
    int opp_threat_at_block = evaluate_threat_fast(board, bx, by, opponent,
                                                    board_size);
    if (opp_threat_at_block >= 8000) {
      // Opponent's forced block also creates a counter-threat, skip
      board[mx][my] = AI_CELL_EMPTY;
      continue;
    }

    // Place opponent's block
    board[bx][by] = opponent;

    // Record our move in sequence
    int saved_seq_len = 0;
    if (sequence && seq_len) {
      saved_seq_len = *seq_len;
      sequence[*seq_len][0] = mx;
      sequence[*seq_len][1] = my;
      (*seq_len)++;
    }

    // Recurse
    int rx, ry;
    int found = find_forced_win_recursive(game, board, player, max_depth - 1,
                                          &rx, &ry, sequence, seq_len);

    // Undo opponent's block and our stone
    board[bx][by] = AI_CELL_EMPTY;
    board[mx][my] = AI_CELL_EMPTY;

    if (found) {
      *result_x = mx;
      *result_y = my;
      return 1;
    }

    // Undo sequence recording on failure
    if (sequence && seq_len) {
      *seq_len = saved_seq_len;
    }
  }

  return 0;
}

int find_forced_win(game_state_t *game, int **board, int player, int max_depth,
                    int *result_x, int *result_y,
                    int sequence[][2], int *seq_len) {
  int local_seq_len = 0;
  if (!seq_len) seq_len = &local_seq_len;
  *seq_len = 0;
  *result_x = -1;
  *result_y = -1;
  return find_forced_win_recursive(game, board, player, max_depth,
                                   result_x, result_y, sequence, seq_len);
}

int find_forced_win_block(game_state_t *game, int **board, int ai_player,
                          int max_depth, int *result_x, int *result_y) {
  int opponent = other_player(ai_player);
  int board_size = game->board_size;

  // Check if opponent has a forced win
  int opp_x, opp_y;
  if (!find_forced_win(game, board, opponent, max_depth,
                       &opp_x, &opp_y, NULL, NULL)) {
    // Opponent has no forced win
    *result_x = -1;
    *result_y = -1;
    return 0;
  }

  // Opponent has a forced win. Find our move that disrupts it.
  move_t moves[361];
  int move_count = generate_moves_optimized(game, board, moves, ai_player,
                                            game->max_depth);

  int best_x = -1, best_y = -1;
  int best_own_threat = -1;

  for (int i = 0; i < move_count; i++) {
    int mx = moves[i].x, my = moves[i].y;

    // Place our stone
    board[mx][my] = ai_player;

    // Check if opponent still has a forced win
    int dummy_x, dummy_y;
    int opp_still_wins = find_forced_win(game, board, opponent, max_depth,
                                         &dummy_x, &dummy_y, NULL, NULL);

    board[mx][my] = AI_CELL_EMPTY;

    if (!opp_still_wins) {
      // This move disrupts the opponent's VCT
      int own_threat = evaluate_threat_fast(board, mx, my, ai_player,
                                            board_size);
      if (own_threat > best_own_threat) {
        best_own_threat = own_threat;
        best_x = mx;
        best_y = my;
      }
    }
  }

  if (best_x >= 0) {
    *result_x = best_x;
    *result_y = best_y;
    return 1;
  }

  // Couldn't find a disrupting move — fall back to blocking the first VCT move
  *result_x = opp_x;
  *result_y = opp_y;
  return 1;
}

//===============================================================================
// MINIMAX ALGORITHM
//===============================================================================

int minimax(int **board, int board_size, int depth, int alpha, int beta,
            int maximizing_player, int ai_player) {
  // Create a temporary game state to use the timeout version.
  // Previously this function assumed a fixed 19x19 board which caused
  // out-of-bounds access for smaller boards.  We now pass the board size
  // explicitly to ensure correct bounds are used during search.
  game_state_t temp_game = {.board = board,
                            .board_size = board_size,
                            .move_timeout = 0,
                            .search_timed_out = 0};

  // Use the center position of the provided board as the initial last move.
  int center = board_size / 2;
  return minimax_with_timeout(&temp_game, board, depth, alpha, beta,
                              maximizing_player, ai_player, center, center);
}

int minimax_with_timeout(game_state_t *game, int **board, int depth, int alpha,
                         int beta, int maximizing_player, int ai_player,
                         int last_x, int last_y) {
  (void)last_x;
  (void)last_y;

  // Check for timeout first
  if (is_search_timed_out(game)) {
    game->search_timed_out = 1;
    return evaluate_position(board, game->board_size, ai_player);
  }

  // Compute position hash
  uint64_t hash = game->current_hash;

  // Probe transposition table
  int tt_value;
  if (probe_transposition(game, hash, depth, alpha, beta, &tt_value)) {
    return tt_value;
  }

  // Check for immediate wins/losses first (terminal conditions)
  if (get_cached_winner(game, ai_player)) {
    int value = WIN_SCORE + depth; // Prefer faster wins
    store_transposition(game, hash, value, depth, TT_EXACT, -1, -1);
    return value;
  }
  if (get_cached_winner(game, other_player(ai_player))) {
    int value = -WIN_SCORE - depth; // Prefer slower losses
    store_transposition(game, hash, value, depth, TT_EXACT, -1, -1);
    return value;
  }

  // Check search depth limit
  if (depth == 0) {
    int value = evaluate_position(board, game->board_size, ai_player);
    store_transposition(game, hash, value, depth, TT_EXACT, -1, -1);
    return value;
  }

  // Use cached stone count
  if (game->stones_on_board == 0) {
    return 0; // Draw
  }

  int current_player_turn =
      maximizing_player ? ai_player : other_player(ai_player);

  // Generate and sort moves using optimized method
  move_t moves[361]; // Max for 19x19 board
  int move_count =
      generate_moves_optimized(game, board, moves, current_player_turn, depth);

  if (move_count == 0) {
    return 0; // No moves available
  }

  // Sort moves by priority (best first)
  qsort(moves, move_count, sizeof(move_t), compare_moves);

  int best_x = -1, best_y = -1;
  int original_alpha = alpha;
  int original_beta = beta;

  if (maximizing_player) {
    int max_eval = -WIN_SCORE - 1;

    for (int m = 0; m < move_count; m++) {
      // Check for timeout before evaluating each move
      if (is_search_timed_out(game)) {
        game->search_timed_out = 1;
        return max_eval;
      }

      int i = moves[m].x;
      int j = moves[m].y;

      board[i][j] = current_player_turn;

      // Update hash incrementally
      int player_index = (current_player_turn == AI_CELL_CROSSES) ? 0 : 1;
      int pos = i * game->board_size + j;
      game->current_hash ^= game->zobrist_keys[player_index][pos];

      // Temporary cache invalidation
      invalidate_winner_cache(game);

      int eval = minimax_with_timeout(game, board, depth - 1, alpha, beta, 0,
                                      ai_player, i, j);

      // Restore hash
      game->current_hash ^= game->zobrist_keys[player_index][pos];

      // Restore cache
      invalidate_winner_cache(game);

      board[i][j] = AI_CELL_EMPTY;

      if (eval > max_eval) {
        max_eval = eval;
        best_x = i;
        best_y = j;
      }
      alpha = max(alpha, eval);

      // Early termination for winning moves
      if (eval >= WIN_SCORE - 1000) {
        break;
      }

      if (beta <= alpha) {
        break; // Alpha-beta pruning
      }
    }

    // Store in transposition table
    int flag = (max_eval <= original_alpha)  ? TT_UPPER_BOUND
               : (max_eval >= original_beta) ? TT_LOWER_BOUND
                                             : TT_EXACT;
    store_transposition(game, hash, max_eval, depth, flag, best_x, best_y);

    // Store killer move if beta cutoff occurred
    if (max_eval >= original_beta && best_x != -1) {
      store_killer_move(game, depth, best_x, best_y);
    }

    return max_eval;

  } else {
    int min_eval = WIN_SCORE + 1;

    for (int m = 0; m < move_count; m++) {
      // Check for timeout before evaluating each move
      if (is_search_timed_out(game)) {
        game->search_timed_out = 1;
        return min_eval;
      }

      int i = moves[m].x;
      int j = moves[m].y;

      board[i][j] = current_player_turn;

      // Update hash incrementally
      int player_index = (current_player_turn == AI_CELL_CROSSES) ? 0 : 1;
      int pos = i * game->board_size + j;
      game->current_hash ^= game->zobrist_keys[player_index][pos];

      // Temporary cache invalidation
      invalidate_winner_cache(game);

      int eval = minimax_with_timeout(game, board, depth - 1, alpha, beta, 1,
                                      ai_player, i, j);

      // Restore hash
      game->current_hash ^= game->zobrist_keys[player_index][pos];

      // Restore cache
      invalidate_winner_cache(game);

      board[i][j] = AI_CELL_EMPTY;

      if (eval < min_eval) {
        min_eval = eval;
        best_x = i;
        best_y = j;
      }
      beta = min(beta, eval);

      // Early termination for losing moves
      if (eval <= -WIN_SCORE + 1000) {
        break;
      }

      if (beta <= alpha) {
        break; // Alpha-beta pruning
      }
    }

    // Store in transposition table
    int flag = (min_eval <= original_alpha)  ? TT_UPPER_BOUND
               : (min_eval >= original_beta) ? TT_LOWER_BOUND
                                             : TT_EXACT;
    store_transposition(game, hash, min_eval, depth, flag, best_x, best_y);

    // Store killer move if alpha cutoff occurred
    if (min_eval <= original_alpha && best_x != -1) {
      store_killer_move(game, depth, best_x, best_y);
    }

    return min_eval;
  }
}

//===============================================================================
// AI MOVE FINDING FUNCTIONS
//===============================================================================

void find_first_ai_move(game_state_t *game, int *best_x, int *best_y) {
  // Find the human's first move
  int human_x = -1, human_y = -1;
  for (int i = 0; i < game->board_size && human_x == -1; i++) {
    for (int j = 0; j < game->board_size && human_x == -1; j++) {
      if (game->board[i][j] == AI_CELL_CROSSES) {
        human_x = i;
        human_y = j;
      }
    }
  }

  if (human_x == -1) {
    // Fallback: place in center if no human move found
    *best_x = game->board_size / 2;
    *best_y = game->board_size / 2;
    return;
  }

  // Collect valid positions 1-2 squares away from human move
  int valid_moves[50][2]; // Enough for nearby positions
  int move_count = 0;

  for (int distance = 1; distance <= 2; distance++) {
    for (int dx = -distance; dx <= distance; dx++) {
      for (int dy = -distance; dy <= distance; dy++) {
        if (dx == 0 && dy == 0)
          continue; // Skip the human's position

        int new_x = human_x + dx;
        int new_y = human_y + dy;

        // Check bounds and if position is empty
        if (new_x >= 0 && new_x < game->board_size && new_y >= 0 &&
            new_y < game->board_size &&
            game->board[new_x][new_y] == AI_CELL_EMPTY) {
          valid_moves[move_count][0] = new_x;
          valid_moves[move_count][1] = new_y;
          move_count++;
        }
      }
    }
  }

  if (move_count > 0) {
    // Randomly select one of the valid moves
    int selected = rand() % move_count;
    *best_x = valid_moves[selected][0];
    *best_y = valid_moves[selected][1];
  } else {
    // Fallback: place adjacent to human move
    *best_x = human_x + (rand() % 3 - 1); // -1, 0, or 1
    *best_y = human_y + (rand() % 3 - 1);

    // Ensure bounds
    *best_x = max(0, min(game->board_size - 1, *best_x));
    *best_y = max(0, min(game->board_size - 1, *best_y));
  }
}

void find_best_ai_move(game_state_t *game, int *best_x, int *best_y,
                       scoring_report_t *report) {
  // Initialize timeout tracking
  game->search_start_time = get_current_time();
  game->search_timed_out = 0;

  // Initialize scoring report
  scoring_report_t local_report;
  if (!report) report = &local_report;
  scoring_report_init(report);

  // Determine which player the AI is playing as
  int ai_player = game->current_player; // Could be CROSSES or NAUGHTS
  char ai_symbol = (ai_player == AI_CELL_CROSSES) ? 'X' : 'O';
  const char *ai_color =
      (ai_player == AI_CELL_CROSSES) ? COLOR_RED : COLOR_BLUE;
  int opponent = other_player(ai_player);

  // Ensure zobrist hash is consistent with the current board
  game->current_hash = compute_zobrist_hash(game);

  // Count stones on board to detect first AI move
  int stone_count = 0;
  for (int i = 0; i < game->board_size; i++) {
    for (int j = 0; j < game->board_size; j++) {
      if (game->board[i][j] != AI_CELL_EMPTY) {
        stone_count++;
      }
    }
  }

  // If there's exactly 1 stone (human's first move), use simple random
  // placement
  if (stone_count == 1) {
    find_first_ai_move(game, best_x, best_y);
    add_ai_history_entry(game, 1); // Random placement, 1 "move" considered
    return;
  }

  // Regular minimax for subsequent moves
  *best_x = -1;
  *best_y = -1;

  // Clear previous AI status message and show thinking message
  strcpy(game->ai_status_message, "");
  if (game->config.skip_welcome && !game->config.headless) {
    if (game->move_timeout > 0) {
      printf("%s%c%s It's AI's Turn... Please wait... (timeout: %ds)\n",
             ai_color, ai_symbol, COLOR_RESET, game->move_timeout);
    } else {
      printf("%s%c%s It's AI's Turn... Please wait...\n", ai_color, ai_symbol,
             COLOR_RESET);
    }
    fflush(stdout);
  }

  // Generate and sort moves using optimized method
  move_t moves[361]; // Max for 19x19 board
  int move_count = generate_moves_optimized(game, game->board, moves, ai_player,
                                            game->max_depth);

  // =========================================================================
  // STEP 1: Check for immediate winning moves
  // =========================================================================
  double step_start = get_current_time();
  int winning_moves_x[361];
  int winning_moves_y[361];
  int winning_move_count = 0;
  int our_max_score = 0;

  for (int i = 0; i < move_count; i++) {
    int threat = evaluate_threat_fast(game->board, moves[i].x, moves[i].y,
                                      ai_player, game->board_size);
    if (threat > our_max_score) our_max_score = threat;
    if (threat >= 100000) {
      winning_moves_x[winning_move_count] = moves[i].x;
      winning_moves_y[winning_move_count] = moves[i].y;
      winning_move_count++;
    }
  }

  {
    scoring_entry_t *e = scoring_report_add(report, "have_win", 1);
    if (e) {
      e->evaluated_moves = move_count;
      e->score = our_max_score;
      e->have_win = (winning_move_count > 0);
      e->time_ms = (get_current_time() - step_start) * 1000.0;
      if (winning_move_count > 0) e->decisive = 1;
    }
    report->offensive_max_score = our_max_score;
  }

  if (winning_move_count > 0) {
    int selected = rand() % winning_move_count;
    *best_x = winning_moves_x[selected];
    *best_y = winning_moves_y[selected];
    snprintf(game->ai_status_message, sizeof(game->ai_status_message),
             "%s%c%s It's a checkmate ;-)", ai_color, ai_symbol, COLOR_RESET);
    add_ai_history_entry(game, winning_move_count);
    return;
  }

  // =========================================================================
  // STEP 2: Block opponent >= 40000 (open four / compound threats)
  // =========================================================================
  step_start = get_current_time();
  int blocking_moves_x[361];
  int blocking_moves_y[361];
  int blocking_threat_level[361];
  int blocking_move_count = 0;
  int max_opp_threat = 0;

  for (int i = 0; i < move_count; i++) {
    int opp_threat = evaluate_threat_fast(game->board, moves[i].x, moves[i].y,
                                          opponent, game->board_size);
    if (opp_threat > max_opp_threat) max_opp_threat = opp_threat;
    if (opp_threat >= 40000) {
      blocking_moves_x[blocking_move_count] = moves[i].x;
      blocking_moves_y[blocking_move_count] = moves[i].y;
      blocking_threat_level[blocking_move_count] = opp_threat;
      blocking_move_count++;
    }
  }

  {
    scoring_entry_t *e = scoring_report_add(report, "block_threat", 0);
    if (e) {
      e->evaluated_moves = move_count;
      e->score = -max_opp_threat;
      e->time_ms = (get_current_time() - step_start) * 1000.0;
      if (blocking_move_count > 0 && max_opp_threat >= 40000) e->decisive = 1;
    }
    report->defensive_max_score = -max_opp_threat;
  }

  if (blocking_move_count > 0 && max_opp_threat >= 40000) {
    int best_blocks_x[361];
    int best_blocks_y[361];
    int best_block_count = 0;

    for (int i = 0; i < blocking_move_count; i++) {
      if (blocking_threat_level[i] == max_opp_threat) {
        best_blocks_x[best_block_count] = blocking_moves_x[i];
        best_blocks_y[best_block_count] = blocking_moves_y[i];
        best_block_count++;
      }
    }

    int selected = rand() % best_block_count;
    *best_x = best_blocks_x[selected];
    *best_y = best_blocks_y[selected];
    snprintf(game->ai_status_message, sizeof(game->ai_status_message),
             "%s%c%s Blocking opponent's threat!", ai_color, ai_symbol,
             COLOR_RESET);
    add_ai_history_entry(game, blocking_move_count);
    return;
  }

  // =========================================================================
  // STEP 3: Offensive VCT — can we force a win?
  // =========================================================================
  step_start = get_current_time();
  int vct_x = -1, vct_y = -1;
  int vct_sequence[MAX_VCT_SEQUENCE][2];
  int vct_len = 0;
  int have_vct = find_forced_win(game, game->board, ai_player, 10,
                                 &vct_x, &vct_y, vct_sequence, &vct_len);

  {
    scoring_entry_t *e = scoring_report_add(report, "have_vct", 1);
    if (e) {
      e->have_vct = have_vct;
      e->score = have_vct ? WIN_SCORE : 0;
      e->time_ms = (get_current_time() - step_start) * 1000.0;
      if (have_vct) {
        e->decisive = 1;
        e->vct_length = vct_len;
        for (int i = 0; i < vct_len && i < MAX_VCT_SEQUENCE; i++) {
          e->vct_sequence[i][0] = vct_sequence[i][0];
          e->vct_sequence[i][1] = vct_sequence[i][1];
        }
      }
      if (have_vct) report->offensive_max_score = WIN_SCORE;
    }
  }

  if (have_vct) {
    *best_x = vct_x;
    *best_y = vct_y;
    snprintf(game->ai_status_message, sizeof(game->ai_status_message),
             "%s%c%s Forced win! (VCT, %d moves)", ai_color, ai_symbol,
             COLOR_RESET, vct_len);
    add_ai_history_entry(game, vct_len);
    return;
  }

  // =========================================================================
  // STEP 4: Defensive VCT — can opponent force a win?
  // =========================================================================
  step_start = get_current_time();
  int dvct_x = -1, dvct_y = -1;
  int opp_has_vct = find_forced_win_block(game, game->board, ai_player, 10,
                                          &dvct_x, &dvct_y);

  {
    scoring_entry_t *e = scoring_report_add(report, "block_vct", 0);
    if (e) {
      e->have_vct = opp_has_vct;
      e->score = opp_has_vct ? -WIN_SCORE : 0;
      e->time_ms = (get_current_time() - step_start) * 1000.0;
      if (opp_has_vct && dvct_x >= 0) e->decisive = 1;
      if (opp_has_vct) report->defensive_max_score = -WIN_SCORE;
    }
  }

  if (opp_has_vct && dvct_x >= 0) {
    *best_x = dvct_x;
    *best_y = dvct_y;
    snprintf(game->ai_status_message, sizeof(game->ai_status_message),
             "%s%c%s Breaking opponent's VCT!", ai_color, ai_symbol,
             COLOR_RESET);
    add_ai_history_entry(game, move_count);
    return;
  }

  // =========================================================================
  // STEP 5: Block opponent open three (1500) only if we have NO initiative
  // Narrowed: only true open three (1500) and compound threats (30000-40000)
  // NOT 2000 (shared open twos), 3000 (developing), or 8000 (gapped four)
  // =========================================================================
  step_start = get_current_time();
  int open_three_blocks_x[361];
  int open_three_blocks_y[361];
  int open_three_block_threat[361];
  int open_three_block_count = 0;
  int max_open_three_threat = 0;

  for (int i = 0; i < move_count; i++) {
    int opp_threat = evaluate_threat_fast(game->board, moves[i].x, moves[i].y,
                                          opponent, game->board_size);
    int needs_blocking = (opp_threat == 1500) ||
                         (opp_threat >= 30000 && opp_threat < 40000);
    if (needs_blocking) {
      open_three_blocks_x[open_three_block_count] = moves[i].x;
      open_three_blocks_y[open_three_block_count] = moves[i].y;
      open_three_block_threat[open_three_block_count] = opp_threat;
      if (opp_threat > max_open_three_threat) {
        max_open_three_threat = opp_threat;
      }
      open_three_block_count++;
    }
  }

  int blocked_open_three = 0;
  if (open_three_block_count > 0) {
    // Check if we have initiative
    int our_max_threat_val = 0;
    int our_four_count = 0;
    int our_open_three_count = 0;

    for (int i = 0; i < move_count; i++) {
      int my_threat = evaluate_threat_fast(game->board, moves[i].x, moves[i].y,
                                           ai_player, game->board_size);
      if (my_threat > our_max_threat_val) our_max_threat_val = my_threat;
      if (my_threat >= 10000) our_four_count++;
      else if (my_threat >= 1500) our_open_three_count++;
    }

    int we_have_initiative =
        (our_max_threat_val >= 40000) ||
        (our_four_count >= 2) ||
        (our_four_count >= 1 && our_open_three_count >= 1) ||
        (our_max_threat_val >= 1500 && our_max_threat_val > max_open_three_threat);

    if (!we_have_initiative) {
      int best_blocks_x[361];
      int best_blocks_y[361];
      int best_block_count = 0;

      for (int i = 0; i < open_three_block_count; i++) {
        if (open_three_block_threat[i] == max_open_three_threat) {
          best_blocks_x[best_block_count] = open_three_blocks_x[i];
          best_blocks_y[best_block_count] = open_three_blocks_y[i];
          best_block_count++;
        }
      }

      int best_block_idx = 0;
      int best_block_own_threat = 0;
      for (int i = 0; i < best_block_count; i++) {
        int own_threat =
            evaluate_threat_fast(game->board, best_blocks_x[i],
                                 best_blocks_y[i], ai_player, game->board_size);
        if (own_threat > best_block_own_threat) {
          best_block_own_threat = own_threat;
          best_block_idx = i;
        }
      }

      *best_x = best_blocks_x[best_block_idx];
      *best_y = best_blocks_y[best_block_idx];
      blocked_open_three = 1;

      snprintf(game->ai_status_message, sizeof(game->ai_status_message),
               "%s%c%s Blocking opponent's open three!", ai_color, ai_symbol,
               COLOR_RESET);
    }
  }

  {
    scoring_entry_t *e = scoring_report_add(report, "block_open_three", 0);
    if (e) {
      e->evaluated_moves = open_three_block_count;
      e->score = -max_open_three_threat;
      e->time_ms = (get_current_time() - step_start) * 1000.0;
      if (blocked_open_three) e->decisive = 1;
    }
  }

  if (blocked_open_three) {
    add_ai_history_entry(game, open_three_block_count);
    return;
  }

  // =========================================================================
  // STEP 6: Play our forcing four
  // =========================================================================
  step_start = get_current_time();
  int forcing_moves_x[361];
  int forcing_moves_y[361];
  int forcing_move_count = 0;
  int max_forcing_threat = 0;

  for (int i = 0; i < move_count; i++) {
    int my_threat = evaluate_threat_fast(game->board, moves[i].x, moves[i].y,
                                         ai_player, game->board_size);
    if (my_threat >= 10000) {
      forcing_moves_x[forcing_move_count] = moves[i].x;
      forcing_moves_y[forcing_move_count] = moves[i].y;
      forcing_move_count++;
      if (my_threat > max_forcing_threat) {
        max_forcing_threat = my_threat;
      }
    }
  }

  int played_forcing = 0;
  if (forcing_move_count > 0) {
    for (int i = 0; i < move_count; i++) {
      int my_threat = evaluate_threat_fast(game->board, moves[i].x, moves[i].y,
                                           ai_player, game->board_size);
      if (my_threat == max_forcing_threat) {
        *best_x = moves[i].x;
        *best_y = moves[i].y;
        played_forcing = 1;
        snprintf(game->ai_status_message, sizeof(game->ai_status_message),
                 "%s%c%s Creating forcing threat!", ai_color, ai_symbol,
                 COLOR_RESET);
        break;
      }
    }
  }

  {
    scoring_entry_t *e = scoring_report_add(report, "forcing_four", 1);
    if (e) {
      e->evaluated_moves = forcing_move_count;
      e->score = max_forcing_threat;
      e->time_ms = (get_current_time() - step_start) * 1000.0;
      if (played_forcing) e->decisive = 1;
    }
  }

  if (played_forcing) {
    add_ai_history_entry(game, forcing_move_count);
    return;
  }

  // =========================================================================
  // STEP 7: Minimax iterative deepening search
  // =========================================================================
  step_start = get_current_time();

  // Sort moves by priority (best first)
  qsort(moves, move_count, sizeof(move_t), compare_moves);

  int moves_considered = 0;

  // Ensure we have a fallback move in case timeout occurs immediately
  if (move_count > 0 && *best_x == -1) {
    *best_x = moves[0].x;
    *best_y = moves[0].y;
  }

  int final_best_score = -WIN_SCORE - 1;

  // Iterative deepening search
  for (int current_depth = 1; current_depth <= game->max_depth;
       current_depth++) {
    if (is_search_timed_out(game)) {
      break;
    }

    int depth_best_score = -WIN_SCORE - 1;

    // Track all moves with the best score for random selection
    int best_moves_x[361];
    int best_moves_y[361];
    int best_moves_count = 0;

    // Search all moves at current depth
    for (int m = 0; m < move_count; m++) {
      // Check for timeout before evaluating each move
      if (is_search_timed_out(game)) {
        game->search_timed_out = 1;
        break;
      }

      int i = moves[m].x;
      int j = moves[m].y;

      game->board[i][j] = ai_player;

      // Update hash incrementally
      int player_index = (ai_player == AI_CELL_CROSSES) ? 0 : 1;
      int pos = i * game->board_size + j;
      game->current_hash ^= game->zobrist_keys[player_index][pos];

      int score = minimax_with_timeout(game, game->board, current_depth - 1,
                                       -WIN_SCORE - 1, WIN_SCORE + 1, 0,
                                       ai_player, i, j);

      // Restore hash
      game->current_hash ^= game->zobrist_keys[player_index][pos];

      game->board[i][j] = AI_CELL_EMPTY;

      if (score > depth_best_score) {
        depth_best_score = score;
        best_moves_x[0] = i;
        best_moves_y[0] = j;
        best_moves_count = 1;

        if (score >= WIN_SCORE - 1000) {
          snprintf(game->ai_status_message, sizeof(game->ai_status_message),
                   "%s%s%s Win (depth %d, %d moves).", COLOR_BLUE, "O",
                   COLOR_RESET, current_depth, moves_considered + 1);
          *best_x = i;
          *best_y = j;
          final_best_score = score;
          add_ai_history_entry(game, moves_considered + 1);

          // Record minimax scoring entry before returning
          {
            scoring_entry_t *e = scoring_report_add(report, "minimax", 1);
            if (e) {
              e->evaluated_moves = moves_considered + 1;
              e->score = score;
              e->have_win = 1;
              e->time_ms = (get_current_time() - step_start) * 1000.0;
            }
            report->offensive_max_score = max(report->offensive_max_score, score);
          }
          return;
        }
      } else if (score == depth_best_score && best_moves_count < 361) {
        best_moves_x[best_moves_count] = i;
        best_moves_y[best_moves_count] = j;
        best_moves_count++;
      }

      moves_considered++;
      if (current_depth == game->max_depth && !game->config.headless) {
        printf("%s•%s", COLOR_BLUE, COLOR_RESET);
        fflush(stdout);
      }

      if (game->search_timed_out) {
        break;
      }
    }

    if (!game->search_timed_out && best_moves_count > 0) {
      int selected = rand() % best_moves_count;
      *best_x = best_moves_x[selected];
      *best_y = best_moves_y[selected];
      final_best_score = depth_best_score;
    }
  }

  {
    scoring_entry_t *e = scoring_report_add(report, "minimax", 1);
    if (e) {
      e->evaluated_moves = moves_considered;
      e->score = final_best_score;
      e->time_ms = (get_current_time() - step_start) * 1000.0;
    }
    report->offensive_max_score = max(report->offensive_max_score, final_best_score);
  }

  // Store the completion message if not already set by early termination
  if (strlen(game->ai_status_message) == 0) {
    double elapsed = get_current_time() - game->search_start_time;
    if (game->search_timed_out) {
      snprintf(game->ai_status_message, sizeof(game->ai_status_message),
               "%.0fs timeout, checked %d moves", elapsed, moves_considered);
    } else {
      snprintf(game->ai_status_message, sizeof(game->ai_status_message),
               "Done in %.0fs (checked %d moves)", elapsed, moves_considered);
    }
  }

  // Add to AI history
  add_ai_history_entry(game, moves_considered);
}
