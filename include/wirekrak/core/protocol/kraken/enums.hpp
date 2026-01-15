#pragma once

#include <string_view>

#include "wirekrak/core/protocol/kraken/enums/method.hpp"
#include "wirekrak/core/protocol/kraken/enums/channel.hpp"
#include "wirekrak/core/protocol/kraken/enums/side.hpp"
#include "wirekrak/core/protocol/kraken/enums/order_type.hpp"
#include "wirekrak/core/protocol/kraken/enums/system_state.hpp"
#include "wirekrak/core/protocol/kraken/enums/payload_type.hpp"


namespace wirekrak::core {
    template <typename>
    inline constexpr bool always_false = false;
}

