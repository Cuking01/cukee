#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <vector>

#include "log.h"
#include "tile.h"

namespace cukee::simulator {

class Game;

enum class Phase {
    小局开始前,
    小局进行中,
    半庄结束,
};

enum class 立直_State {
    未立直,
    宣告立直,
    立直成功,
};

enum class 副露类型_T : uint8_t {
    吃,
    碰,
    明杠,
    暗杠,
    加杠,
};

struct 副露_Block {
    副露类型_T type{};
    std::array<牌_T, 4> 牌{};
    uint8_t 牌数{};
    uint8_t target{};
};

struct 手牌_State {
    std::vector<牌_T> free_tiles{};
    std::vector<副露_Block> 副露{};
};

struct 打牌_Record {
    牌_T 牌{};
    bool 摸切{};
    bool 立直宣告{};
};

struct 牌河_State {
    std::vector<打牌_Record> 打牌{};
};

struct 上一张打牌_State {
    uint8_t actor{};
    打牌_Record record{};
    bool valid{};
};

struct 牌山_State {
    std::array<牌_T, 136> 牌山{};
    uint8_t front_draw_count{};
    uint8_t back_draw_count{};

    [[nodiscard]] std::size_t front_draw_index() const;
    [[nodiscard]] std::size_t back_draw_index() const;
    [[nodiscard]] const 牌_T& front_draw_tile() const;
    [[nodiscard]] const 牌_T& back_draw_tile() const;
    void draw_front();
    void draw_back();
};

struct 包牌_State {
    std::array<std::optional<uint8_t>, 4> 大三元{};
    std::array<std::optional<uint8_t>, 4> 大四喜{};
};

struct Simulator_State {
    explicit Simulator_State(Game* game);

    Game* game{};
    std::size_t 小局_index{};

    uint8_t 场风{};
    uint8_t 局编号{};
    uint8_t 亲家{};
    牌山_State 牌山{};
    std::array<手牌_State, 4> 手牌{};
    std::array<牌河_State, 4> 牌河{};
    std::array<int32_t, 4> points{};
    uint8_t 本场数{};
    uint8_t 立直棒数{};
    std::array<立直_State, 4> 立直{};
    std::array<bool, 4> 一发{};
    std::array<bool, 4> 双立直{};
    std::array<bool, 4> 振听{};
    上一张打牌_State 上一张打牌{};
    包牌_State 包牌{};
    uint8_t 宝牌指示牌亮牌数{};
    std::array<uint8_t, 34> 宝牌倍率{};
    bool 待开宝牌{};
    bool 全场有鸣牌{};
    std::array<bool, 4> 被鸣牌{};
    uint8_t current_actor{};
    bool 需要摸牌{true};
    bool 下次摸岭上牌{};
    Phase phase{Phase::小局开始前};

    void load_player_info_from_replay_log();
    void sync_player_info_to_record_log() const;
    void reset_小局_state();
    void prepare_牌山(const std::array<牌_T, 136>* specified_wall = nullptr);
    void apply_event(const log::牌谱_Event& event);
    void record_event(const log::牌谱_Event& event);
};

} // namespace cukee::simulator
