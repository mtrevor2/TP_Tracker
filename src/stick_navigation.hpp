#pragma once
#include <algorithm>
#include <cstdlib>
#include <cstdint>

namespace tracker {
// One dominant direction, dead zone, and delayed repeat for an analog stick.
struct StickNavigation {
    int held = 0;
    int64_t next = 0;
    int sample(int x, int y, int64_t milliseconds) {
        int direction = 0;
        if (std::max(std::abs(x), std::abs(y)) >= 16000)
            direction = std::abs(x) >= std::abs(y) ? (x > 0 ? 1 : -1) : (y > 0 ? -2 : 2);
        if (!direction) { held = 0; return 0; }
        if (direction != held) { held = direction; next = milliseconds + 350; return direction; }
        if (milliseconds < next) return 0;
        next = milliseconds + 150;
        return direction;
    }
};
}
