#include "state.h"

#include <algorithm>
#include <stdexcept>

namespace cukee::simulator {

namespace {

bool same_牌(const 牌_T& lhs, const 牌_T& rhs)
{
    return lhs.门 == rhs.门 && lhs.编号 == rhs.编号 && lhs.红宝牌 == rhs.红宝牌;
}

std::array<牌_T, 136> make_ordered_牌山()
{
    std::array<牌_T, 136> wall{};
    std::size_t index = 0;

    for (const char 门 : {'m', 'p', 's'}) {
        for (uint8_t 编号 = 0; 编号 < 9; ++编号) {
            for (uint8_t copy = 0; copy < 4; ++copy) {
                wall[index++] = 牌_T{门, 编号, 编号 == 4 && copy == 0};
            }
        }
    }

    for (uint8_t 编号 = 0; 编号 < 7; ++编号) {
        for (uint8_t copy = 0; copy < 4; ++copy) {
            wall[index++] = 牌_T{'z', 编号, false};
        }
    }

    return wall;
}

} // namespace

Simulator_State::Simulator_State(std::array<Role*, 4> roles_) : roles(roles_)
{
    for (const auto role : roles) {
        if (role == nullptr) {
            throw std::invalid_argument("role is null");
        }
    }
}

std::size_t 牌山_State::front_draw_index() const
{
    if (front_draw_count + back_draw_count >= 136) {
        throw std::out_of_range("wall is empty");
    }
    return front_draw_count;
}

std::size_t 牌山_State::back_draw_index() const
{
    if (front_draw_count + back_draw_count >= 136) {
        throw std::out_of_range("wall is empty");
    }

    const std::size_t pair_index = back_draw_count / 2;
    const std::size_t offset_in_pair = back_draw_count % 2;
    const std::size_t index = 136 - 2 * (pair_index + 1) + offset_in_pair;
    if (index < front_draw_count) {
        throw std::out_of_range("wall is empty");
    }
    return index;
}

const 牌_T& 牌山_State::front_draw_tile() const
{
    return 牌山[front_draw_index()];
}

const 牌_T& 牌山_State::back_draw_tile() const
{
    return 牌山[back_draw_index()];
}

void 牌山_State::draw_front()
{
    (void)front_draw_index();
    ++front_draw_count;
}

void 牌山_State::draw_back()
{
    (void)back_draw_index();
    ++back_draw_count;
}

void Simulator_State::load_player_info_from_replay_log()
{
    if (replay_log == nullptr) {
        throw std::invalid_argument("replay log is null");
    }
    for (std::size_t i = 0; i < players.size(); ++i) {
        players[i].rank = replay_log->player_ranks[i];
        players[i].name = replay_log->player_names[i];
    }
}

void Simulator_State::sync_player_info_to_record_log() const
{
    if (record_log == nullptr) {
        return;
    }
    for (std::size_t i = 0; i < players.size(); ++i) {
        record_log->player_ranks[i] = players[i].rank;
        record_log->player_names[i] = players[i].name;
    }
}

void Simulator_State::reset_小局_state()
{
    for (auto& hand : 手牌) {
        hand.free_tiles.clear();
        hand.副露.clear();
    }
    for (auto& river : 牌河) {
        river.打牌.clear();
    }

    立直.fill(立直_State::未立直);
    一发.fill(false);
    双立直.fill(false);
    振听.fill(false);
    上一张打牌 = 上一张打牌_State{};
    包牌 = 包牌_State{};
    宝牌指示牌亮牌数 = 0;
    宝牌倍率.fill(0);
    待开宝牌 = false;
    全场有鸣牌 = false;
    被鸣牌.fill(false);
    current_actor = 0;
    phase = Phase::小局开始前;
}

void Simulator_State::prepare_牌山(const std::array<牌_T, 136>* specified_wall)
{
    if (replay_mode) {
        if (replay_log == nullptr) {
            throw std::invalid_argument("replay log is null");
        }
        if (小局_index >= replay_log->小局.size()) {
            throw std::out_of_range("replay round index is out of range");
        }

        const auto& replay_wall = replay_log->小局[小局_index].牌山;
        if (specified_wall != nullptr) {
            for (std::size_t i = 0; i < replay_wall.size(); ++i) {
                if (!same_牌((*specified_wall)[i], replay_wall[i])) {
                    throw std::invalid_argument("specified wall conflicts with replay log");
                }
            }
        }
        牌山.牌山 = replay_wall;
    } else if (specified_wall != nullptr) {
        牌山.牌山 = *specified_wall;
    } else {
        牌山.牌山 = make_ordered_牌山();
        std::mt19937_64 generator(seed);
        std::shuffle(牌山.牌山.begin(), 牌山.牌山.end(), generator);
    }

    牌山.front_draw_count = 0;
    牌山.back_draw_count = 0;

    if (!replay_mode) {
        sync_player_info_to_record_log();
    }
}

} // namespace cukee::simulator
