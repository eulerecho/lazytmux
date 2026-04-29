#include <lazytmux/version.hpp>

namespace lazytmux {

// Anchor to keep the static library non-empty until the version
// metadata grows real out-of-line state.
const std::string_view& version_anchor() noexcept {
    return kVersion;
}

}  // namespace lazytmux
