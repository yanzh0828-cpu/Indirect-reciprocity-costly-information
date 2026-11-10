#include "simulation.h"

#include "rng.h"
#include "types.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr uint8_t GOOD = 0;
constexpr uint8_t BAD = 1;

// Strategy codes (see types.h): ALLD=0, ALLD_TILDE=1, DIS=2, DIS_TILDE=3.
// Returns true if the donor cooperates.
inline bool donor_cooperates(uint8_t strat, uint8_t recip_rep,
                             double q, double dq, Rng& rng) {
    switch (strat) {
        case 0: return false;  // ALLD
        case 1: return false;  // ALLD_TILDE
        default: {             // DIS or DIS_TILDE
            double recog = (strat == 2) ? q : std::min(1.0, q + dq);
            uint8_t perceived = (rng.uniform() < recog) ? recip_rep : GOOD;
            return perceived == GOOD;
        }
    }
}

} // namespace

RunResult run_one_seed(const Params& p, uint64_t seed) {
    const int N = p.N;
    const int R = p.rounds;
    const double b = p.b, c = p.c, pcost = p.p;
    const double beta = p.beta;

    Rng rng(seed);

    std::vector<uint8_t> strat(N);
    std::vector<uint8_t> rep(N, GOOD);
    std::vector<double> payoff(N, 0.0);
    std::vector<int> order(N);
    std::vector<int> counts(NUM_STRATEGIES, 0);

    // Start from a mixed population (equal shares). The initial drift to the
    // first monomorphic state is NOT a successful fixation (no mutant yet).
    for (int i = 0; i < N; ++i) {
        strat[i] = static_cast<uint8_t>((i * NUM_STRATEGIES) / N);
        ++counts[strat[i]];
    }
    for (int i = 0; i < N; ++i) order[i] = i;

    const uint64_t measure_fix = measure_fix_for(p);

    uint64_t fix_count[NUM_STRATEGIES] = {0, 0, 0, 0};
    uint64_t successful_fix = 0;
    uint8_t mutant = 0;
    bool cycling = false;  // true once the fixation->mutation cycles have begun

    for (;;) {
        // --- per-generation reset: reputation -> GOOD, payoff -> 0 ---
        std::fill(rep.begin(), rep.end(), GOOD);
        std::fill(payoff.begin(), payoff.end(), 0.0);

        // --- interaction stage: R rounds of random-pair donation ---
        for (int r = 0; r < R; ++r) {
            for (int i = N - 1; i > 0; --i) {
                int j = rng.uniform_int(i + 1);
                std::swap(order[i], order[j]);
            }
            for (int k = 0; k < N; k += 2) {
                const int a = order[k];
                const int bb = order[k + 1];
                int donor, recip;
                if (rng.uniform_int(2) == 0) { donor = a; recip = bb; }
                else                         { donor = bb; recip = a; }

                const uint8_t ds = strat[donor];
                const bool coop = donor_cooperates(ds, rep[recip], p.q, p.dq, rng);

                if (ds == 1 || ds == 3) payoff[donor] -= pcost; // ALLD_TILDE & DIS_TILDE pay p as donor
                if (coop) {
                    payoff[donor] -= c;
                    payoff[recip] += b;
                }
                rep[donor] = coop ? GOOD : BAD;
            }
        }

        // average payoff per round
        const double invR = 1.0 / static_cast<double>(R);
        for (int i = 0; i < N; ++i) payoff[i] *= invR;

        // --- strategy update: fixation-triggered mutation ---
        int fixed_type = -1;
        for (int s = 0; s < NUM_STRATEGIES; ++s) {
            if (counts[s] == N) { fixed_type = s; break; }
        }

        if (fixed_type >= 0) {
            // Monomorphic: force a single mutation to one of the other three
            // strategies, chosen uniformly.
            cycling = true;
            const uint8_t resident = static_cast<uint8_t>(fixed_type);
            mutant = static_cast<uint8_t>((resident + 1 + rng.uniform_int(NUM_STRATEGIES - 1)) % NUM_STRATEGIES);
            const int A = rng.uniform_int(N);
            --counts[resident];
            ++counts[mutant];
            strat[A] = mutant;
        } else {
            // Polymorphic: Moran-like Fermi imitation.
            const int A = rng.uniform_int(N);
            const uint8_t old_s = strat[A];
            uint8_t new_s = old_s;

            int B = rng.uniform_int(N - 1);
            if (B >= A) ++B;
            const double dpi = payoff[B] - payoff[A];
            const double prob = 1.0 / (1.0 + std::exp(-beta * dpi));
            if (rng.uniform() < prob) new_s = strat[B];

            if (new_s != old_s) {
                --counts[old_s];
                ++counts[new_s];
                strat[A] = new_s;
            }

            // Successful takeover: the introduced mutant just fixed.
            if (counts[new_s] == N && cycling && new_s == mutant) {
                ++fix_count[new_s];
                ++successful_fix;
            }
        }

        if (successful_fix >= measure_fix) break;
    }

    RunResult res;
    res.seed = seed;
    res.measure_fix = measure_fix;
    uint64_t fix_total = 0;
    for (int s = 0; s < NUM_STRATEGIES; ++s) fix_total += fix_count[s];
    res.fixations = fix_total;
    const double fix_denom = static_cast<double>(measure_fix);
    for (int s = 0; s < NUM_STRATEGIES; ++s) {
        res.fix_abund[s] = static_cast<double>(fix_count[s]) / fix_denom;
    }
    return res;
}
