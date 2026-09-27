#pragma once

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <type_traits>

namespace banana {

/// Type-safe bit set over an enum class whose enumerators are bit *indices* (0, 1, 2, ...).
template <typename E>
    requires std::is_enum_v<E>
class Flags {
public:
    using Mask = std::uint32_t;

    constexpr Flags() = default;
    constexpr Flags(std::initializer_list<E> flags)
    {
        for (E flag : flags) {
            set(flag);
        }
    }

    constexpr Flags& set(E flag)
    {
        mask_ |= bit(flag);
        return *this;
    }
    constexpr Flags& reset(E flag)
    {
        mask_ &= ~bit(flag);
        return *this;
    }
    constexpr void clear() { mask_ = 0; }

    [[nodiscard]] constexpr bool test(E flag) const { return (mask_ & bit(flag)) != 0; }
    [[nodiscard]] constexpr bool any() const { return mask_ != 0; }
    [[nodiscard]] constexpr bool none() const { return mask_ == 0; }
    /// True if `flag` is set and nothing else is.
    [[nodiscard]] constexpr bool only(E flag) const { return mask_ == bit(flag); }
    [[nodiscard]] constexpr int count() const { return std::popcount(mask_); }
    [[nodiscard]] constexpr Mask raw() const { return mask_; }

    constexpr Flags& operator|=(Flags other)
    {
        mask_ |= other.mask_;
        return *this;
    }
    constexpr Flags& operator&=(Flags other)
    {
        mask_ &= other.mask_;
        return *this;
    }
    [[nodiscard]] friend constexpr Flags operator|(Flags lhs, Flags rhs) { return lhs |= rhs; }
    [[nodiscard]] friend constexpr Flags operator&(Flags lhs, Flags rhs) { return lhs &= rhs; }
    [[nodiscard]] friend constexpr bool operator==(Flags, Flags) = default;

private:
    [[nodiscard]] static constexpr Mask bit(E flag)
    {
        return Mask{1} << static_cast<std::underlying_type_t<E>>(flag);
    }

    Mask mask_ = 0;
};

} // namespace banana
