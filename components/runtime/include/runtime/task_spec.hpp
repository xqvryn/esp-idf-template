#pragma once

#include <cstdint>

namespace runtime {

// Scheduling request. core -1 means unpinned. This struct does not create a task.
struct TaskSpec {
    const char* name{nullptr};
    std::uint32_t stack_bytes{0};
    unsigned priority{0};
    int core{-1};
};

// max_priority is exclusive, matching configMAX_PRIORITIES on the target.
bool task_spec_valid(const TaskSpec& spec, unsigned max_priority) noexcept;

} // namespace runtime
