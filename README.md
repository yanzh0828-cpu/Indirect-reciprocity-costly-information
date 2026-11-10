# Evolution of cooperation in indirect reciprocity with costly information

This repository contains the simulation code for the manuscript by Zhenghan Yan and Bin Wu.

## Repository structure

| Path | Contents |
| --- | --- |
| `src/`, `env2/`, `env3/` | Finite-mutation C++ simulations for Environments 1, 2, and 3, respectively. Each environment has its own `CMakeLists.txt`. |
| `fixation_mutation/` | C++ simulations for the small-mutation limit, organized into `env1/`, `env2/`, and `env3/`. |
| `reproduce.sh` | Script for building and running the simulations. |
| `plot_compare_env_no_welfare.py`, `requirements.txt` | Figure-generation script and its Python dependency. |
| [`figures/mu.pdf`](figures/mu.pdf) | Original manuscript figure on discriminator abundance across mutation rates. |
| [`data/simulation_summary.csv`](data/simulation_summary.csv) | Archived database summaries for the two discriminator strategies in each environment. |

## Correspondence with the manuscript

The finite-mutation programs implement the individual-based simulations described in the manuscript's **Mutation rate** subsection. They use repeated donation games with Image Scoring and Fermi strategy updates. Each environment uses the strategy set defined in the manuscript.

The three curves show the combined abundance of `DIS` and `DIS_TILDE` across 13 mutation rates. Each point summarizes 512 independent simulations with population size `N = 100` and selection intensity `beta = 0.0003`. The diamonds show the small-mutation estimates obtained from successful fixations. The plotting script preserves the numerical inputs used for the manuscript figure.
