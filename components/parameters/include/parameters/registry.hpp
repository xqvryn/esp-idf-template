#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace parameters {

inline constexpr std::size_t max_parameters = 16;
inline constexpr std::size_t max_name_length = 15;
inline constexpr std::size_t max_blob_bytes = 256;
inline constexpr std::uint8_t blob_schema = 1;

enum class ValueKind : std::uint8_t {
    boolean,
    integer,
    real
};

// In-memory parameter table. Names are case-sensitive. set_* rejects a value
// outside the declared range and leaves the stored value unchanged.
class Registry {
    public:
    bool define_bool(const char* name, bool initial) noexcept;
    bool define_int(const char* name, std::int32_t initial, std::int32_t min,
                    std::int32_t max) noexcept;
    bool define_real(const char* name, float initial, float min, float max) noexcept;

    bool set_bool(const char* name, bool value) noexcept;
    bool set_int(const char* name, std::int32_t value) noexcept;
    bool set_real(const char* name, float value) noexcept;

    bool get_bool(const char* name, bool& value) const noexcept;
    bool get_int(const char* name, std::int32_t& value) const noexcept;
    bool get_real(const char* name, float& value) const noexcept;

    std::size_t size() const noexcept {
        return count_;
    }

    // Little-endian schema 1 blob. Returns 0 when the buffer is too small.
    std::size_t encode(std::span<std::uint8_t> out) const noexcept;
    // Applies every record or none. Unknown names and out-of-range values fail.
    bool decode(std::span<const std::uint8_t> bytes) noexcept;

    private:
    struct Slot {
        char name[max_name_length + 1]{};
        ValueKind kind{ValueKind::boolean};
        bool boolean{false};
        std::int32_t integer{0};
        std::int32_t min_int{0};
        std::int32_t max_int{0};
        float real{0};
        float min_real{0};
        float max_real{0};
    };

    const Slot* find(const char* name) const noexcept;
    Slot* find(const char* name) noexcept;
    bool claim(const char* name, Slot*& slot) noexcept;

    Slot slots_[max_parameters]{};
    std::size_t count_{0};
};

} // namespace parameters
