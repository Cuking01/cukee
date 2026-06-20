#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "log.h"
#include "role.h"
#include "state.h"

namespace cukee::simulator {

struct Player_Info {
    uint8_t rank{};
    std::string name{};
};

class Game {
public:
    explicit Game(std::array<std::unique_ptr<Role>, 4> roles);

    const log::牌谱_T* replay_log{};
    log::牌谱_T* record_log{};
    std::array<Player_Info, 4> players{};
    bool replay_mode{};
    uint64_t seed{};
    bool keep_history{};

    Simulator_State current_state;
    std::vector<Simulator_State> history_states{};

    [[nodiscard]] Role& role(uint8_t actor);
    [[nodiscard]] const Role& role(uint8_t actor) const;
    void save_history_snapshot();
    log::牌谱_Event step_once();

private:
    std::array<std::unique_ptr<Role>, 4> roles_;
};

} // namespace cukee::simulator
