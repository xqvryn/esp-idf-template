#include "parameters/registry.hpp"

#include <cmath>
#include <cstring>

namespace parameters {

namespace {

void store_u32_le(std::uint8_t* out, std::uint32_t value) noexcept {
    out[0] = static_cast<std::uint8_t>(value);
    out[1] = static_cast<std::uint8_t>(value >> 8);
    out[2] = static_cast<std::uint8_t>(value >> 16);
    out[3] = static_cast<std::uint8_t>(value >> 24);
}

bool read_u32_le(std::span<const std::uint8_t> bytes, std::size_t& cursor,
                 std::uint32_t& value) noexcept {
    if (cursor > bytes.size() || bytes.size() - cursor < 4) {
        return false;
    }
    value = static_cast<std::uint32_t>(bytes[cursor]) |
            (static_cast<std::uint32_t>(bytes[cursor + 1]) << 8) |
            (static_cast<std::uint32_t>(bytes[cursor + 2]) << 16) |
            (static_cast<std::uint32_t>(bytes[cursor + 3]) << 24);
    cursor += 4;
    return true;
}

bool name_ok(const char* name) noexcept {
    if (name == nullptr || name[0] == '\0') {
        return false;
    }
    return std::strlen(name) <= max_name_length;
}

bool in_real_range(float value, float min, float max) noexcept {
    return std::isfinite(value) && std::isfinite(min) && std::isfinite(max) && min <= max &&
           value >= min && value <= max;
}

} // namespace

bool Registry::claim(const char* name, Slot*& slot) noexcept {
    if (!name_ok(name) || find(name) != nullptr || count_ >= max_parameters) {
        return false;
    }
    slot = &slots_[count_];
    std::strncpy(slot->name, name, max_name_length);
    slot->name[max_name_length] = '\0';
    ++count_;
    return true;
}

const Registry::Slot* Registry::find(const char* name) const noexcept {
    if (name == nullptr) {
        return nullptr;
    }
    for (std::size_t index = 0; index < count_; ++index) {
        if (std::strcmp(slots_[index].name, name) == 0) {
            return &slots_[index];
        }
    }
    return nullptr;
}

Registry::Slot* Registry::find(const char* name) noexcept {
    return const_cast<Slot*>(static_cast<const Registry*>(this)->find(name));
}

bool Registry::define_bool(const char* name, bool initial) noexcept {
    Slot* slot = nullptr;
    if (!claim(name, slot)) {
        return false;
    }
    slot->kind = ValueKind::boolean;
    slot->boolean = initial;
    return true;
}

bool Registry::define_int(const char* name, std::int32_t initial, std::int32_t min,
                          std::int32_t max) noexcept {
    if (min > max || initial < min || initial > max) {
        return false;
    }
    Slot* slot = nullptr;
    if (!claim(name, slot)) {
        return false;
    }
    slot->kind = ValueKind::integer;
    slot->integer = initial;
    slot->min_int = min;
    slot->max_int = max;
    return true;
}

bool Registry::define_real(const char* name, float initial, float min, float max) noexcept {
    if (!in_real_range(initial, min, max)) {
        return false;
    }
    Slot* slot = nullptr;
    if (!claim(name, slot)) {
        return false;
    }
    slot->kind = ValueKind::real;
    slot->real = initial;
    slot->min_real = min;
    slot->max_real = max;
    return true;
}

bool Registry::set_bool(const char* name, bool value) noexcept {
    Slot* slot = find(name);
    if (slot == nullptr || slot->kind != ValueKind::boolean) {
        return false;
    }
    slot->boolean = value;
    return true;
}

bool Registry::set_int(const char* name, std::int32_t value) noexcept {
    Slot* slot = find(name);
    if (slot == nullptr || slot->kind != ValueKind::integer || value < slot->min_int ||
        value > slot->max_int) {
        return false;
    }
    slot->integer = value;
    return true;
}

bool Registry::set_real(const char* name, float value) noexcept {
    Slot* slot = find(name);
    if (slot == nullptr || slot->kind != ValueKind::real ||
        !in_real_range(value, slot->min_real, slot->max_real)) {
        return false;
    }
    slot->real = value;
    return true;
}

bool Registry::get_bool(const char* name, bool& value) const noexcept {
    const Slot* slot = find(name);
    if (slot == nullptr || slot->kind != ValueKind::boolean) {
        return false;
    }
    value = slot->boolean;
    return true;
}

bool Registry::get_int(const char* name, std::int32_t& value) const noexcept {
    const Slot* slot = find(name);
    if (slot == nullptr || slot->kind != ValueKind::integer) {
        return false;
    }
    value = slot->integer;
    return true;
}

bool Registry::get_real(const char* name, float& value) const noexcept {
    const Slot* slot = find(name);
    if (slot == nullptr || slot->kind != ValueKind::real) {
        return false;
    }
    value = slot->real;
    return true;
}

std::size_t Registry::encode(std::span<std::uint8_t> out) const noexcept {
    std::size_t need = 2;
    for (std::size_t index = 0; index < count_; ++index) {
        const auto name_len = std::strlen(slots_[index].name);
        const std::size_t payload = slots_[index].kind == ValueKind::boolean ? 1 : 4;
        need += 1 + name_len + 1 + payload;
    }
    if (need > out.size() || need > max_blob_bytes) {
        return 0;
    }
    out[0] = blob_schema;
    out[1] = static_cast<std::uint8_t>(count_);
    std::size_t cursor = 2;
    for (std::size_t index = 0; index < count_; ++index) {
        const auto name_len = static_cast<std::uint8_t>(std::strlen(slots_[index].name));
        out[cursor++] = name_len;
        std::memcpy(out.data() + cursor, slots_[index].name, name_len);
        cursor += name_len;
        out[cursor++] = static_cast<std::uint8_t>(slots_[index].kind);
        if (slots_[index].kind == ValueKind::boolean) {
            out[cursor++] = slots_[index].boolean ? 1 : 0;
        } else if (slots_[index].kind == ValueKind::integer) {
            store_u32_le(out.data() + cursor, static_cast<std::uint32_t>(slots_[index].integer));
            cursor += 4;
        } else {
            std::uint32_t bits = 0;
            static_assert(sizeof(float) == 4);
            std::memcpy(&bits, &slots_[index].real, sizeof(bits));
            store_u32_le(out.data() + cursor, bits);
            cursor += 4;
        }
    }
    return cursor;
}

bool Registry::decode(std::span<const std::uint8_t> bytes) noexcept {
    if (bytes.size() < 2 || bytes[0] != blob_schema) {
        return false;
    }
    const auto records = bytes[1];
    if (records > count_) {
        return false;
    }
    Registry staged = *this;
    std::size_t cursor = 2;
    for (std::uint8_t record = 0; record < records; ++record) {
        if (cursor >= bytes.size()) {
            return false;
        }
        const auto name_len = bytes[cursor++];
        if (name_len == 0 || name_len > max_name_length || cursor > bytes.size() ||
            bytes.size() - cursor < static_cast<std::size_t>(name_len) + 1) {
            return false;
        }
        char name[max_name_length + 1] = {};
        std::memcpy(name, bytes.data() + cursor, name_len);
        cursor += name_len;
        const auto kind = bytes[cursor++];
        Slot* slot = staged.find(name);
        if (slot == nullptr || static_cast<std::uint8_t>(slot->kind) != kind) {
            return false;
        }
        if (slot->kind == ValueKind::boolean) {
            if (cursor >= bytes.size() || (bytes[cursor] != 0 && bytes[cursor] != 1)) {
                return false;
            }
            slot->boolean = bytes[cursor++] != 0;
        } else if (slot->kind == ValueKind::integer) {
            std::uint32_t raw = 0;
            if (!read_u32_le(bytes, cursor, raw)) {
                return false;
            }
            const auto value = static_cast<std::int32_t>(raw);
            if (value < slot->min_int || value > slot->max_int) {
                return false;
            }
            slot->integer = value;
        } else {
            std::uint32_t raw = 0;
            if (!read_u32_le(bytes, cursor, raw)) {
                return false;
            }
            float value = 0;
            std::memcpy(&value, &raw, sizeof(value));
            if (!in_real_range(value, slot->min_real, slot->max_real)) {
                return false;
            }
            slot->real = value;
        }
    }
    if (cursor != bytes.size()) {
        return false;
    }
    *this = staged;
    return true;
}

} // namespace parameters
