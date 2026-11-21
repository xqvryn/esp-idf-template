#include "parameters/text.hpp"

#include <cerrno>
#include <charconv>
#include <cstdlib>

namespace parameters {

namespace {

bool copy_name(std::string_view name, char (&out)[max_name_length + 1]) noexcept {
    if (name.empty() || name.size() > max_name_length) {
        return false;
    }
    for (char character : name) {
        if (character == '\0') {
            return false;
        }
    }
    name.copy(out, name.size());
    out[name.size()] = '\0';
    return true;
}

} // namespace

ParameterWrite write_text(Registry& registry, std::string_view name,
                          std::string_view text) noexcept {
    char key[max_name_length + 1] = {};
    if (!copy_name(name, key)) {
        return ParameterWrite::rejected;
    }
    bool boolean = false;
    std::int32_t integer = 0;
    float real = 0;
    const bool known_bool = registry.get_bool(key, boolean);
    const bool known_int = registry.get_int(key, integer);
    const bool known_real = registry.get_real(key, real);
    if (!known_bool && !known_int && !known_real) {
        return ParameterWrite::unknown;
    }
    if (known_bool) {
        if (text == "1" || text == "true") {
            return registry.set_bool(key, true) ? ParameterWrite::applied
                                                : ParameterWrite::rejected;
        }
        if (text == "0" || text == "false") {
            return registry.set_bool(key, false) ? ParameterWrite::applied
                                                 : ParameterWrite::rejected;
        }
        return ParameterWrite::rejected;
    }
    if (known_int) {
        if (text.empty()) {
            return ParameterWrite::rejected;
        }
        std::int32_t value = 0;
        const auto* begin = text.data();
        const auto result = std::from_chars(begin, begin + text.size(), value);
        if (result.ec != std::errc{} || result.ptr != begin + text.size()) {
            return ParameterWrite::rejected;
        }
        return registry.set_int(key, value) ? ParameterWrite::applied : ParameterWrite::rejected;
    }
    if (text.empty()) {
        return ParameterWrite::rejected;
    }
    char local[32] = {};
    if (text.size() >= sizeof(local)) {
        return ParameterWrite::rejected;
    }
    text.copy(local, text.size());
    char* end = nullptr;
    errno = 0;
    const float value = std::strtof(local, &end);
    if (end == local || end != local + text.size() || errno == ERANGE) {
        return ParameterWrite::rejected;
    }
    return registry.set_real(key, value) ? ParameterWrite::applied : ParameterWrite::rejected;
}

} // namespace parameters
