//
//  board.cpp
//  gomoku - Modern C++23 Board Implementation
//
//  Implementation of dynamic board factory and interface
//

#include "board.hpp"
#include <memory>
#include <stdexcept>

namespace gomoku {

//===============================================================================
// CONCRETE BOARD IMPLEMENTATIONS
//===============================================================================

template<int Size> requires ValidBoardSize<Size>
class ConcreteBoardImpl : public BoardInterface {
private:
    Board<Size> board_;

public:
    ConcreteBoardImpl() = default;
    
    [[nodiscard]] int size() const noexcept override {
        return Size;
    }
    
    [[nodiscard]] Player at(const Position& pos) const noexcept override {
        return board_.at(pos);
    }
    
    [[nodiscard]] Player at(int x, int y) const noexcept override {
        return board_.at(x, y);
    }
    
    void set(const Position& pos, Player player) noexcept override {
        board_.set(pos, player);
    }
    
    void set(int x, int y, Player player) noexcept override {
        board_.set(x, y, player);
    }
    
    void clear() noexcept override {
        board_.clear();
    }
    
    [[nodiscard]] bool is_valid_move(const Position& pos) const noexcept override {
        return board_.is_valid_move(pos);
    }
    
    [[nodiscard]] bool is_valid_move(int x, int y) const noexcept override {
        return board_.is_valid_move(x, y);
    }
    
    [[nodiscard]] bool has_winner(Player player) const noexcept override {
        return board_.has_winner(player);
    }
    
    [[nodiscard]] int stone_count() const noexcept override {
        return board_.stone_count();
    }
    
    [[nodiscard]] std::string to_string() const override {
        return board_.to_string();
    }
    
    // Allow access to the underlying strongly-typed board
    [[nodiscard]] Board<Size>& get_board() noexcept { return board_; }
    [[nodiscard]] const Board<Size>& get_board() const noexcept { return board_; }
};

//===============================================================================
// FACTORY FUNCTION
//===============================================================================

[[nodiscard]] std::unique_ptr<BoardInterface> create_board(int size) {
    switch (size) {
        case 15:
            return std::make_unique<ConcreteBoardImpl<15>>();
        case 19:
            return std::make_unique<ConcreteBoardImpl<19>>();
        default:
            throw std::invalid_argument(
                std::format("Invalid board size: {}. Must be 15 or 19.", size)
            );
    }
}

//===============================================================================
// COORDINATE UTILITY IMPLEMENTATIONS
//===============================================================================

namespace detail {
    // Unicode coordinate display characters
    constexpr std::array<std::string_view, 19> COORDINATE_UNICODE = {{
        "❶", "❷", "❸", "❹", "❺", "❻", "❼", "❽", "❾", "❿",
        "⓫", "⓬", "⓭", "⓮", "⓯", "⓰", "⓱", "⓲", "⓳"
    }};
}

[[nodiscard]] std::string_view get_coordinate_unicode(int index) noexcept {
    if (index >= 0 && index < static_cast<int>(detail::COORDINATE_UNICODE.size())) {
        return detail::COORDINATE_UNICODE[index];
    }
    return "?";
}

} // namespace gomoku