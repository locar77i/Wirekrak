#pragma once

#include "wirekrak/Client.hpp"
#include "wirekrak/winhttp/WebSocket.hpp"


namespace wirekrak {
namespace winhttp {

using WinClient = Client<winhttp::WebSocket>;


} // namespace winhttp
} // namespace wirekrak
