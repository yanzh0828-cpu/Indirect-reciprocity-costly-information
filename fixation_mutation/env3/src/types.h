#pragma once

#include <cstdint>

// Environment 3 strategy set: {DIS, DIS~, ALLD, ALLD~}.
// ALLD~ always defects like ALLD but pays the information cost p as donor.
enum class Strategy : uint8_t {
    ALLD = 0,        // Always Defect
    ALLD_TILDE = 1,  // Always Defect, pays p as donor
    DIS = 2,         // Discriminator
    DIS_TILDE = 3    // Informative discriminator (pays p, recognition q+dq)
};

constexpr int NUM_STRATEGIES = 4;

inline const char* strategy_name(int s) {
    switch (s) {
        case 0: return "ALLD";
        case 1: return "ALLD_TILDE";
        case 2: return "DIS";
        case 3: return "DIS_TILDE";
        default: return "?";
    }
}
