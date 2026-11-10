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
        "  --seeds <n>         seeds (default 64)\n"
        "  --rounds <n>        rounds per generation T (default 50)\n"
        "  --beta <v>          selection intensity beta (default 0.0003)\n"
        "  --measure-fix <n>   successful fixations to measure (default 1200)\n"
        "  --threads <n>       OpenMP thread count (default: all)\n"
        "  --base-seed <n>     base RNG seed (default: random_device)\n"
        "  --quick             tiny measure for a fast smoke test\n"
        "  --no-db             do not write to MySQL\n"
        "  --no-csv            do not write CSV files\n"
        "  -h, --help          show this help\n",
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
        if (a == "--seeds") p.seeds = std::atoi(next("--seeds"));
        else if (a == "--rounds") p.rounds = std::atoi(next("--rounds"));
        else if (a == "--beta") p.beta = std::atof(next("--beta"));
        else if (a == "--measure-fix") p.measure_fixations = std::strtoull(next("--measure-fix"), nullptr, 10);
        else if (a == "--threads") {
#ifdef _OPENMP
            omp_set_num_threads(std::atoi(next("--threads")));
#else
            (void)next("--threads");
#endif
        }
        else if (a == "--base-seed") p.base_seed = std::strtoull(next("--base-seed"), nullptr, 10);
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

    const int n_seed = p.seeds;
    const long n_tasks = static_cast<long>(n_seed);

    std::vector<RunResult> results(n_tasks);

    std::printf("N=%d b=%g c=%g q=%g dq=%g p=%g beta=%g rounds=%d seeds=%d quick=%d\n",
                p.N, p.b, p.c, p.q, p.dq, p.p, p.beta, p.rounds, n_seed, p.quick ? 1 : 0);
    std::printf("measure_fix=%llu\n", static_cast<unsigned long long>(measure_fix_for(p)));
    std::printf("base_seed=%llu  tasks=%ld\n", static_cast<unsigned long long>(base_seed), n_tasks);
#ifdef _OPENMP
    std::printf("threads=%d\n", omp_get_max_threads());
#endif

    auto t0 = std::chrono::steady_clock::now();
    std::printf("running %ld tasks (%d seeds) ...\n", n_tasks, n_seed);
    std::fflush(stdout);

    std::atomic<long> done{0};
    const long report_step = std::max<long>(1, n_tasks / 20);

#pragma omp parallel for schedule(dynamic, 1)
    for (long t = 0; t < n_tasks; ++t) {
        const uint64_t seed = splitmix64(
            base_seed
            ^ (static_cast<uint64_t>(t + 1) * 0x9e3779b97f4a7c15ULL));
        results[t] = run_one_seed(p, seed);

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

    // --- aggregate across seeds (fixation fraction only) ---
    struct Agg {
        int n;
        double mean_fix[NUM_STRATEGIES];
        double se_fix[NUM_STRATEGIES];
        double mean_fixations;
    } agg;
    {
        double sf[NUM_STRATEGIES] = {0, 0, 0, 0};
        double sf2[NUM_STRATEGIES] = {0, 0, 0, 0};
        double sum_fix = 0.0;
        for (int s = 0; s < n_seed; ++s) {
            const RunResult& r = results[s];
            sum_fix += static_cast<double>(r.fixations);
            for (int k = 0; k < NUM_STRATEGIES; ++k) {
                sf[k] += r.fix_abund[k];
                sf2[k] += r.fix_abund[k] * r.fix_abund[k];
            }
        }
        agg.n = n_seed;
        agg.mean_fixations = sum_fix / n_seed;
        for (int k = 0; k < NUM_STRATEGIES; ++k) {
            agg.mean_fix[k] = sf[k] / n_seed;
            agg.se_fix[k] = (n_seed > 1)
                ? std::sqrt((sf2[k] - sf[k] * sf[k] / n_seed) / (n_seed - 1) / n_seed)
                : 0.0;
        }
    }

    std::printf("\n== summary (fixation fraction across %d seeds) ==\n", agg.n);
    std::printf("mean successful fixations/seed = %.1f\n", agg.mean_fixations);
    std::printf("fixation abundance : ALLD_1=%.6f ALLD_2=%.6f DIS=%.6f DIS~=%.6f\n",
                agg.mean_fix[0], agg.mean_fix[1], agg.mean_fix[2], agg.mean_fix[3]);
    std::printf("  se               : ALLD_1=%.6f ALLD_2=%.6f DIS=%.6f DIS~=%.6f\n",
                agg.se_fix[0], agg.se_fix[1], agg.se_fix[2], agg.se_fix[3]);

    // --- CSV output ---
    if (p.use_csv) {
        mkdir(p.csv_dir.c_str(), 0755);
        {
            std::ofstream f(p.csv_dir + "/single_run.csv");
            f << "seed,measure_fix,fixations,fix_ALLD_1,fix_ALLD_2,fix_DIS,fix_DIS_TILDE\n";
            f << std::setprecision(17);
            for (const auto& r : results) {
                f << r.seed << ',' << r.measure_fix << ',' << r.fixations;
                for (int k = 0; k < NUM_STRATEGIES; ++k) f << ',' << r.fix_abund[k];
                f << '\n';
            }
        }
        {
            std::ofstream f(p.csv_dir + "/averaged.csv");
            f << "num_seeds,mean_fixations,mean_fix_ALLD_1,mean_fix_ALLD_2,mean_fix_DIS,mean_fix_DIS_TILDE,"
                 "se_fix_ALLD_1,se_fix_ALLD_2,se_fix_DIS,se_fix_DIS_TILDE\n";
            f << std::setprecision(17);
            f << agg.n << ',' << agg.mean_fixations;
            for (int k = 0; k < NUM_STRATEGIES; ++k) f << ',' << agg.mean_fix[k];
            for (int k = 0; k < NUM_STRATEGIES; ++k) f << ',' << agg.se_fix[k];
            f << '\n';
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
            if (db.aggregate(p)) std::printf("DB: aggregated into averaged_result_fix_env2\n");
            else std::fprintf(stderr, "DB aggregate failed: %s\n", db.last_error().c_str());
        }
    }

    std::printf("done.\n");
    return 0;
}
