#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

#include "tile.h"

namespace cukee::log {

enum class 副露类型_T : uint8_t {
    吃,
    碰,
    明杠,
};

enum class 吃类型_T : uint8_t {
    上吃,
    中吃,
    下吃,
};

enum class 流局类型_T : uint8_t {
    荒牌,
    九种九牌,
    四风连打,
    四杠散了,
    四家立直,
};

struct 摸牌_Event_T {
    uint8_t actor{};
    牌_T 牌{};

    [[nodiscard]] std::size_t save_size() const
    {
        return 0;
    }

    void save(uint8_t*) const
    {
    }
};

struct 打牌_Event_T {
    uint8_t actor{};
    牌_T 牌{};
    bool 摸切{};
    bool 立直宣告{};

    [[nodiscard]] std::size_t save_size() const
    {
        return 1;
    }

    void save(uint8_t* p) const
    {
        p[0] = 牌.to_37_index();
        if (立直宣告) {
            p[0] |= 0x40;
        }
        if (摸切) {
            p[0] |= 0x80;
        }
    }
};

struct 副露_Event_T {
    uint8_t actor{};
    uint8_t target{};
    副露类型_T type{};
    吃类型_T 吃类型{};
    牌_T 牌{};
    std::array<牌_T, 3> consumed{};
    uint8_t consumed_count{};

    [[nodiscard]] std::size_t save_size() const
    {
        return 1;
    }

    void save(uint8_t* p) const
    {
        bool consumed_has_red = false;
        for (std::size_t i = 0; i < consumed_count && i < consumed.size(); ++i) {
            consumed_has_red = consumed_has_red || consumed[i].红宝牌;
        }

        p[0] = static_cast<uint8_t>(actor & 0x03);
        p[0] |= static_cast<uint8_t>((static_cast<uint8_t>(type) & 0x03) << 2);
        p[0] |= static_cast<uint8_t>((static_cast<uint8_t>(吃类型) & 0x03) << 4);
        if (consumed_has_red) {
            p[0] |= 0x40;
        }
    }
};

struct 暗杠_Event_T {
    uint8_t actor{};
    牌_T 牌{};
    bool 摸杠{};

    [[nodiscard]] std::size_t save_size() const
    {
        return 1;
    }

    void save(uint8_t* p) const
    {
        p[0] = 牌.to_37_index();
        if (摸杠) {
            p[0] |= 0x40;
        }
    }
};

struct 加杠_Event_T {
    uint8_t actor{};
    牌_T 牌{};
    bool 摸杠{};

    [[nodiscard]] std::size_t save_size() const
    {
        return 1;
    }

    void save(uint8_t* p) const
    {
        p[0] = 牌.to_37_index();
        if (摸杠) {
            p[0] |= 0x40;
        }
    }
};

struct 和牌_Event_T {
    std::array<bool, 4> 和{};
    bool 自摸{};

    [[nodiscard]] std::size_t save_size() const
    {
        return 1;
    }

    void save(uint8_t* p) const
    {
        p[0] = 0;
        for (std::size_t i = 0; i < 和.size(); ++i) {
            if (和[i]) {
                p[0] |= static_cast<uint8_t>(1u << i);
            }
        }
        if (自摸) {
            p[0] |= 0x10;
        }
    }
};

struct 流局_Event_T {
    流局类型_T type{};

    [[nodiscard]] std::size_t save_size() const
    {
        return 1;
    }

    void save(uint8_t* p) const
    {
        p[0] = static_cast<uint8_t>(type);
    }
};

struct 牌谱_Event {
    using Variant = std::variant<
        摸牌_Event_T,
        打牌_Event_T,
        副露_Event_T,
        暗杠_Event_T,
        加杠_Event_T,
        和牌_Event_T,
        流局_Event_T>;

    Variant data{};

    牌谱_Event() = default;

    template <typename Event_T>
    explicit 牌谱_Event(Event_T event) : data(std::move(event))
    {
    }

    [[nodiscard]] bool is_摸牌() const
    {
        return std::holds_alternative<摸牌_Event_T>(data);
    }

    [[nodiscard]] uint8_t encode_type() const
    {
        if (std::holds_alternative<打牌_Event_T>(data)) {
            return 0;
        }
        if (std::holds_alternative<副露_Event_T>(data)) {
            return 1;
        }
        if (std::holds_alternative<暗杠_Event_T>(data)) {
            return 2;
        }
        if (std::holds_alternative<加杠_Event_T>(data)) {
            return 3;
        }
        if (std::holds_alternative<和牌_Event_T>(data)) {
            return 4;
        }
        if (std::holds_alternative<流局_Event_T>(data)) {
            return 5;
        }
        throw std::invalid_argument("draw event has no saved type encoding");
    }

    [[nodiscard]] std::size_t save_size() const
    {
        return std::visit([](const auto& event) { return event.save_size(); }, data);
    }

    void save(uint8_t* p) const
    {
        std::visit([p](const auto& event) { event.save(p); }, data);
    }
};

struct 小局_T {
    uint8_t 场风{};
    uint8_t 局编号{};
    uint8_t 本场数{};
    uint8_t 立直棒数{};
    uint8_t 亲家{};
    std::array<int32_t, 4> 起始点数{};

    std::array<牌_T, 136> 牌山{};
    std::vector<牌谱_Event> events{};

    [[nodiscard]] std::size_t save_size() const
    {
        constexpr std::size_t fixed_size = 5 + 4 * 4 + 136 + 2;
        std::size_t saved_event_count = 0;
        std::size_t payload_size = 0;
        for (const auto& event : events) {
            if (event.is_摸牌()) {
                continue;
            }
            ++saved_event_count;
            payload_size += event.save_size();
        }
        const std::size_t type_size = (saved_event_count + 1) / 2;
        return fixed_size + type_size + payload_size;
    }

    void save(uint8_t* p) const
    {
        uint8_t* cursor = p;
        *cursor++ = 场风;
        *cursor++ = 局编号;
        *cursor++ = 本场数;
        *cursor++ = 立直棒数;
        *cursor++ = 亲家;

        for (const auto score : 起始点数) {
            const auto value = static_cast<uint32_t>(score);
            cursor[0] = static_cast<uint8_t>(value & 0xff);
            cursor[1] = static_cast<uint8_t>((value >> 8) & 0xff);
            cursor[2] = static_cast<uint8_t>((value >> 16) & 0xff);
            cursor[3] = static_cast<uint8_t>((value >> 24) & 0xff);
            cursor += 4;
        }

        for (const auto& 牌 : 牌山) {
            *cursor++ = 牌.to_37_index();
        }

        std::size_t saved_event_count = 0;
        for (const auto& event : events) {
            saved_event_count += event.is_摸牌() ? 0 : 1;
        }

        if (saved_event_count > 0xffff) {
            throw std::invalid_argument("too many events");
        }
        const auto event_count = static_cast<uint16_t>(saved_event_count);
        cursor[0] = static_cast<uint8_t>(event_count & 0xff);
        cursor[1] = static_cast<uint8_t>((event_count >> 8) & 0xff);
        cursor += 2;

        const std::size_t type_size = (saved_event_count + 1) / 2;
        for (std::size_t i = 0; i < type_size; ++i) {
            cursor[i] = 0;
        }
        std::size_t saved_index = 0;
        for (const auto& event : events) {
            if (event.is_摸牌()) {
                continue;
            }
            const auto type = static_cast<uint8_t>(event.encode_type() & 0x0f);
            if ((saved_index & 1) == 0) {
                cursor[saved_index / 2] |= type;
            } else {
                cursor[saved_index / 2] |= static_cast<uint8_t>(type << 4);
            }
            ++saved_index;
        }
        cursor += type_size;

        for (const auto& event : events) {
            if (event.is_摸牌()) {
                continue;
            }
            event.save(cursor);
            cursor += event.save_size();
        }
    }

    const uint8_t* load(const uint8_t* p)
    {
        const uint8_t* cursor = p;
        场风 = *cursor++;
        局编号 = *cursor++;
        本场数 = *cursor++;
        立直棒数 = *cursor++;
        亲家 = *cursor++;

        for (auto& score : 起始点数) {
            const uint32_t value = static_cast<uint32_t>(cursor[0])
                | (static_cast<uint32_t>(cursor[1]) << 8)
                | (static_cast<uint32_t>(cursor[2]) << 16)
                | (static_cast<uint32_t>(cursor[3]) << 24);
            score = static_cast<int32_t>(value);
            cursor += 4;
        }

        for (auto& 牌 : 牌山) {
            牌 = 牌_T::from_37_index(*cursor++ & 0x3f);
        }

        const uint16_t event_count = static_cast<uint16_t>(cursor[0])
            | static_cast<uint16_t>(cursor[1] << 8);
        cursor += 2;

        const uint8_t* type_cursor = cursor;
        cursor += (event_count + 1) / 2;

        events.clear();
        uint8_t current_actor = 亲家;
        bool need_draw = true;
        bool next_draw_is_kan = false;
        std::size_t normal_draw_index = 52;
        std::size_t kan_draw_index = 135;
        bool has_previous_discard = false;
        uint8_t previous_discard_actor = 0;
        牌_T previous_discard{};

        auto push_draw = [&]() {
            if (!need_draw) {
                return;
            }
            if (next_draw_is_kan) {
                if (kan_draw_index < normal_draw_index) {
                    throw std::invalid_argument("invalid kan draw index");
                }
                events.emplace_back(摸牌_Event_T{current_actor, 牌山[kan_draw_index--]});
            } else {
                if (normal_draw_index > kan_draw_index) {
                    throw std::invalid_argument("invalid normal draw index");
                }
                events.emplace_back(摸牌_Event_T{current_actor, 牌山[normal_draw_index++]});
            }
            need_draw = false;
            next_draw_is_kan = false;
        };

        auto make_same_tile = [](牌_T base, bool red) {
            base.红宝牌 = red && base.门 != 'z' && base.编号 == 4;
            return base;
        };

        auto fill_chi_consumed = [](副露_Event_T& event, bool red) {
            const auto called = event.牌;
            auto set_tile = [&](std::size_t index, uint8_t 编号) {
                event.consumed[index] = 牌_T{called.门, 编号, false};
            };

            if (called.门 == 'z') {
                throw std::invalid_argument("honor tile cannot be chi");
            }

            switch (event.吃类型) {
            case 吃类型_T::上吃:
                if (called.编号 < 2) {
                    throw std::invalid_argument("invalid upper chi");
                }
                set_tile(0, static_cast<uint8_t>(called.编号 - 2));
                set_tile(1, static_cast<uint8_t>(called.编号 - 1));
                break;
            case 吃类型_T::中吃:
                if (called.编号 < 1 || called.编号 > 7) {
                    throw std::invalid_argument("invalid middle chi");
                }
                set_tile(0, static_cast<uint8_t>(called.编号 - 1));
                set_tile(1, static_cast<uint8_t>(called.编号 + 1));
                break;
            case 吃类型_T::下吃:
                if (called.编号 > 6) {
                    throw std::invalid_argument("invalid lower chi");
                }
                set_tile(0, static_cast<uint8_t>(called.编号 + 1));
                set_tile(1, static_cast<uint8_t>(called.编号 + 2));
                break;
            }

            if (red) {
                for (auto& 牌 : event.consumed) {
                    if (牌.门 != 'z' && 牌.编号 == 4) {
                        牌.红宝牌 = true;
                        break;
                    }
                }
            }
        };

        for (uint16_t i = 0; i < event_count; ++i) {
            const uint8_t type = (i & 1) == 0
                ? static_cast<uint8_t>(type_cursor[i / 2] & 0x0f)
                : static_cast<uint8_t>((type_cursor[i / 2] >> 4) & 0x0f);

            switch (type) {
            case 0: {
                push_draw();
                const uint8_t byte = *cursor++;
                打牌_Event_T event;
                event.actor = current_actor;
                event.牌 = 牌_T::from_37_index(byte & 0x3f);
                event.立直宣告 = (byte & 0x40) != 0;
                event.摸切 = (byte & 0x80) != 0;
                events.emplace_back(event);
                previous_discard_actor = event.actor;
                previous_discard = event.牌;
                has_previous_discard = true;
                current_actor = static_cast<uint8_t>((event.actor + 1) & 0x03);
                need_draw = true;
                next_draw_is_kan = false;
                break;
            }
            case 1: {
                if (!has_previous_discard) {
                    throw std::invalid_argument("call without previous discard");
                }
                const uint8_t byte = *cursor++;
                副露_Event_T event;
                event.actor = static_cast<uint8_t>(byte & 0x03);
                event.target = previous_discard_actor;
                event.type = static_cast<副露类型_T>((byte >> 2) & 0x03);
                event.吃类型 = static_cast<吃类型_T>((byte >> 4) & 0x03);
                event.牌 = previous_discard;
                const bool consumed_has_red = (byte & 0x40) != 0;

                if (event.type == 副露类型_T::吃) {
                    event.consumed_count = 2;
                    fill_chi_consumed(event, consumed_has_red);
                } else if (event.type == 副露类型_T::碰) {
                    event.consumed_count = 2;
                    event.consumed[0] = make_same_tile(event.牌, consumed_has_red);
                    event.consumed[1] = make_same_tile(event.牌, false);
                } else if (event.type == 副露类型_T::明杠) {
                    event.consumed_count = 3;
                    event.consumed[0] = make_same_tile(event.牌, consumed_has_red);
                    event.consumed[1] = make_same_tile(event.牌, false);
                    event.consumed[2] = make_same_tile(event.牌, false);
                } else {
                    throw std::invalid_argument("invalid call type encoding");
                }

                events.emplace_back(event);
                current_actor = event.actor;
                need_draw = false;
                next_draw_is_kan = false;
                has_previous_discard = false;
                break;
            }
            case 2: {
                push_draw();
                const uint8_t byte = *cursor++;
                暗杠_Event_T event;
                event.actor = current_actor;
                event.牌 = 牌_T::from_37_index(byte & 0x3f);
                event.摸杠 = (byte & 0x40) != 0;
                events.emplace_back(event);
                need_draw = true;
                next_draw_is_kan = true;
                has_previous_discard = false;
                break;
            }
            case 3: {
                push_draw();
                const uint8_t byte = *cursor++;
                加杠_Event_T event;
                event.actor = current_actor;
                event.牌 = 牌_T::from_37_index(byte & 0x3f);
                event.摸杠 = (byte & 0x40) != 0;
                events.emplace_back(event);
                need_draw = true;
                next_draw_is_kan = true;
                has_previous_discard = false;
                break;
            }
            case 4: {
                const uint8_t byte = *cursor++;
                和牌_Event_T event;
                for (std::size_t j = 0; j < event.和.size(); ++j) {
                    event.和[j] = (byte & (1u << j)) != 0;
                }
                event.自摸 = (byte & 0x10) != 0;
                if (event.自摸) {
                    push_draw();
                }
                events.emplace_back(event);
                break;
            }
            case 5: {
                const uint8_t byte = *cursor++;
                流局_Event_T event;
                event.type = static_cast<流局类型_T>(byte & 0x0f);
                if (event.type == 流局类型_T::九种九牌) {
                    push_draw();
                }
                events.emplace_back(event);
                break;
            }
            default:
                throw std::invalid_argument("invalid event type");
            }
        }

        return cursor;
    }
};

struct 牌谱_T {
    std::vector<小局_T> 小局{};

    [[nodiscard]] std::size_t save_size() const
    {
        std::size_t size = 2;
        for (const auto& round : 小局) {
            size += round.save_size();
        }
        return size;
    }

    void save(uint8_t* p) const
    {
        if (小局.size() > 0xffff) {
            throw std::invalid_argument("too many rounds");
        }

        uint8_t* cursor = p;
        const auto round_count = static_cast<uint16_t>(小局.size());
        cursor[0] = static_cast<uint8_t>(round_count & 0xff);
        cursor[1] = static_cast<uint8_t>((round_count >> 8) & 0xff);
        cursor += 2;

        for (const auto& round : 小局) {
            round.save(cursor);
            cursor += round.save_size();
        }
    }

    const uint8_t* load(const uint8_t* p)
    {
        const uint8_t* cursor = p;
        const uint16_t round_count = static_cast<uint16_t>(cursor[0])
            | static_cast<uint16_t>(cursor[1] << 8);
        cursor += 2;

        小局.clear();
        小局.resize(round_count);
        for (auto& round : 小局) {
            cursor = round.load(cursor);
        }

        return cursor;
    }
};

} // namespace cukee::log
