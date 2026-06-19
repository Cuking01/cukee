#pragma once

#include <cstdint>
#include <stdexcept>

namespace cukee {

struct 牌_T {
    char 门{};
    uint8_t 编号{};
    bool 红宝牌{};

    [[nodiscard]] uint8_t to_37_index() const
    {
        if (红宝牌) {
            if (门 == 'm' && 编号 == 4) {
                return 34;
            }
            if (门 == 'p' && 编号 == 4) {
                return 35;
            }
            if (门 == 's' && 编号 == 4) {
                return 36;
            }
            throw std::invalid_argument("invalid red tile");
        }

        if (门 == 'm' && 编号 < 9) {
            return 编号;
        }
        if (门 == 'p' && 编号 < 9) {
            return static_cast<uint8_t>(9 + 编号);
        }
        if (门 == 's' && 编号 < 9) {
            return static_cast<uint8_t>(18 + 编号);
        }
        if (门 == 'z' && 编号 < 7) {
            return static_cast<uint8_t>(27 + 编号);
        }

        throw std::invalid_argument("invalid tile");
    }

    static 牌_T from_37_index(uint8_t index)
    {
        if (index < 9) {
            return 牌_T{'m', index, false};
        }
        if (index < 18) {
            return 牌_T{'p', static_cast<uint8_t>(index - 9), false};
        }
        if (index < 27) {
            return 牌_T{'s', static_cast<uint8_t>(index - 18), false};
        }
        if (index < 34) {
            return 牌_T{'z', static_cast<uint8_t>(index - 27), false};
        }
        if (index == 34) {
            return 牌_T{'m', 4, true};
        }
        if (index == 35) {
            return 牌_T{'p', 4, true};
        }
        if (index == 36) {
            return 牌_T{'s', 4, true};
        }
        throw std::invalid_argument("invalid tile index");
    }
};

} // namespace cukee
