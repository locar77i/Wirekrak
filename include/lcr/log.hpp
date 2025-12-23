#pragma once

#include <iostream>

namespace lcr {

#ifdef DEBUG
#define DBG(x) do { std::cout << x << "\n"; } while(0)
#define FS_WRN(x) do { std::cerr << "[!!]: " << x << "\n"; } while(0)
#define FS_ERR(x) do { std::cerr << "[ERROR]: " << x << "\n"; } while(0)
#else
#define DBG(x) do {} while(0)
#define FS_WRN(x) do {} while(0)
#define FS_ERR(x) do {} while(0)
#endif

} // namespace lcr
