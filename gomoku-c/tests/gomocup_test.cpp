//
//  gomocup_test.cpp
//  gomoku-c — Gomocup brain
//
//  GoogleTest cases for the standalone parser + coordinate translation.
//  The brain's main loop is exercised separately by gomocup_protocol_e2e.sh.
//

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "coords.h"
#include "protocol.h"
}

// ----- coords ---------------------------------------------------------------

TEST(GomocupCoordsTest, RoundTripAllCells) {
  // The mapping is trivial but cheap to over-verify; if anyone ever
  // re-defines an axis we want a hard failure.
  for (int gy = 0; gy < 15; gy++) {
    for (int gx = 0; gx < 15; gx++) {
      int row = -1, col = -1;
      gomocup_to_engine(gx, gy, &row, &col);
      EXPECT_EQ(row, gy);
      EXPECT_EQ(col, gx);
      int gx2 = -1, gy2 = -1;
      engine_to_gomocup(row, col, &gx2, &gy2);
      EXPECT_EQ(gx2, gx);
      EXPECT_EQ(gy2, gy);
    }
  }
}

TEST(GomocupCoordsTest, BoundsCheck) {
  EXPECT_TRUE(gomocup_coord_in_bounds(0, 0, 15));
  EXPECT_TRUE(gomocup_coord_in_bounds(14, 14, 15));
  EXPECT_FALSE(gomocup_coord_in_bounds(-1, 0, 15));
  EXPECT_FALSE(gomocup_coord_in_bounds(0, -1, 15));
  EXPECT_FALSE(gomocup_coord_in_bounds(15, 0, 15));
  EXPECT_FALSE(gomocup_coord_in_bounds(0, 15, 15));
}

// ----- protocol parser ------------------------------------------------------

TEST(GomocupProtocolTest, EmptyLine) {
  parsed_command_t cmd;
  protocol_parse_line("", &cmd);
  EXPECT_EQ(cmd.kind, CMD_EMPTY);

  protocol_parse_line("   \r\n", &cmd);
  EXPECT_EQ(cmd.kind, CMD_EMPTY);
}

TEST(GomocupProtocolTest, ParseStart) {
  parsed_command_t cmd;
  protocol_parse_line("START 15", &cmd);
  EXPECT_EQ(cmd.kind, CMD_START);
  EXPECT_EQ(cmd.width, 15);
  EXPECT_EQ(cmd.height, 15);

  protocol_parse_line("START 20\r\n", &cmd);
  EXPECT_EQ(cmd.kind, CMD_START);
  EXPECT_EQ(cmd.width, 20);

  protocol_parse_line("start 19", &cmd);
  EXPECT_EQ(cmd.kind, CMD_START);
  EXPECT_EQ(cmd.width, 19);
}

TEST(GomocupProtocolTest, ParseStartInvalid) {
  parsed_command_t cmd;
  protocol_parse_line("START", &cmd);
  EXPECT_EQ(cmd.kind, CMD_INVALID);
  protocol_parse_line("START -1", &cmd);
  EXPECT_EQ(cmd.kind, CMD_INVALID);
  protocol_parse_line("START garbage", &cmd);
  EXPECT_EQ(cmd.kind, CMD_INVALID);
}

TEST(GomocupProtocolTest, ParseTurn) {
  parsed_command_t cmd;
  protocol_parse_line("TURN 7,7", &cmd);
  EXPECT_EQ(cmd.kind, CMD_TURN);
  EXPECT_EQ(cmd.x, 7);
  EXPECT_EQ(cmd.y, 7);

  protocol_parse_line("TURN 7,8\r\n", &cmd);
  EXPECT_EQ(cmd.kind, CMD_TURN);
  EXPECT_EQ(cmd.x, 7);
  EXPECT_EQ(cmd.y, 8);

  protocol_parse_line("TURN  7 , 8 ", &cmd);
  EXPECT_EQ(cmd.kind, CMD_TURN);
  EXPECT_EQ(cmd.x, 7);
  EXPECT_EQ(cmd.y, 8);
}

TEST(GomocupProtocolTest, ParseTurnInvalid) {
  parsed_command_t cmd;
  protocol_parse_line("TURN 7", &cmd);
  EXPECT_EQ(cmd.kind, CMD_INVALID);
  protocol_parse_line("TURN", &cmd);
  EXPECT_EQ(cmd.kind, CMD_INVALID);
  protocol_parse_line("TURN garbage", &cmd);
  EXPECT_EQ(cmd.kind, CMD_INVALID);
}

TEST(GomocupProtocolTest, ParseBoardKeyword) {
  parsed_command_t cmd;
  protocol_parse_line("BOARD", &cmd);
  EXPECT_EQ(cmd.kind, CMD_BOARD);
}

TEST(GomocupProtocolTest, ParseBoardRow) {
  int x = -1, y = -1, field = -1;
  EXPECT_EQ(protocol_parse_board_row("7,7,1", &x, &y, &field), 1);
  EXPECT_EQ(x, 7);
  EXPECT_EQ(y, 7);
  EXPECT_EQ(field, 1);

  EXPECT_EQ(protocol_parse_board_row("8,7,2\r\n", &x, &y, &field), 1);
  EXPECT_EQ(x, 8);
  EXPECT_EQ(y, 7);
  EXPECT_EQ(field, 2);

  EXPECT_EQ(protocol_parse_board_row("DONE", &x, &y, &field), 0);
  EXPECT_EQ(protocol_parse_board_row("", &x, &y, &field), 0);
  EXPECT_EQ(protocol_parse_board_row("7,7", &x, &y, &field), 0);
}

TEST(GomocupProtocolTest, ParseInfoKnown) {
  parsed_command_t cmd;
  protocol_parse_line("INFO timeout_turn 5000", &cmd);
  EXPECT_EQ(cmd.kind, CMD_INFO);
  EXPECT_STREQ(cmd.info_key, "timeout_turn");
  EXPECT_STREQ(cmd.info_value, "5000");

  protocol_parse_line("INFO time_left 12345\r\n", &cmd);
  EXPECT_EQ(cmd.kind, CMD_INFO);
  EXPECT_STREQ(cmd.info_key, "time_left");
  EXPECT_STREQ(cmd.info_value, "12345");
}

TEST(GomocupProtocolTest, ParseInfoUnknownKey) {
  // Per spec, unknown keys are tolerated; we still classify the verb as INFO.
  parsed_command_t cmd;
  protocol_parse_line("INFO foo bar baz", &cmd);
  EXPECT_EQ(cmd.kind, CMD_INFO);
  EXPECT_STREQ(cmd.info_key, "foo");
  EXPECT_STREQ(cmd.info_value, "bar baz");
}

TEST(GomocupProtocolTest, ParseInfoNoValue) {
  parsed_command_t cmd;
  protocol_parse_line("INFO", &cmd);
  EXPECT_EQ(cmd.kind, CMD_INVALID);
}

TEST(GomocupProtocolTest, ParseSimpleVerbs) {
  parsed_command_t cmd;
  protocol_parse_line("BEGIN", &cmd);
  EXPECT_EQ(cmd.kind, CMD_BEGIN);
  protocol_parse_line("END\r\n", &cmd);
  EXPECT_EQ(cmd.kind, CMD_END);
  protocol_parse_line("ABOUT", &cmd);
  EXPECT_EQ(cmd.kind, CMD_ABOUT);
  protocol_parse_line("RESTART", &cmd);
  EXPECT_EQ(cmd.kind, CMD_RESTART);
  protocol_parse_line("SWAP2BOARD", &cmd);
  EXPECT_EQ(cmd.kind, CMD_SWAP2BOARD);
}

TEST(GomocupProtocolTest, ParseRectStart) {
  parsed_command_t cmd;
  protocol_parse_line("RECTSTART 15,20", &cmd);
  EXPECT_EQ(cmd.kind, CMD_RECTSTART);
  EXPECT_EQ(cmd.width, 15);
  EXPECT_EQ(cmd.height, 20);
}

TEST(GomocupProtocolTest, ParseTakeback) {
  parsed_command_t cmd;
  protocol_parse_line("TAKEBACK 3,4", &cmd);
  EXPECT_EQ(cmd.kind, CMD_TAKEBACK);
  EXPECT_EQ(cmd.x, 3);
  EXPECT_EQ(cmd.y, 4);
}

TEST(GomocupProtocolTest, UnknownVerbPreservesRaw) {
  parsed_command_t cmd;
  protocol_parse_line("FROBNICATE foo bar\r\n", &cmd);
  EXPECT_EQ(cmd.kind, CMD_UNKNOWN);
  // Raw must be CR/LF-stripped so an UNKNOWN reply does not double-newline.
  EXPECT_STREQ(cmd.raw, "FROBNICATE foo bar");
}

TEST(GomocupProtocolTest, FormatMove) {
  char buf[16] = {0};
  size_t n = protocol_format_move(7, 7, buf, sizeof(buf));
  EXPECT_GT(n, 0u);
  EXPECT_STREQ(buf, "7,7");

  n = protocol_format_move(14, 0, buf, sizeof(buf));
  EXPECT_GT(n, 0u);
  EXPECT_STREQ(buf, "14,0");
}
