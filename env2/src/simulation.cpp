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

// Strategy codes (see types.h): ALLD_1=0, ALLD_2=1, DIS=2, DIS_TILDE=3.
// Returns true if the donor cooperates.
inline bool donor_cooperates(uint8_t strat, uint8_t recip_rep,
                             double q, double dq, Rng& rng) {
    switch (strat) {
        case 0: return false;  // ALLD_1
        case 1: return false;  // ALLD_2
        default: {             // DIS or DIS_TILDE
            double recog = (strat == 2) ? q : std::min(1.0, q + dq);
            uint8_t perceived = (rng.uniform() < recog) ? recip_rep : GOOD;
            return perceived == GOOD;
        }
    }
}

} // namespace

RunResult run_one_seed(const Params& p, double mu, uint64_t seed) {
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

    // initial composition: equal shares of the four strategies
    for (int i = 0; i < N; ++i) {
        strat[i] = static_cast<uint8_t>((i * NUM_STRATEGIES) / N);
        ++counts[strat[i]];
    }
    for (int i = 0; i < N; ++i) order[i] = i;

    const uint64_t burn_in = burn_in_for(p, mu);
    const uint64_t measure = measure_for(p, mu);
    const uint64_t total_gen = burn_in + measure;
    // Deterministic mutation: mutate once every mut_interval = 1/mu strategy
    // updates (mirrors how the interaction rounds are fixed rather than
    // geometric, cf. w).
    const uint64_t mut_interval =
        std::max<uint64_t>(1, static_cast<uint64_t>(std::llround(1.0 / mu)));

    uint64_t abund_accum[NUM_STRATEGIES] = {0, 0, 0, 0};

    for (uint64_t gen = 0; gen < total_gen; ++gen) {
        // --- per-generation reset: reputation -> GOOD, payoff -> 0 ---
        std::fill(rep.begin(), rep.end(), GOOD);
        std::fill(payoff.begin(), payoff.end(), 0.0);

        // --- interaction stage: R rounds of random-pair donation ---
        for (int r = 0; r < R; ++r) {
            // Fisher-Yates shuffle -> random perfect matching
            for (int i = N - 1; i > 0; --i) {
                int j = rng.uniform_int(i + 1);
                std::swap(order[i], order[j]);
            }
            for (int k = 0; k < N; k += 2) {
                const int a = order[k];
                const int bb = order[k + 1];
                // random role assignment within the pair (fair coin)
                int donor, recip;
                if (rng.uniform_int(2) == 0) { donor = a; recip = bb; }
                else                         { donor = bb; recip = a; }

                const uint8_t ds = strat[donor];
                const bool coop = donor_cooperates(ds, rep[recip], p.q, p.dq, rng);

                if (ds == 3) payoff[donor] -= pcost; // DIS_TILDE pays p whenever it is donor
                if (coop) {
                    payoff[donor] -= c;
                    payoff[recip] += b;
                }
                // Image Scoring: cooperation -> Good, defection -> Bad
                rep[donor] = coop ? GOOD : BAD;
            }
        }

        // average payoff per round
        const double invR = 1.0 / static_cast<double>(R);
        for (int i = 0; i < N; ++i) payoff[i] *= invR;

        // --- strategy update (Moran-like) ---
        const int A = rng.uniform_int(N);
        const uint8_t old_s = strat[A];
        uint8_t new_s = old_s;

        if (gen % mut_interval == 0) {
            // deterministic mutation: once every 1/mu strategy updates
            new_s = static_cast<uint8_t>((old_s + 1 + rng.uniform_int(3)) % NUM_STRATEGIES);
        } else {
            int B = rng.uniform_int(N - 1);
            if (B >= A) ++B; // B in [0, N) \ {A}
            const double dpi = payoff[B] - payoff[A]; // individual average payoffs
            const double prob = 1.0 / (1.0 + std::exp(-beta * dpi));
            if (rng.uniform() < prob) new_s = strat[B];
        }

        if (new_s != old_s) {
            --counts[old_s];
            ++counts[new_s];
            strat[A] = new_s;
        }

        // --- accumulate abundance in the measurement window ---
        if (gen >= burn_in) {
            for (int s = 0; s < NUM_STRATEGIES; ++s) abund_accum[s] += counts[s];
        }
    }

    RunResult res;
    res.mu = mu;
    res.seed = seed;
    res.burn_in = burn_in;
    res.measure = measure;
    const double denom = static_cast<double>(measure) * static_cast<double>(N);
    for (int s = 0; s < NUM_STRATEGIES; ++s) {
        res.abund[s] = static_cast<double>(abund_accum[s]) / denom;
    }
    return res;
}
