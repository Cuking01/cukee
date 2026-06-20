#pragma once

#include <array>
#include <variant>

#include "log.h"
#include "tile.h"

namespace cukee::simulator {

class Game;

struct 打牌_Action {
    牌_T 牌{};
    bool 摸切{};
    bool 立直宣告{};
};

struct 暗杠_Action {
    牌_T 牌{};
    bool 摸杠{};
};

struct 加杠_Action {
    牌_T 牌{};
    bool 摸杠{};
};

struct 自摸_Action {
};

struct 九种九牌_Action {
};

using 摸牌后_Action = std::variant<
    打牌_Action,
    暗杠_Action,
    加杠_Action,
    自摸_Action,
    九种九牌_Action>;

struct 跳过_Action {
};

struct 吃_Action {
    log::吃类型_T 吃类型{};
    std::array<牌_T, 2> consumed{};
};

struct 碰_Action {
    std::array<牌_T, 2> consumed{};
};

struct 明杠_Action {
    std::array<牌_T, 3> consumed{};
};

struct 荣和_Action {
};

using 换序_Action = std::variant<
    跳过_Action,
    吃_Action,
    碰_Action,
    明杠_Action,
    荣和_Action>;

class Role {
public:
    virtual ~Role() = default;

    void set_game(Game* game)
    {
        game_ = game;
    }

    [[nodiscard]] Game& game() const
    {
        return *game_;
    }

    virtual 摸牌后_Action decide_摸牌后() = 0;
    virtual 换序_Action decide_换序(uint8_t 打牌_actor, const log::打牌_Event_T& event) = 0;
    virtual void on_event(const log::牌谱_Event&)
    {
    }

private:
    Game* game_{};
};

} // namespace cukee::simulator
