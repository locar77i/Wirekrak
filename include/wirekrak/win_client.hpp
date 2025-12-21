#pragma once

#include "wirekrak/client.hpp"
#include "wirekrak/transport/winhttp/WebSocket.hpp"


namespace wirekrak {

using WinClient = Client<transport::winhttp::WebSocket>;


} // namespace wirekrak
