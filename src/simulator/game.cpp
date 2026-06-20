#include "game.h"

#include <optional>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace cukee::simulator {

namespace {

log::牌谱_Event make_event(uint8_t actor, const 摸牌后_Action& action)
{
    return std::visit([actor](const auto& action_data) -> log::牌谱_Event {
        using Action_T = std::decay_t<decltype(action_data)>;

        if constexpr (std::is_same_v<Action_T, 打牌_Action>) {
            return log::牌谱_Event(log::打牌_Event_T{
                actor,
                action_data.牌,
                action_data.摸切,
                action_data.立直宣告,
            });
        } else if constexpr (std::is_same_v<Action_T, 暗杠_Action>) {
            return log::牌谱_Event(log::暗杠_Event_T{
                actor,
                action_data.牌,
                action_data.摸杠,
            });
        } else if constexpr (std::is_same_v<Action_T, 加杠_Action>) {
            return log::牌谱_Event(log::加杠_Event_T{
                actor,
                action_data.牌,
                action_data.摸杠,
            });
        } else if constexpr (std::is_same_v<Action_T, 自摸_Action>) {
            log::和牌_Event_T event;
            event.和[actor] = true;
            event.自摸 = true;
            return log::牌谱_Event(event);
        } else {
            return log::牌谱_Event(log::流局_Event_T{log::流局类型_T::九种九牌});
        }
    }, action);
}

log::牌谱_Event make_event(uint8_t actor, uint8_t target, const log::打牌_Event_T& 打牌_event, const 换序_Action& action)
{
    return std::visit([actor, target, &打牌_event](const auto& action_data) -> log::牌谱_Event {
        using Action_T = std::decay_t<decltype(action_data)>;

        if constexpr (std::is_same_v<Action_T, 吃_Action>) {
            log::副露_Event_T event;
            event.actor = actor;
            event.target = target;
            event.type = log::副露类型_T::吃;
            event.吃类型 = action_data.吃类型;
            event.牌 = 打牌_event.牌;
            event.consumed[0] = action_data.consumed[0];
            event.consumed[1] = action_data.consumed[1];
            event.consumed_count = 2;
            return log::牌谱_Event(event);
        } else if constexpr (std::is_same_v<Action_T, 碰_Action>) {
            log::副露_Event_T event;
            event.actor = actor;
            event.target = target;
            event.type = log::副露类型_T::碰;
            event.牌 = 打牌_event.牌;
            event.consumed[0] = action_data.consumed[0];
            event.consumed[1] = action_data.consumed[1];
            event.consumed_count = 2;
            return log::牌谱_Event(event);
        } else if constexpr (std::is_same_v<Action_T, 明杠_Action>) {
            log::副露_Event_T event;
            event.actor = actor;
            event.target = target;
            event.type = log::副露类型_T::明杠;
            event.牌 = 打牌_event.牌;
            event.consumed = action_data.consumed;
            event.consumed_count = 3;
            return log::牌谱_Event(event);
        } else if constexpr (std::is_same_v<Action_T, 荣和_Action>) {
            log::和牌_Event_T event;
            event.和[actor] = true;
            event.自摸 = false;
            return log::牌谱_Event(event);
        } else {
            throw std::invalid_argument("skip action cannot be converted to event");
        }
    }, action);
}

bool is_跳过(const 换序_Action& action)
{
    return std::holds_alternative<跳过_Action>(action);
}

} // namespace

Game::Game(std::array<std::unique_ptr<Role>, 4> roles)
    : current_state(this),
      roles_(std::move(roles))
{
    for (auto& role : roles_) {
        if (role == nullptr) {
            throw std::invalid_argument("role is null");
        }
        role->set_game(this);
    }
}

Role& Game::role(uint8_t actor)
{
    if (actor >= roles_.size()) {
        throw std::out_of_range("invalid actor");
    }
    return *roles_[actor];
}

const Role& Game::role(uint8_t actor) const
{
    if (actor >= roles_.size()) {
        throw std::out_of_range("invalid actor");
    }
    return *roles_[actor];
}

void Game::save_history_snapshot()
{
    if (keep_history) {
        history_states.push_back(current_state);
    }
}

log::牌谱_Event Game::step_once()
{
    save_history_snapshot();

    log::牌谱_Event event;
    if (current_state.需要摸牌) {
        const auto actor = current_state.current_actor;
        const auto& 牌 = current_state.下次摸岭上牌
            ? current_state.牌山.back_draw_tile()
            : current_state.牌山.front_draw_tile();
        event = log::牌谱_Event(log::摸牌_Event_T{actor, 牌});
        if (current_state.下次摸岭上牌) {
            current_state.牌山.draw_back();
        } else {
            current_state.牌山.draw_front();
        }
    } else if (current_state.上一张打牌.valid) {
        const auto 打牌_actor = current_state.上一张打牌.actor;
        const auto 打牌_event = log::打牌_Event_T{
            打牌_actor,
            current_state.上一张打牌.record.牌,
            current_state.上一张打牌.record.摸切,
            current_state.上一张打牌.record.立直宣告,
        };

        std::optional<log::牌谱_Event> response_event;
        for (uint8_t offset = 1; offset < 4; ++offset) {
            const auto actor = static_cast<uint8_t>((打牌_actor + offset) & 0x03);
            auto action = role(actor).decide_换序(打牌_actor, 打牌_event);
            if (!is_跳过(action)) {
                response_event = make_event(actor, 打牌_actor, 打牌_event, action);
                break;
            }
        }

        if (response_event.has_value()) {
            event = *response_event;
        } else {
            current_state.上一张打牌.valid = false;
            const auto actor = current_state.current_actor;
            const auto& 牌 = current_state.牌山.front_draw_tile();
            event = log::牌谱_Event(log::摸牌_Event_T{actor, 牌});
            current_state.牌山.draw_front();
        }
    } else {
        const auto actor = current_state.current_actor;
        event = make_event(actor, role(actor).decide_摸牌后());
    }

    current_state.apply_event(event);
    current_state.record_event(event);
    for (auto& role : roles_) {
        role->on_event(event);
    }
    return event;
}

} // namespace cukee::simulator
