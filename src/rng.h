#pragma once

#include <cstdint>

// SplitMix64: deterministic, high-quality seed mixing.
inline uint64_t splitmix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

// xoshiro256++ PRNG (Vigna & Blackman). Fast and high quality; good choice
// for Monte Carlo. State is seeded from a 64-bit value via splitmix64.
class Rng {
public:
    explicit Rng(uint64_t seed) {
        uint64_t x = seed;
        for (int i = 0; i < 4; ++i) {
            x = splitmix64(x);
            s_[i] = x;
        }
    }

    inline uint64_t next_u64() {
        const uint64_t result = rotl(s_[0] + s_[3], 23) + s_[0];
        const uint64_t t = s_[1] << 17;
        s_[2] ^= s_[0];
        s_[3] ^= s_[1];
        s_[1] ^= s_[2];
        s_[0] ^= s_[3];
        s_[2] ^= t;
        s_[3] = rotl(s_[3], 45);
        return result;
    }

    // uniform double in [0, 1) using the top 53 bits
    inline double uniform() {
        return static_cast<double>(next_u64() >> 11) * (1.0 / 9007199254740992.0);
    }

    // uniform integer in [0, n). n is small; modulo bias is negligible.
    inline int uniform_int(int n) {
        return static_cast<int>(next_u64() % static_cast<uint64_t>(n));
    }

private:
    static inline uint64_t rotl(uint64_t x, int k) {
        return (x << k) | (x >> (64 - k));
    }

    uint64_t s_[4];
};
