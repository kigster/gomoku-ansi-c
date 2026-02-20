#ifndef NET_TEST_CLIENT_UTILS_H
#define NET_TEST_CLIENT_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create an initial game state JSON payload (same depth for both players).
 * Caller owns the returned buffer.
 */
char *test_client_create_initial_game_state(int board_size, int depth,
                                             int radius);

/**
 * Create an initial game state JSON with depth_x, depth_o and optional timeout.
 * If timeout_sec is 0, "timeout" is sent as "none".
 * Caller owns the returned buffer.
 */
char *test_client_create_initial_game_state_ex(int board_size, int depth_x,
                                                int depth_o, int radius,
                                                int timeout_sec);

/**
 * Extract the last move (player label and position) from JSON response.
 * Returns 1 on success, 0 on failure.
 */
int test_client_get_last_move(const char *json, const char **out_label,
                              int *out_x, int *out_y);

#ifdef __cplusplus
}
#endif

#endif // NET_TEST_CLIENT_UTILS_H
