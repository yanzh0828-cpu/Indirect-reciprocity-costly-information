#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

struct Params {
    // model parameters
    int N = 100;
    double b = 5.0;
    double c = 1.0;
    double q = 0.3;
    double dq = 0.3;      // Delta q
    double p = 0.1;       // information cost for DIS_TILDE
    double beta = 0.001;  // selection intensity
    double w = 0.9;       // informational only (rounds are fixed below)
    int rounds = 50;      // T: fixed rounds per generation

    // 13 log-spaced mutation rates from 1e-4 to 1 (4 points per decade)
    std::vector<double> mus = {
        1e-4, 2.154434690031884e-4, 4.641588833612779e-4,
        1e-3, 2.154434690031884e-3, 4.641588833612779e-3,
        1e-2, 2.154434690031884e-2, 4.641588833612779e-2,
        1e-1, 2.154434690031884e-1, 4.641588833612779e-1,
        1.0
    };
    int seeds_per_mu = 64;

    bool quick = false;   // tiny burn_in/measure for a fast smoke test

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

// Burn-in and measurement windows scale with the mutation timescale tau = 1/mu.
inline uint64_t burn_in_for(const Params& p, double mu) {
    if (p.quick) return 1000;
    uint64_t tau = static_cast<uint64_t>(std::llround(1.0 / mu));
    return std::max<uint64_t>(10000, 50 * tau);
}

inline uint64_t measure_for(const Params& p, double mu) {
    if (p.quick) return 10000;
    // ~30 independent fixation events per seed (30 * N / mu) so that SE < 0.01
    uint64_t tau = static_cast<uint64_t>(std::llround(1.0 / mu));
    return std::max<uint64_t>(100000, 30 * static_cast<uint64_t>(p.N) * tau);
}
