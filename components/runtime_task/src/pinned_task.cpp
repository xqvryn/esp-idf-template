#include "runtime_task/pinned_task.hpp"

namespace {

struct NopWork {
    void step() noexcept {
    }
};

} // namespace

namespace runtime_task {

bool runtime_task_linked() noexcept {
    return runtime::task_spec_valid(runtime::TaskSpec{"meter", 2048, 1, -1}, 25);
}

template class PinnedPeriodicTask<NopWork>;

} // namespace runtime_task
