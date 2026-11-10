#pragma once

#include "params.h"
#include "types.h"
#include <cstdint>

struct RunResult {
    uint64_t seed;
    uint64_t measure_fix;    // successful fixations requested
    uint64_t fixations;      // successful fixations observed
    double fix_abund[NUM_STRATEGIES]; // fraction of successful fixations
};

// Run one fixation->mutation simulation (one seed). Starts from a mixed
// population and counts successful fixations from the first takeover onward.
RunResult run_one_seed(const Params& p, uint64_t seed);
