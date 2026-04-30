//
//  coords.h
//  gomoku-c — Gomocup brain
//
//  Coordinate translation between the Gomocup wire protocol and the engine.
//  Centralised here so every other layer stays in engine-native (row, col).
//

#ifndef GOMOCUP_COORDS_H
#define GOMOCUP_COORDS_H

// Gomocup wire protocol uses [X],[Y] with X = column, Y = row, 0-indexed,
// origin top-left. The engine's board[row][col] indexes are (x, y) where the
// FIRST index is the row and the SECOND is the column. The two conventions
// disagree on which axis the letter "x" labels, so all translation funnels
// through this module to keep the rest of the brain unambiguous.

/**
 * Translate a Gomocup wire-protocol pair (gx, gy) to engine indices
 * (row, col). gx is the column on the wire; gy is the row on the wire.
 *
 * @param gx Gomocup X (column on the wire)
 * @param gy Gomocup Y (row on the wire)
 * @param row Output: engine row index (= gy)
 * @param col Output: engine column index (= gx)
 */
void gomocup_to_engine(int gx, int gy, int *row, int *col);

/**
 * Translate engine indices (row, col) back to a Gomocup wire pair.
 *
 * @param row Engine row index
 * @param col Engine column index
 * @param gx Output: Gomocup X (column on the wire) (= col)
 * @param gy Output: Gomocup Y (row on the wire) (= row)
 */
void engine_to_gomocup(int row, int col, int *gx, int *gy);

/**
 * Validate that a Gomocup pair fits inside a square board of `size`.
 *
 * @return 1 if both coordinates are in [0, size), 0 otherwise.
 */
int gomocup_coord_in_bounds(int gx, int gy, int size);

#endif // GOMOCUP_COORDS_H
