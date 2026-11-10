#pragma once

#include "params.h"
#include "types.h"
#include <cstdint>

struct RunResult {
    double mu;
    uint64_t seed;
    uint64_t burn_in;
    uint64_t measure;
    double abund[NUM_STRATEGIES]; // time-averaged abundance in [0, 1]
};

// Run one full evolutionary simulation (one seed) for the given mutation rate.
RunResult run_one_seed(const Params& p, double mu, uint64_t seed);
