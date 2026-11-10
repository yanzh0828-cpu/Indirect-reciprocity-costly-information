#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

struct Params {
    // model parameters
    int N = 100;
    double b = 5.0;
    double c = 1.0;
    double q = 0.3;
    double dq = 0.3;      // Delta q
    double p = 0.1;       // information cost paid by DIS_TILDE and ALLD_TILDE
    double beta = 0.0003; // selection intensity
    double w = 0.9;       // informational only (rounds are fixed below)
    int rounds = 50;      // T: fixed rounds per generation

    // "fixation -> mutation" scheme (no mutation-rate parameter). Only the
    // successful-fixation outcome is measured (fixation fraction). There is no
    // burn-in: the successful-takeover jump chain mixes in ~1 step.
    uint64_t measure_fixations = 1200;  // successful fixations to measure
    int seeds = 64;

    bool quick = false;   // tiny measure for a fast smoke test

    // MySQL
    std::string db_host = "127.0.0.1";
    int db_port = 3306;
    std::string db_user = "ir_user";
    std::string db_pass;
    std::string db_name = "indirect_reciprocity";
    bool use_db = true;
    bool use_csv = true;

    std::string csv_dir = "output";
    uint64_t base_seed = 0; // 0 => derive from std::random_device
};

inline uint64_t measure_fix_for(const Params& p) {
    return p.quick ? 2 : p.measure_fixations;
}
