#pragma once

#include <cstdint>

// Strategy underlying values are used directly as uint8_t in hot loops.
enum class Strategy : uint8_t {
    ALLC = 0,      // Always Cooperate
    ALLD = 1,      // Always Defect
    DIS = 2,       // Discriminator
    DIS_TILDE = 3  // Informative discriminator (pays p, recognition q+dq)
};

constexpr int NUM_STRATEGIES = 4;

inline const char* strategy_name(int s) {
    switch (s) {
        case 0: return "ALLC";
        case 1: return "ALLD";
        case 2: return "DIS";
        case 3: return "DIS_TILDE";
        default: return "?";
    }
}
