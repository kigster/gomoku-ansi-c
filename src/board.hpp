//
//  board.hpp
//  gomoku - Modern C++23 Board Management
//
//  Converted from C99 to modern C++ with RAII, ranges, and type safety
//

#pragma once

#include "gomoku.hpp"
#include <vector>
#include <array>
#include <span>
#include <memory>
#include <string>
#include <ranges>
#include <algorithm>

namespace gomoku {

//===============================================================================
// BOARD CLASS
//===============================================================================

template<int Size = DEFAULT_BOARD_SIZE> requires ValidBoardSize<Size>
class Board {
public:
    static constexpr int size = Size;
    static constexpr int total_cells = Size * Size;
    
    using BoardData = std::array<std::array<Player, Size>, Size>;
    using Row = std::span<Player, Size>;
    using ConstRow = std::span<const Player, Size>;

private:
    BoardData board_;
    int stone_count_ = 0;

public:
    //===============================================================================
    // CONSTRUCTORS AND DESTRUCTORS
    //===============================================================================
    
    constexpr Board() noexcept {
        clear();
    }
    
    // Copy and move constructors/assignments use default implementations
    Board(const Board&) = default;
    Board(Board&&) noexcept = default;
    Board& operator=(const Board&) = default;
    Board& operator=(Board&&) noexcept = default;
    ~Board() = default;

    //===============================================================================
    // BOARD ACCESS
    //===============================================================================
    
    [[nodiscard]] constexpr Player at(const Position& pos) const noexcept {
        return board_[pos.x][pos.y];
    }
    
    [[nodiscard]] constexpr Player at(int x, int y) const noexcept {
        return board_[x][y];
    }
    
    constexpr void set(const Position& pos, Player player) noexcept {
        if (board_[pos.x][pos.y] == Player::Empty && player != Player::Empty) {
            ++stone_count_;
        } else if (board_[pos.x][pos.y] != Player::Empty && player == Player::Empty) {
            --stone_count_;
        }
        board_[pos.x][pos.y] = player;
    }
    
    constexpr void set(int x, int y, Player player) noexcept {
        set({x, y}, player);
    }
    
    // Array-like access
    [[nodiscard]] constexpr auto operator[](int x) noexcept -> Row {
        return std::span{board_[x]};
    }
    
    [[nodiscard]] constexpr auto operator[](int x) const noexcept -> ConstRow {
        return std::span{board_[x]};
    }

    //===============================================================================
    // BOARD STATE
    //===============================================================================
    
    constexpr void clear() noexcept {
        for (auto& row : board_) {
            std::ranges::fill(row, Player::Empty);
        }
        stone_count_ = 0;
    }
    
    [[nodiscard]] constexpr bool is_empty() const noexcept {
        return stone_count_ == 0;
    }
    
    [[nodiscard]] constexpr bool is_full() const noexcept {
        return stone_count_ == total_cells;
    }
    
    [[nodiscard]] constexpr int stone_count() const noexcept {
        return stone_count_;
    }
    
    [[nodiscard]] constexpr int empty_count() const noexcept {
        return total_cells - stone_count_;
    }

    //===============================================================================
    // MOVE VALIDATION
    //===============================================================================
    
    [[nodiscard]] constexpr bool is_valid_position(const Position& pos) const noexcept {
        return pos.is_valid(Size);
    }
    
    [[nodiscard]] constexpr bool is_valid_position(int x, int y) const noexcept {
        return is_valid_position({x, y});
    }
    
    [[nodiscard]] constexpr bool is_empty_at(const Position& pos) const noexcept {
        return is_valid_position(pos) && at(pos) == Player::Empty;
    }
    
    [[nodiscard]] constexpr bool is_empty_at(int x, int y) const noexcept {
        return is_empty_at({x, y});
    }
    
    [[nodiscard]] constexpr bool is_valid_move(const Position& pos) const noexcept {
        return is_empty_at(pos);
    }
    
    [[nodiscard]] constexpr bool is_valid_move(int x, int y) const noexcept {
        return is_valid_move({x, y});
    }

    //===============================================================================
    // WIN CONDITION CHECKING
    //===============================================================================
    
    [[nodiscard]] constexpr bool has_winner(Player player) const noexcept {
        // Check all positions for potential winning lines
        for (int x = 0; x < Size; ++x) {
            for (int y = 0; y < Size; ++y) {
                if (at(x, y) == player && check_win_from_position({x, y}, player)) {
                    return true;
                }
            }
        }
        return false;
    }
    
    [[nodiscard]] constexpr bool check_win_from_position(const Position& pos, Player player) const noexcept {
        // Check all four directions from this position
        return std::ranges::any_of(DIRECTIONS, [&](const Position& dir) {
            return count_line(pos, dir, player) >= NEED_TO_WIN;
        });
    }

    //===============================================================================
    // LINE ANALYSIS
    //===============================================================================
    
    [[nodiscard]] constexpr int count_line(const Position& start, const Position& direction, Player player) const noexcept {
        int count = 0;
        
        // Count backwards
        for (int i = 1; i < NEED_TO_WIN; ++i) {
            Position pos = start + (direction * -i);
            if (!is_valid_position(pos) || at(pos) != player) break;
            ++count;
        }
        
        // Count current position
        if (at(start) == player) ++count;
        
        // Count forwards
        for (int i = 1; i < NEED_TO_WIN; ++i) {
            Position pos = start + (direction * i);
            if (!is_valid_position(pos) || at(pos) != player) break;
            ++count;
        }
        
        return count;
    }
    
    // Get line of stones in a direction (for pattern analysis)
    [[nodiscard]] constexpr std::array<Player, NEED_TO_WIN * 2 - 1> 
    get_line(const Position& center, const Position& direction) const noexcept {
        std::array<Player, NEED_TO_WIN * 2 - 1> line;
        
        for (int i = -(NEED_TO_WIN - 1); i <= (NEED_TO_WIN - 1); ++i) {
            Position pos = center + (direction * i);
            line[i + (NEED_TO_WIN - 1)] = is_valid_position(pos) ? at(pos) : Player::Empty;
        }
        
        return line;
    }

    //===============================================================================
    // ITERATION SUPPORT
    //===============================================================================
    
    // Range-based iteration support
    [[nodiscard]] constexpr auto begin() noexcept { return board_.begin(); }
    [[nodiscard]] constexpr auto end() noexcept { return board_.end(); }
    [[nodiscard]] constexpr auto begin() const noexcept { return board_.begin(); }
    [[nodiscard]] constexpr auto end() const noexcept { return board_.end(); }
    [[nodiscard]] constexpr auto cbegin() const noexcept { return board_.cbegin(); }
    [[nodiscard]] constexpr auto cend() const noexcept { return board_.cend(); }
    
    // Get all positions with a specific player
    [[nodiscard]] std::vector<Position> positions_with_player(Player player) const {
        std::vector<Position> positions;
        for (int x = 0; x < Size; ++x) {
            for (int y = 0; y < Size; ++y) {
                if (at(x, y) == player) {
                    positions.emplace_back(x, y);
                }
            }
        }
        return positions;
    }
    
    // Get all empty positions
    [[nodiscard]] std::vector<Position> empty_positions() const {
        return positions_with_player(Player::Empty);
    }

    //===============================================================================
    // COORDINATE UTILITIES
    //===============================================================================
    
    // Convert 0-based coordinate to display string (A-S for 19x19)
    [[nodiscard]] static constexpr char index_to_coordinate(int index) noexcept {
        return static_cast<char>('A' + index);
    }
    
    // Convert display coordinate to 0-based index
    [[nodiscard]] static constexpr int coordinate_to_index(char coord) noexcept {
        return static_cast<int>(std::toupper(coord) - 'A');
    }
    
    // Convert board coordinate to display coordinate (1-based)
    [[nodiscard]] static constexpr int board_to_display_coord(int board_coord) noexcept {
        return board_coord + 1;
    }
    
    // Convert display coordinate to board coordinate (0-based)
    [[nodiscard]] static constexpr int display_to_board_coord(int display_coord) noexcept {
        return display_coord - 1;
    }

    //===============================================================================
    // STRING REPRESENTATION
    //===============================================================================
    
    [[nodiscard]] std::string to_string() const {
        std::string result;
        result.reserve((Size + 1) * (Size * 2 + 1)); // Rough size estimate
        
        for (int x = 0; x < Size; ++x) {
            for (int y = 0; y < Size; ++y) {
                result += player_to_unicode(at(x, y));
                if (y < Size - 1) result += " ";
            }
            if (x < Size - 1) result += "\n";
        }
        
        return result;
    }

    //===============================================================================
    // COMPARISON OPERATORS
    //===============================================================================
    
    constexpr bool operator==(const Board& other) const noexcept = default;
    constexpr auto operator<=>(const Board& other) const noexcept = default;

    //===============================================================================
    // DEBUGGING SUPPORT
    //===============================================================================
    
    // Get board statistics
    struct Statistics {
        int cross_count = 0;
        int naught_count = 0;
        int empty_count = 0;
        double fill_ratio = 0.0;
    };
    
    [[nodiscard]] Statistics get_statistics() const noexcept {
        Statistics stats;
        stats.empty_count = empty_count();
        
        // Count manually since ranges::join may not work with nested arrays
        for (int x = 0; x < Size; ++x) {
            for (int y = 0; y < Size; ++y) {
                if (at(x, y) == Player::Cross) {
                    ++stats.cross_count;
                } else if (at(x, y) == Player::Naught) {
                    ++stats.naught_count;
                }
            }
        }
        
        stats.fill_ratio = static_cast<double>(stone_count_) / total_cells;
        return stats;
    }
};

//===============================================================================
// TYPE ALIASES FOR COMMON BOARD SIZES
//===============================================================================

using Board15 = Board<15>;
using Board19 = Board<19>;
using StandardBoard = Board<DEFAULT_BOARD_SIZE>;

//===============================================================================
// UTILITY FUNCTIONS
//===============================================================================

// Factory function for dynamic board size
[[nodiscard]] std::unique_ptr<class BoardInterface> create_board(int size);

// Abstract interface for dynamic polymorphism when needed
class BoardInterface {
public:
    virtual ~BoardInterface() = default;
    [[nodiscard]] virtual int size() const noexcept = 0;
    [[nodiscard]] virtual Player at(const Position& pos) const noexcept = 0;
    [[nodiscard]] virtual Player at(int x, int y) const noexcept = 0;
    virtual void set(const Position& pos, Player player) noexcept = 0;
    virtual void set(int x, int y, Player player) noexcept = 0;
    virtual void clear() noexcept = 0;
    [[nodiscard]] virtual bool is_valid_move(const Position& pos) const noexcept = 0;
    [[nodiscard]] virtual bool is_valid_move(int x, int y) const noexcept = 0;
    [[nodiscard]] virtual bool has_winner(Player player) const noexcept = 0;
    [[nodiscard]] virtual int stone_count() const noexcept = 0;
    [[nodiscard]] virtual std::string to_string() const = 0;
};

} // namespace gomoku