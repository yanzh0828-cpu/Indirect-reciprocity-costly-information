#pragma once

#include <cstdint>

// Environment 2 strategy set: {ALLD_1, ALLD_2, DIS, DIS~}.
// ALLD_1 and ALLD_2 behave identically to ALLD (always defect); they are
// distinguished only as two labels. Underlying values are used as uint8_t.
enum class Strategy : uint8_t {
    ALLD_1 = 0,    // Always Defect (copy 1)
    ALLD_2 = 1,    // Always Defect (copy 2, identical behaviour)
    DIS = 2,       // Discriminator
    DIS_TILDE = 3  // Informative discriminator (pays p, recognition q+dq)
};

constexpr int NUM_STRATEGIES = 4;

inline const char* strategy_name(int s) {
    switch (s) {
        case 0: return "ALLD_1";
        case 1: return "ALLD_2";
        case 2: return "DIS";
        case 3: return "DIS_TILDE";
        default: return "?";
    }
}
