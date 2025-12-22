#pragma once

#include "wirekrak/protocol/kraken/client.hpp"
#include "wirekrak/transport/winhttp/WebSocket.hpp"


namespace wirekrak {

using WinClient = protocol::kraken::Client<transport::winhttp::WebSocket>;


} // namespace wirekrak
