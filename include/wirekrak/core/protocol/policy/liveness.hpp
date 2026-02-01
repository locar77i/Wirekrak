#pragma once


namespace wirekrak::core::protocol::policy {

enum class Liveness {
    Passive,  // liveness reflects observable protocol traffic only
    Active    // protocol is responsible for maintaining liveness
};

} // namespace wirekrak::core::protocol::policy
