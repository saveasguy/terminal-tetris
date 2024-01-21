#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <sstream>

#include <Windows.h>

namespace {

    struct Mino final {
        int8_t x;
        int8_t y;
    };

    using BucketT = std::array<std::array<bool, 10>, 20>;
    using MinosT = std::array<Mino, 4>;

    class Tetromino final {
    public:
        enum class BlockStyle : unsigned {
            I, J, L, O, S, T, Z, Undef
        };

    public:
        int8_t x;
        int8_t y;

    public:
        static Tetromino I() {
            return { {{ { -2, 0 }, { -1, 0 }, { 0, 0 }, { 1, 0 } }}, BlockStyle::I };
        }
        static Tetromino J() {
            return { {{ { -1, -1 }, { -1, 0 }, { 0, 0 }, { 1, 0 } }}, BlockStyle::J };
        }
        static Tetromino L() {
            return { {{ { -1, 0 }, { 0, 0 }, { 1, 0 }, { 1, -1 } }}, BlockStyle::L };
        }
        static Tetromino O() {
            return { {{ { -1, -1 }, { 0, -1 }, { -1, 0 }, { 0, 0 } }}, BlockStyle::O };
        }
        static Tetromino S() {
            return { {{ { -1, 0 }, { 0, 0 }, { 0, -1 }, { 1, -1 } }}, BlockStyle::S };
        }
        static Tetromino T() {
            return { {{ { -1, 0 }, { 0, 0 }, { 1, 0 }, { 0, -1 } }}, BlockStyle::T };
        }
        static Tetromino Z() {
            return { {{ { -1, -1 }, { 0, -1 }, { 0, 0 }, { 1, 0 } }}, BlockStyle::Z };
        }
        static Tetromino Undef() {
            return { {{}}, BlockStyle::Undef };
        }

        void rotate() noexcept {
            assert(style_ != BlockStyle::Undef && "Rotation of undefined block is impossible");
            rotation_ = (rotation_ + 1) % 4;
            switch (style_) {
            case BlockStyle::I: rotate_I_(); break;
            case BlockStyle::O: break;
            default: {
                for (auto&& mino : minos_) {
                    auto tmp = mino.x;
                    mino.x = -1 * mino.y;
                    mino.y = tmp;
                }
            }
            }
        }

        std::array<Mino, 4> minos() const noexcept {
            std::array<Mino, 4> global_minos = minos_;
            for (auto&& mino : global_minos) {
                mino.x += x;
                mino.y += y;
            }
            return global_minos;
        }

        BlockStyle style() const noexcept { return style_; }
        int8_t rotation() const noexcept { return rotation_; }
        bool is_undef() const noexcept { return style_ == BlockStyle::Undef; }

    private:
        MinosT minos_;
        BlockStyle style_;
        int8_t rotation_;

    private:
        Tetromino(std::array<Mino, 4> minos_arr, BlockStyle block_style) :
            x{ 0 },
            y{ 0 },
            minos_{ minos_arr },
            style_{ block_style },
            rotation_{ 0 }
        {}

        void rotate_I_() noexcept {
            /**
            *  0            1            2            3
            *  .  .  .  .   .  . [ ] .   .  .  .  .   . [ ] .  .
            * [ ][ ][ ][ ]  .  . [ ] .   .  .  .  .   . [ ] .  .
            *  .  .  .  .   .  . [ ] .  [ ][ ][ ][ ]  . [ ] .  .
            *  .  .  .  .   .  . [ ] .   .  .  .  .   . [ ] .  .
            *  y = 0        x = 0        y = 1        x = -1
            *  0/2=0        1/2=0        2/2=1        3/2=1
            *  0%2=0        1%2=1        2%2=0        3%2=1
            *  (1-0*2)*0=0  (1-1*2)*0=0  (1-0*2)*1=1  (1-1*2)*1=-1
            */
            int8_t rotation_div = rotation_ / 2;
            int8_t rotation_rem = rotation_ % 2;
            int8_t position_offset = (1 - rotation_rem * 2) * rotation_div;
            if (rotation_ % 2) {
                for (auto&& mino : minos_) {
                    mino.y = -mino.x;
                    mino.x = position_offset;
                }
                return;
            }
            for (auto&& mino : minos_) {
                mino.x = -mino.y;
                mino.y = position_offset;
            }
        }
    };

    class Tetris final {
    public:
        Tetris() :
            common_wall_kick_tests_{ {
                    {{ {0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2} }},
                    {{ {0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2} }},
                    {{ {0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2} }},
                    {{ {0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2} }}
            } },
            I_wall_kick_tests_{ {
                    {{ {0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2} }},
                    {{ {0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1} }},
                    {{ {0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2} }},
                    {{ {0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1} }}
            } },
            bucket_{ { {{false}} } },
            tetromino_{ Tetromino::Undef() },
            next_tetromino_{ Tetromino::Undef() },
            score_{ 0 },
            level_{ 1 },
            cleared_lines_{ 0 },
            last_update_{ 0 },
            game_speed_{ std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count() },
            common_game_speed_{ std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count() },
            move_direction_{ 0 },
            rotate_{ false },
            game_over_{ false },
            left_key_pressed_{ false },
            right_key_pressed_{ false },
            rotate_key_pressed_{ false }
        {
            auto now = std::chrono::system_clock::now().time_since_epoch();
            auto now_in_seconds = std::chrono::duration_cast<std::chrono::seconds>(now).count();
            std::srand(static_cast<unsigned int>(now_in_seconds));
            choose_next_tetromino_();
        }

        bool update(long long delta) {
            if (game_over_) {
                return false;
            }
            bool changed = false;
            if (tetromino_.is_undef()) {
                tetromino_ = next_tetromino_;
                tetromino_.x = 5;
                tetromino_.y = 1;
                for (auto&& mino : tetromino_.minos()) {
                    if (bucket_[mino.y][mino.x]) {
                        game_over_ = true;
                        return false;
                    }
                }
                choose_next_tetromino_();
                changed = true;
            }
            for (auto&& mino : tetromino_.minos()) {
                bucket_[mino.y][mino.x] = false;
            }
            if (rotate_) {
                changed = try_rotate_();
                rotate_ = false;
            }
            if (move_direction_) {
                changed = try_move_();
                move_direction_ = 0;
            }
            last_update_ += delta;
            bool collided = false;
            if (last_update_ >= game_speed_) {
                constexpr int ROWS = 20;
                last_update_ = 0;
                // Check for horizontal collision
                for (auto&& mino : tetromino_.minos()) {
                    collided |= mino.y + 1 >= ROWS || bucket_[mino.y + 1][mino.x];
                }
                if (!collided) {
                    tetromino_.y += 1;
                    changed = true;
                }
            }
            for (auto&& mino : tetromino_.minos()) {
                bucket_[mino.y][mino.x] = true;
            }
            if (collided) {
                changed = try_remove_full_rows_();
                tetromino_ = Tetromino::Undef();
            }
            return changed;
        }

        bool process_input() {
            bool changed = false;

            auto left_key_state = GetKeyState('7') & 0x8000;
            if (!left_key_state) {
                left_key_pressed_ = false;
            }
            else if (!left_key_pressed_ && left_key_state) {
                move_direction_ = -1;
                left_key_pressed_ = true;
                changed = true;
            }

            auto right_key_state = GetKeyState('9') & 0x8000;
            if (!right_key_state) {
                right_key_pressed_ = false;
            }
            else if (!right_key_pressed_ && right_key_state) {
                ++move_direction_ = 1;
                right_key_pressed_ = true;
                changed = true;
            }

            // Only right rotation supported due to comfy keyboard layout.
            auto up_key_state = GetKeyState('8') & 0x8000;
            if (!up_key_state) {
                rotate_key_pressed_ = false;
            }
            else if (!rotate_key_pressed_ && up_key_state) {
                rotate_ = true;
                rotate_key_pressed_ = true;
                changed = true;
            }

            auto down_key_state = GetKeyState('4') & 0x8000;
            if (down_key_state) {
                using namespace std::chrono;
                using namespace std::chrono_literals;
                auto speed_up_time = duration_cast<nanoseconds>(100ms).count();
                if (speed_up_time < common_game_speed_)
                    game_speed_ = speed_up_time;
            }
            else {
                using namespace std::chrono;
                using namespace std::chrono_literals;
                game_speed_ = common_game_speed_;
            }

            auto reset_key_state = GetKeyState(VK_SPACE) & 0x8000;
            if (reset_key_state) {
                reset_();
            }
            return changed;
        }

        const BucketT& bucket() const& { return bucket_; }
        const Tetromino& next_tetromino() const& { return next_tetromino_; }
        bool game_over() const noexcept { return game_over_; }
        size_t score() const noexcept { return score_; }
        size_t cleared_lines() const noexcept { return cleared_lines_; }
        size_t level() const noexcept { return level_; }

    private:
        using WallKickData = std::array<std::pair<int8_t, int8_t>, 5>;

    private:
        void reset_() {
            for (auto&& row : bucket_) {
                for (auto&& cell : row) {
                    cell = false;
                }
            }
            tetromino_ = Tetromino::Undef();
            choose_next_tetromino_();
            game_over_ = false;
            score_ = 0;
            level_ = 1;
            cleared_lines_ = 0;
            last_update_ = 0;
            game_speed_ = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count();
            common_game_speed_ = game_speed_;
            move_direction_ = 0;
            rotate_ = false;
            game_over_ = false;
            left_key_pressed_ = false;
            right_key_pressed_ = false;
            rotate_key_pressed_ = false;
        }

        void choose_next_tetromino_() {
            int rand_number = std::rand() % 7;
            switch (rand_number)
            {
            case 0: next_tetromino_ = Tetromino::I(); break;
            case 1: next_tetromino_ = Tetromino::J(); break;
            case 2: next_tetromino_ = Tetromino::L(); break;
            case 3: next_tetromino_ = Tetromino::O(); break;
            case 4: next_tetromino_ = Tetromino::S(); break;
            case 5: next_tetromino_ = Tetromino::T(); break;
            case 6: next_tetromino_ = Tetromino::Z(); break;
            default:
                assert("This can't happen");
            }
        }

        // SRS rotation
        bool try_rotate_() {
            assert(rotate_ && "try_rotate_ is called with rotate_ other than true");
            auto rotated_tetromino = tetromino_;
            rotated_tetromino.rotate();
            WallKickData* wall_kick_tests = &common_wall_kick_tests_[tetromino_.rotation()];
            if (tetromino_.style() == Tetromino::BlockStyle::I) {
                wall_kick_tests = &I_wall_kick_tests_[tetromino_.rotation()];
            }
            for (auto [x, y] : *wall_kick_tests) {
                bool fits = true;
                rotated_tetromino.x += x;
                rotated_tetromino.y += y;
                for (auto&& mino : rotated_tetromino.minos()) {
                    constexpr int COLS = 10;
                    constexpr int ROWS = 20;
                    auto y = mino.y;
                    auto x = mino.x;
                    if (x < 0 || x >= COLS || y < 0 || y >= ROWS || bucket_[y][x]) {
                        fits = false;
                    }
                }
                if (fits) {
                    break;
                }
                rotated_tetromino.x -= x;
                rotated_tetromino.y -= y;
            }
            tetromino_ = rotated_tetromino;
            return true;
        }

        bool try_move_() {
            assert((move_direction_ == -1 || move_direction_ == 1) && "try_move_ called with move_direction_ other than 1 or -1");
            for (auto&& mino : tetromino_.minos()) {
                constexpr int COLS = 10;
                int x_approximation = mino.x + move_direction_;
                if (x_approximation < 0 || x_approximation >= COLS || bucket_[mino.y][x_approximation]) {
                    return false;
                }
            }
            tetromino_.x += move_direction_;
            return true;
        }

        bool try_remove_full_rows_() {
            assert(!tetromino_.is_undef() && "try_remove_full_rows_ has to be called before undefing tetromino");
            constexpr int8_t DUMMY_ROW = 20;
            std::array<int8_t, 4> ys = { DUMMY_ROW, DUMMY_ROW, DUMMY_ROW, DUMMY_ROW };
            auto free_it = std::begin(ys);
            for (auto&& mino : tetromino_.minos()) {
                auto end = std::end(ys);
                auto it = std::find(std::begin(ys), end, mino.y);
                if (it == end) {
                    *free_it = mino.y;
                    ++free_it;
                }
            }
            std::sort(std::begin(ys), std::end(ys));
            int number_of_filled = 0;
            for (auto y : ys) {
                if (y == DUMMY_ROW) {
                    break;
                }
                bool filled = std::all_of(std::begin(bucket_[y]), std::end(bucket_[y]), [](auto&& cell) {
                    return cell;
                    });
                if (!filled) {
                    continue;
                }
                ++number_of_filled;
                for (auto&& cell : bucket_[y]) {
                    cell = false;
                }
                for (; y > 0; --y) {
                    std::swap(bucket_[y], bucket_[y - 1]);
                }
            }
            // Original BPS scoring system
            switch (number_of_filled) {
            case 1: score_ += 40; break;
            case 2: score_ += 100; break;
            case 3: score_ += 300; break;
            case 4: score_ += 1200; break;
            }
            auto remains_for_new_level = cleared_lines_ % 10;
            cleared_lines_ += number_of_filled;
            if (remains_for_new_level + number_of_filled >= 10) {
                ++level_;
            }
            if (level_ <= 10) {
                common_game_speed_ = common_game_speed_ * 4 / 5;
            }
            else if (level_ >= 29) {
                using namespace std::chrono;
                using namespace std::chrono_literals;
                common_game_speed_ = duration_cast<nanoseconds>(20ms).count();
            }
            return false;
        }

    private:
        // Wall kick rules for "right" rotations. "Left" rotations are not supported.
        std::array<WallKickData, 4> common_wall_kick_tests_;
        std::array<WallKickData, 4> I_wall_kick_tests_;
        BucketT bucket_;
        Tetromino tetromino_;
        Tetromino next_tetromino_;
        size_t score_;
        size_t level_;
        size_t cleared_lines_;
        long long last_update_; // in ns
        long long game_speed_; // in ns
        long long common_game_speed_; // in ns
        int move_direction_;
        bool rotate_;
        bool game_over_;
        bool left_key_pressed_;
        bool right_key_pressed_;
        bool rotate_key_pressed_;
    };

    class TerminalTetrisRenderer final {
    public:
        TerminalTetrisRenderer() :
            renderer_string_(
                //     6 8    14  18   23 26 29 32 35 38 41 44 47 50                     73
                //     | |     |   |    |  |  |  |  |  |  |  |  |  |                      |
                "LINES CLEARED:    0 <! .  .  .  .  .  .  .  .  .  . !>                   \n" // 0
                "LEVEL:            1 <! .  .  .  .  .  .  .  .  .  . !> 7: left  9: right \n" // 1
                "  SCORE:       0    <! .  .  .  .  .  .  .  .  .  . !>    8: rotate      \n" // 2
                "                    <! .  .  .  .  .  .  .  .  .  . !>   4: speed up     \n" // 3
                "                    <! .  .  .  .  .  .  .  .  .  . !>  space - reset    \n" // 4
                "                    <! .  .  .  .  .  .  .  .  .  . !>                   \n" // 5
                "                    <! .  .  .  .  .  .  .  .  .  . !>                   \n" // 6
                "                    <! .  .  .  .  .  .  .  .  .  . !>                   \n" // 7
                "                    <! .  .  .  .  .  .  .  .  .  . !>                   \n" // 8
                "                    <! .  .  .  .  .  .  .  .  .  . !>                   \n" // 9
                "                    <! .  .  .  .  .  .  .  .  .  . !>                   \n" // 10
                "                    <! .  .  .  .  .  .  .  .  .  . !>                   \n" // 11
                "                    <! .  .  .  .  .  .  .  .  .  . !>                   \n" // 12
                "                    <! .  .  .  .  .  .  .  .  .  . !>                   \n" // 13
                "                    <! .  .  .  .  .  .  .  .  .  . !>                   \n" // 14
                "                    <! .  .  .  .  .  .  .  .  .  . !>                   \n" // 15
                "                    <! .  .  .  .  .  .  .  .  .  . !>                   \n" // 16
                "                    <! .  .  .  .  .  .  .  .  .  . !>                   \n" // 17
                "                    <! .  .  .  .  .  .  .  .  .  . !>                   \n" // 18
                "                    <! .  .  .  .  .  .  .  .  .  . !>                   \n" // 19
                "                    <!==============================!>                   \n"
                "                      \\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/                   \n"
            ) {
        }

        void draw_bucket(const BucketT& bucket) noexcept {
            constexpr int ROWS = 20;
            constexpr int COLS = 10;
            constexpr int SCREEN_WIDTH = 74;
            constexpr int OUTER_WIDTH = SCREEN_WIDTH - 3 * COLS;
            constexpr int INNER_OFFSET = 22;
            auto renderer_string_it = std::begin(renderer_string_) + INNER_OFFSET;
            for (int i = 0; i < ROWS; ++i) {
                for (int j = 0; j < COLS; ++j) {
                    if (bucket[i][j]) {
                        renderer_string_.replace(renderer_string_it, renderer_string_it + 3, "[ ]");
                    }
                    else {
                        renderer_string_.replace(renderer_string_it, renderer_string_it + 3, " . ");
                    }
                    renderer_string_it += 3;
                }
                renderer_string_it += OUTER_WIDTH;
            }
        }

        void draw_next_tetromino(const Tetromino& tetromino) {
            constexpr int X_RANGE_BEGIN = 6;
            constexpr int X_RANGE_END = 18;
            constexpr int Y_RANGE_BEGIN = 11;
            constexpr int Y_RANGE_END = 13;
            constexpr int SCREEN_WIDTH = 74;
            for (int i = Y_RANGE_BEGIN; i <= Y_RANGE_END; ++i) {
                for (int j = X_RANGE_BEGIN; j <= X_RANGE_END; ++j) {
                    renderer_string_[i * SCREEN_WIDTH + j] = ' ';
                }
            }
            constexpr int CENTER_X = (X_RANGE_BEGIN + X_RANGE_END) / 2;
            constexpr int CENTER_Y = (Y_RANGE_BEGIN + Y_RANGE_END) / 2;
            constexpr int CENTER_IDX = CENTER_Y * SCREEN_WIDTH + CENTER_X;
            auto it = std::begin(renderer_string_);
            for (auto&& mino : tetromino.minos()) {
                int idx = CENTER_IDX + 3 * mino.x + SCREEN_WIDTH * mino.y;
                renderer_string_.replace(it + idx, it + idx + 3, "[ ]");
            }
        }

        void draw_score(size_t score) {
            std::stringstream score_converter;
            score_converter << std::setw(8) << std::setfill(' ') << score;
            constexpr size_t SCREEN_WIDTH = 74;
            constexpr size_t SCORE_OFFSET = 8;
            auto score_str = score_converter.str();
            assert(score_str.length() == 8 && "Score string has to be length of 8");
            auto renderer_string_it = std::begin(renderer_string_) + 2 * SCREEN_WIDTH + SCORE_OFFSET;
            std::copy(std::begin(score_str), std::end(score_str), renderer_string_it);
        }

        void draw_level(size_t level) {
            std::stringstream level_converter;
            level_converter << std::setw(4) << std::setfill(' ') << level;
            constexpr size_t SCREEN_WIDTH = 74;
            constexpr size_t LEVEL_OFFSET = 15;
            auto level_str = level_converter.str();
            assert(level_str.length() == 4 && "Score string has to be length of 4");
            auto renderer_string_it = std::begin(renderer_string_) + SCREEN_WIDTH + LEVEL_OFFSET;
            std::copy(std::begin(level_str), std::end(level_str), renderer_string_it);
        }

        void draw_cleared_lines(size_t cleared_lines) {
            std::stringstream cl_converter;
            cl_converter << std::setw(4) << std::setfill(' ') << cleared_lines;
            constexpr size_t SCREEN_WIDTH = 74;
            constexpr size_t CL_OFFSET = 15;
            auto cl_str = cl_converter.str();
            assert(cl_str.length() == 4 && "Score string has to be length of 4");
            auto renderer_string_it = std::begin(renderer_string_) + CL_OFFSET;
            std::copy(std::begin(cl_str), std::end(cl_str), renderer_string_it);
        }


        void render() const {
            std::cout << renderer_string_;
        }

    private:
        std::string renderer_string_;
    };
}

int main() {
    Tetris t;
    TerminalTetrisRenderer ttr;
    auto last_update = std::chrono::high_resolution_clock::now();
    using namespace std::chrono_literals;
    using namespace std::chrono;
    auto lag = 0ns;
    while (true) {
        auto current_update = high_resolution_clock::now();
        auto elapsed = duration_cast<nanoseconds>(current_update - last_update);
        last_update = current_update;
        lag += elapsed;

        bool changed = t.process_input();
        if (t.game_over()) {
            continue;
        }
        if (changed) {
            changed = t.update(lag.count());
            lag = 0ns;
        }
        constexpr auto NS_PER_UPDATE = duration_cast<nanoseconds>(16ms);
        while (lag > NS_PER_UPDATE) {
            changed |= t.update(NS_PER_UPDATE.count());
            lag -= NS_PER_UPDATE;
        }

        if (changed) {
            std::system("cls");
            ttr.draw_next_tetromino(t.next_tetromino());
            ttr.draw_bucket(t.bucket());
            ttr.draw_score(t.score());
            ttr.draw_cleared_lines(t.cleared_lines());
            ttr.draw_level(t.level());
            ttr.render();
        }
    }
    return 0;
}
