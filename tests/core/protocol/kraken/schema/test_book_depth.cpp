#include <cassert>

#include "wirekrak/core/protocol/kraken/schema/book/common.hpp"

using wirekrak::core::protocol::kraken::schema::book::is_valid_depth;

int main() {
    assert(is_valid_depth(10));
    assert(is_valid_depth(25));
    assert(is_valid_depth(100));
    assert(is_valid_depth(500));
    assert(is_valid_depth(1000));

    assert(!is_valid_depth(0));
    assert(!is_valid_depth(1));
    assert(!is_valid_depth(50));
    assert(!is_valid_depth(999));
    assert(!is_valid_depth(10000));

    return 0;
}
