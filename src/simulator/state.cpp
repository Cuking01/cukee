#include "state.h"

#include <algorithm>
#include <stdexcept>
#include <type_traits>

#include "game.h"

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

Simulator_State::Simulator_State(Game* game_) : game(game_)
{
    if (game == nullptr) {
        throw std::invalid_argument("game is null");
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
    if (game->replay_log == nullptr) {
        throw std::invalid_argument("replay log is null");
    }
    for (std::size_t i = 0; i < game->players.size(); ++i) {
        game->players[i].rank = game->replay_log->player_ranks[i];
        game->players[i].name = game->replay_log->player_names[i];
    }
}

void Simulator_State::sync_player_info_to_record_log() const
{
    if (game->record_log == nullptr) {
        return;
    }
    for (std::size_t i = 0; i < game->players.size(); ++i) {
        game->record_log->player_ranks[i] = game->players[i].rank;
        game->record_log->player_names[i] = game->players[i].name;
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
    需要摸牌 = true;
    下次摸岭上牌 = false;
    phase = Phase::小局开始前;
}

void Simulator_State::prepare_牌山(const std::array<牌_T, 136>* specified_wall)
{
    if (game->replay_mode) {
        if (game->replay_log == nullptr) {
            throw std::invalid_argument("replay log is null");
        }
        if (小局_index >= game->replay_log->小局.size()) {
            throw std::out_of_range("replay round index is out of range");
        }

        const auto& replay_wall = game->replay_log->小局[小局_index].牌山;
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
        std::mt19937_64 generator(game->seed);
        std::shuffle(牌山.牌山.begin(), 牌山.牌山.end(), generator);
    }

    牌山.front_draw_count = 0;
    牌山.back_draw_count = 0;

    if (!game->replay_mode) {
        sync_player_info_to_record_log();
    }
}

void Simulator_State::apply_event(const log::牌谱_Event& event)
{
    std::visit([this](const auto& event_data) {
        using Event_T = std::decay_t<decltype(event_data)>;

        if constexpr (std::is_same_v<Event_T, log::摸牌_Event_T>) {
            current_actor = event_data.actor;
            手牌[event_data.actor].free_tiles.push_back(event_data.牌);
            需要摸牌 = false;
            下次摸岭上牌 = false;
        } else if constexpr (std::is_same_v<Event_T, log::打牌_Event_T>) {
            牌河[event_data.actor].打牌.push_back(打牌_Record{
                event_data.牌,
                event_data.摸切,
                event_data.立直宣告,
            });
            上一张打牌 = 上一张打牌_State{
                event_data.actor,
                打牌_Record{event_data.牌, event_data.摸切, event_data.立直宣告},
                true,
            };
            current_actor = static_cast<uint8_t>((event_data.actor + 1) & 0x03);
            需要摸牌 = true;
            if (待开宝牌) {
                ++宝牌指示牌亮牌数;
                待开宝牌 = false;
            }
        } else if constexpr (std::is_same_v<Event_T, log::副露_Event_T>) {
            全场有鸣牌 = true;
            被鸣牌[event_data.target] = true;

            副露_Block block;
            block.target = event_data.target;
            block.牌[0] = event_data.牌;
            for (std::size_t i = 0; i < event_data.consumed_count; ++i) {
                block.牌[i + 1] = event_data.consumed[i];
            }
            block.牌数 = static_cast<uint8_t>(event_data.consumed_count + 1);
            if (event_data.type == log::副露类型_T::吃) {
                block.type = 副露类型_T::吃;
            } else if (event_data.type == log::副露类型_T::碰) {
                block.type = 副露类型_T::碰;
            } else {
                block.type = 副露类型_T::明杠;
                下次摸岭上牌 = true;
                待开宝牌 = true;
            }
            手牌[event_data.actor].副露.push_back(block);
            current_actor = event_data.actor;
            需要摸牌 = false;
            上一张打牌.valid = false;
        } else if constexpr (std::is_same_v<Event_T, log::暗杠_Event_T>) {
            副露_Block block;
            block.type = 副露类型_T::暗杠;
            block.牌.fill(event_data.牌);
            block.牌数 = 4;
            block.target = event_data.actor;
            手牌[event_data.actor].副露.push_back(block);
            current_actor = event_data.actor;
            需要摸牌 = true;
            下次摸岭上牌 = true;
            ++宝牌指示牌亮牌数;
            上一张打牌.valid = false;
        } else if constexpr (std::is_same_v<Event_T, log::加杠_Event_T>) {
            副露_Block block;
            block.type = 副露类型_T::加杠;
            block.牌.fill(event_data.牌);
            block.牌数 = 4;
            block.target = event_data.actor;
            手牌[event_data.actor].副露.push_back(block);
            current_actor = event_data.actor;
            需要摸牌 = true;
            下次摸岭上牌 = true;
            待开宝牌 = true;
            上一张打牌.valid = false;
        } else if constexpr (std::is_same_v<Event_T, log::和牌_Event_T>) {
            phase = Phase::小局开始前;
            上一张打牌.valid = false;
        } else if constexpr (std::is_same_v<Event_T, log::流局_Event_T>) {
            phase = Phase::小局开始前;
            上一张打牌.valid = false;
        }
    }, event.data);
}

void Simulator_State::record_event(const log::牌谱_Event& event)
{
    if (game->record_log == nullptr || game->replay_mode || 小局_index >= game->record_log->小局.size()) {
        return;
    }
    game->record_log->小局[小局_index].events.push_back(event);
}

} // namespace cukee::simulator
