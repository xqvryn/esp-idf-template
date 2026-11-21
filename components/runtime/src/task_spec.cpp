#include "runtime/task_spec.hpp"

#include <cstring>

namespace runtime {

bool task_spec_valid(const TaskSpec& spec, unsigned max_priority) noexcept {
    if (spec.name == nullptr || spec.name[0] == '\0' || std::strlen(spec.name) > 15) {
        return false;
    }
    if (spec.stack_bytes < 2048 || spec.stack_bytes > 65536) {
        return false;
    }
    if (max_priority == 0 || spec.priority >= max_priority) {
        return false;
    }
    return spec.core == -1 || spec.core == 0 || spec.core == 1;
}

} // namespace runtime
