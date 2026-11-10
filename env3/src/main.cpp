#include "db.h"
#include "params.h"
#include "rng.h"
#include "simulation.h"
#include "types.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include <sys/stat.h>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {

void print_usage(const char* argv0) {
    std::printf(
        "Usage: %s [options]\n"
        "Options:\n"
        "  --seeds <n>       seeds per mu (default 64)\n"
        "  --rounds <n>      rounds per generation T (default 50)\n"
        "  --beta <v>        selection intensity beta (default 0.001)\n"
        "  --threads <n>     OpenMP thread count (default: all)\n"
        "  --base-seed <n>   base RNG seed (default: random_device)\n"
        "  --quick           tiny burn_in/measure for a fast smoke test\n"
        "  --no-db           do not write to MySQL\n"
        "  --no-csv          do not write CSV files\n"
        "  -h, --help        show this help\n",
        argv0);
}

} // namespace

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0); // unbuffered so progress shows in logs

    Params p;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) { std::fprintf(stderr, "missing value for %s\n", name); std::exit(2); }
            return argv[++i];
        };
        if (a == "--seeds") p.seeds_per_mu = std::atoi(next("--seeds"));
        else if (a == "--rounds") p.rounds = std::atoi(next("--rounds"));
        else if (a == "--beta") p.beta = std::atof(next("--beta"));
        else if (a == "--threads") {
#ifdef _OPENMP
            omp_set_num_threads(std::atoi(next("--threads")));
#else
            (void)next("--threads");
#endif
        }
        else if (a == "--base-seed") p.base_seed = std::strtoull(next("--base-seed"), nullptr, 10);
        else if (a == "--mu-first") {
            int n = std::atoi(next("--mu-first"));
            if (n >= 1 && n <= static_cast<int>(p.mus.size())) p.mus.resize(n);
            else { std::fprintf(stderr, "--mu-first must be in [1, %zu]\n", p.mus.size()); std::exit(2); }
        }
        else if (a == "--mu-skip") {
            int k = std::atoi(next("--mu-skip"));
            if (k >= 0 && k < static_cast<int>(p.mus.size())) p.mus.erase(p.mus.begin(), p.mus.begin() + k);
            else { std::fprintf(stderr, "--mu-skip must be in [0, %zu)\n", p.mus.size()); std::exit(2); }
        }
        else if (a == "--quick") p.quick = true;
        else if (a == "--no-db") p.use_db = false;
        else if (a == "--no-csv") p.use_csv = false;
        else if (a == "-h" || a == "--help") { print_usage(argv[0]); return 0; }
        else { std::fprintf(stderr, "unknown option: %s\n", a.c_str()); print_usage(argv[0]); return 2; }
    }

    uint64_t base_seed = p.base_seed;
    if (base_seed == 0) {
        std::random_device rd;
        base_seed = (static_cast<uint64_t>(rd()) << 32) ^ rd();
        if (base_seed == 0) base_seed = 0x9e3779b97f4a7c15ULL;
    }

    const int n_mu = static_cast<int>(p.mus.size());
    const int n_seed = p.seeds_per_mu;
    const long n_tasks = static_cast<long>(n_mu) * n_seed;

    std::vector<RunResult> results(n_tasks);

    std::printf("N=%d b=%g c=%g q=%g dq=%g p=%g beta=%g rounds=%d seeds_per_mu=%d quick=%d\n",
                p.N, p.b, p.c, p.q, p.dq, p.p, p.beta, p.rounds, n_seed, p.quick ? 1 : 0);
    std::printf("base_seed=%llu  tasks=%ld\n",
                static_cast<unsigned long long>(base_seed), n_tasks);
#ifdef _OPENMP
    std::printf("threads=%d\n", omp_get_max_threads());
#endif

    auto t0 = std::chrono::steady_clock::now();

    std::printf("running %ld tasks (%d mu x %d seeds) ...\n", n_tasks, n_mu, n_seed);
    std::fflush(stdout);

    std::atomic<long> done{0};
    const long report_step = std::max<long>(1, n_tasks / 20);

#pragma omp parallel for schedule(dynamic, 1)
    for (long t = 0; t < n_tasks; ++t) {
        const int m = static_cast<int>(t / n_seed);
        const int s = static_cast<int>(t % n_seed);
        const uint64_t seed = splitmix64(
            base_seed
            ^ (static_cast<uint64_t>(m + 1) * 0x9e3779b97f4a7c15ULL)
            ^ (static_cast<uint64_t>(s + 1) * 0xbf58476d1ce4e5b9ULL));
        results[t] = run_one_seed(p, p.mus[m], seed);

        const long d = done.fetch_add(1, std::memory_order_relaxed) + 1;
        if (d % report_step == 0 || d == n_tasks) {
#pragma omp critical(progress)
            {
                std::printf("progress: %ld/%ld tasks done (%.1f%%)\n",
                            d, n_tasks, 100.0 * static_cast<double>(d) / n_tasks);
                std::fflush(stdout);
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    std::printf("simulation wall time: %.2f s\n", std::chrono::duration<double>(t1 - t0).count());

    // --- aggregate per mu (mean and standard error across seeds) ---
    struct Agg { int n; double mean[NUM_STRATEGIES]; double se[NUM_STRATEGIES]; };
    std::vector<Agg> aggs(n_mu);
    for (int m = 0; m < n_mu; ++m) {
        double sum[NUM_STRATEGIES] = {0, 0, 0, 0};
        double sum2[NUM_STRATEGIES] = {0, 0, 0, 0};
        for (int s = 0; s < n_seed; ++s) {
            const RunResult& r = results[static_cast<long>(m) * n_seed + s];
            for (int k = 0; k < NUM_STRATEGIES; ++k) {
                sum[k] += r.abund[k];
                sum2[k] += r.abund[k] * r.abund[k];
            }
        }
        aggs[m].n = n_seed;
        for (int k = 0; k < NUM_STRATEGIES; ++k) {
            const double mean = sum[k] / n_seed;
            const double var = (sum2[k] - sum[k] * sum[k] / n_seed) / (n_seed - 1);
            aggs[m].mean[k] = mean;
            aggs[m].se[k] = std::sqrt(var / n_seed);
        }
    }

    std::printf("\n== summary (mean abundance +- SE) ==\n");
    for (int m = 0; m < n_mu; ++m) {
        std::printf("mu=%-8g seeds=%d\n", p.mus[m], aggs[m].n);
        std::printf("  mean: ALLD=%.6f ALLD~=%.6f DIS=%.6f DIS~=%.6f\n",
                    aggs[m].mean[0], aggs[m].mean[1], aggs[m].mean[2], aggs[m].mean[3]);
        std::printf("  se  : ALLD=%.6f ALLD~=%.6f DIS=%.6f DIS~=%.6f\n",
                    aggs[m].se[0], aggs[m].se[1], aggs[m].se[2], aggs[m].se[3]);
    }

    // --- CSV output ---
    if (p.use_csv) {
        mkdir(p.csv_dir.c_str(), 0755);
        {
            std::ofstream f(p.csv_dir + "/single_run.csv");
            f << "mu,seed,burn_in,measure,abund_ALLD,abund_ALLD_TILDE,abund_DIS,abund_DIS_TILDE\n";
            f << std::setprecision(17);
            for (const auto& r : results) {
                f << r.mu << ',' << r.seed << ',' << r.burn_in << ',' << r.measure;
                for (int k = 0; k < NUM_STRATEGIES; ++k) f << ',' << r.abund[k];
                f << '\n';
            }
        }
        {
            std::ofstream f(p.csv_dir + "/averaged.csv");
            f << "mu,num_seeds,mean_ALLD,mean_ALLD_TILDE,mean_DIS,mean_DIS_TILDE,se_ALLD,se_ALLD_TILDE,se_DIS,se_DIS_TILDE\n";
            f << std::setprecision(17);
            for (int m = 0; m < n_mu; ++m) {
                f << p.mus[m] << ',' << aggs[m].n;
                for (int k = 0; k < NUM_STRATEGIES; ++k) f << ',' << aggs[m].mean[k];
                for (int k = 0; k < NUM_STRATEGIES; ++k) f << ',' << aggs[m].se[k];
                f << '\n';
            }
        }
        std::printf("CSV written to %s/single_run.csv and %s/averaged.csv\n",
                    p.csv_dir.c_str(), p.csv_dir.c_str());
    }

    // --- MySQL output ---
    if (p.use_db) {
        DbWriter db;
        if (const char* password = std::getenv("IR_DB_PASSWORD")) {
            p.db_pass = password;
        }
        if (!db.connect(p.db_host, p.db_port, p.db_user, p.db_pass, p.db_name)) {
            std::fprintf(stderr, "DB connect failed: %s\n", db.last_error().c_str());
        } else if (!db.ensure_schema()) {
            std::fprintf(stderr, "DB schema failed: %s\n", db.last_error().c_str());
        } else {
            int ok = 0;
            for (const auto& r : results) {
                if (db.insert_run(r, p)) ++ok;
                else std::fprintf(stderr, "DB insert failed: %s\n", db.last_error().c_str());
            }
            std::printf("DB: inserted %d/%zu single-run rows\n", ok, results.size());
            if (db.aggregate(p)) std::printf("DB: aggregated into averaged_result\n");
            else std::fprintf(stderr, "DB aggregate failed: %s\n", db.last_error().c_str());
        }
    }

    std::printf("done.\n");
    return 0;
}
