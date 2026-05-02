#include <lazytmux/frecency.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numbers>
#include <utility>

namespace lazytmux::frecency {

Index::Index(std::map<tmux::SessionName, Entry> entries) : entries_(std::move(entries)) {}

void Index::visit(const tmux::SessionName& name, std::int64_t now) {
    auto& e = entries_[name];
    if (e.visits != std::numeric_limits<std::uint32_t>::max()) {
        e.visits = e.visits + 1U;
    }
    e.last_used = now;
}

double Index::score(const tmux::SessionName& name, std::int64_t now) const {
    const auto it = entries_.find(name);
    if (it == entries_.end() || it->second.visits == 0) {
        return 0.0;
    }
    const std::int64_t raw_age = now - it->second.last_used;
    const auto clamped = std::max<std::int64_t>(0, raw_age);
    const double age = static_cast<double>(clamped);
    const double lambda = std::numbers::ln2 / kHalfLifeSecs;
    return static_cast<double>(it->second.visits) * std::exp(-lambda * age);
}

const std::map<tmux::SessionName, Entry>& Index::entries() const noexcept {
    return entries_;
}

std::int64_t unix_now() {
    using std::chrono::duration_cast;
    using std::chrono::seconds;
    using std::chrono::system_clock;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

}  // namespace lazytmux::frecency
