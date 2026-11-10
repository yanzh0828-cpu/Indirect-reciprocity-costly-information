#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Draw the manuscript figure or plot CSV output from a new full run."""
import argparse
import csv
import math
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

# Historical plotting inputs, preserved for comparison with figures/mu.pdf.
mus = [1e-4, 2.154434690031884e-4, 4.641588833612779e-4, 1e-3,
       2.154434690031884e-3, 4.641588833612779e-3, 1e-2,
       2.154434690031884e-2, 4.641588833612779e-2, 1e-1,
       2.154434690031884e-1, 4.641588833612779e-1, 1.0]

num_seeds = [512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512]  # β=0.0003 各 μ seed 数

# env1 策略 {ALLC, ALLD, DIS, DIS~}   β=0.0003, 512 seed
env1 = {
    "name": "Environment 1",
    "mean_ALLC":      [0.251564, 0.250078, 0.247927, 0.251391, 0.251015, 0.249237, 0.252914, 0.249450, 0.251405, 0.249986, 0.250029, 0.250081, 0.249949],
    "mean_ALLD":      [0.246708, 0.248073, 0.252918, 0.250988, 0.252830, 0.249224, 0.246915, 0.250068, 0.249128, 0.250202, 0.250131, 0.249862, 0.249936],
    "mean_DIS":       [0.250653, 0.249502, 0.247641, 0.250500, 0.250392, 0.250494, 0.248452, 0.252170, 0.250215, 0.249669, 0.249956, 0.249918, 0.249987],
    "mean_DIS_TILDE": [0.251076, 0.252347, 0.251514, 0.247121, 0.245763, 0.251045, 0.251718, 0.248313, 0.249251, 0.250143, 0.249885, 0.250140, 0.250128],
    "se_ALLC":        [0.004344, 0.003889, 0.003788, 0.003907, 0.003416, 0.002780, 0.002125, 0.001637, 0.000967, 0.000468, 0.000255, 0.000121, 0.000075],
    "se_ALLD":        [0.004125, 0.004055, 0.003988, 0.003707, 0.003310, 0.002631, 0.002245, 0.001557, 0.000999, 0.000460, 0.000260, 0.000125, 0.000079],
    "se_DIS":         [0.004015, 0.003997, 0.003976, 0.003909, 0.003527, 0.002937, 0.002218, 0.001686, 0.000969, 0.000461, 0.000260, 0.000126, 0.000072],
    "se_DIS_TILDE":   [0.004374, 0.004004, 0.003916, 0.003758, 0.003326, 0.002859, 0.002393, 0.001553, 0.000968, 0.000475, 0.000256, 0.000113, 0.000079],
}

# env2 策略 {ALLD_1, ALLD_2, DIS, DIS~}   β=0.0003, 512 seed
env2 = {
    "name": "Environment 2",
    "mean_ALLD_1":    [0.250588, 0.253060, 0.251613, 0.249124, 0.248606, 0.252552, 0.247715, 0.250231, 0.248299, 0.249846, 0.249544, 0.250009, 0.250003],
    "mean_ALLD_2":    [0.250069, 0.250476, 0.252955, 0.249457, 0.248591, 0.243840, 0.249897, 0.251986, 0.250961, 0.249701, 0.250275, 0.249960, 0.249961],
    "mean_DIS":       [0.245883, 0.252038, 0.246117, 0.255280, 0.248985, 0.252746, 0.252837, 0.249320, 0.251099, 0.250653, 0.250080, 0.250136, 0.249994],
    "mean_DIS_TILDE": [0.253460, 0.244426, 0.249316, 0.246139, 0.253819, 0.250863, 0.249551, 0.248463, 0.249642, 0.249800, 0.250101, 0.249896, 0.250042],
    "se_ALLD_1":      [0.004195, 0.004405, 0.004180, 0.003769, 0.003200, 0.002791, 0.002161, 0.001725, 0.000988, 0.000486, 0.000263, 0.000121, 0.000078],
    "se_ALLD_2":      [0.004242, 0.004121, 0.003838, 0.003870, 0.003334, 0.002719, 0.002197, 0.001589, 0.001084, 0.000498, 0.000248, 0.000122, 0.000074],
    "se_DIS":         [0.004065, 0.004036, 0.004051, 0.003913, 0.003256, 0.002939, 0.002324, 0.001634, 0.000988, 0.000485, 0.000259, 0.000126, 0.000072],
    "se_DIS_TILDE":   [0.004471, 0.004017, 0.003851, 0.003719, 0.003403, 0.002805, 0.002135, 0.001643, 0.001001, 0.000484, 0.000258, 0.000120, 0.000073],
}
# env3 策略 {ALLD, ALLD~, DIS, DIS~}   β=0.0003, 512 seed
env3 = {
    "name": "Environment 3",
    "mean_ALLD":       [0.245002, 0.246462, 0.251468, 0.249708, 0.248301, 0.250830, 0.246842, 0.251143, 0.248686, 0.249488, 0.249469, 0.250131, 0.249902],
    "mean_ALLD_TILDE": [0.250899, 0.247102, 0.248250, 0.249728, 0.251539, 0.248608, 0.248735, 0.250960, 0.250345, 0.249813, 0.250536, 0.250037, 0.250008],
    "mean_DIS":        [0.252146, 0.254778, 0.252876, 0.247592, 0.252766, 0.250146, 0.251553, 0.247002, 0.250287, 0.250053, 0.249841, 0.250005, 0.249994],
    "mean_DIS_TILDE":  [0.251953, 0.251658, 0.247407, 0.252972, 0.247395, 0.250416, 0.252871, 0.250895, 0.250681, 0.250646, 0.250154, 0.249826, 0.250095],
    "se_ALLD":         [0.004260, 0.003972, 0.004275, 0.003705, 0.003217, 0.002726, 0.002203, 0.001541, 0.001030, 0.000477, 0.000265, 0.000125, 0.000072],
    "se_ALLD_TILDE":   [0.004143, 0.004147, 0.004093, 0.003640, 0.003540, 0.002720, 0.002148, 0.001714, 0.000975, 0.000478, 0.000261, 0.000119, 0.000073],
    "se_DIS":          [0.004203, 0.004265, 0.004056, 0.003805, 0.003551, 0.002889, 0.002260, 0.001669, 0.000981, 0.000464, 0.000251, 0.000122, 0.000072],
    "se_DIS_TILDE":    [0.004282, 0.004088, 0.003899, 0.003888, 0.003387, 0.002857, 0.002294, 0.001573, 0.000964, 0.000500, 0.000265, 0.000119, 0.000074],
}
# ============================================================

def tot(env):
    return [a + b for a, b in zip(env["mean_DIS"], env["mean_DIS_TILDE"])]

# ---- μ → 0（固定即突变）的 fixation fraction（512 seed，β=0.0003）----
# 判别者总量 DIS + DIS~（用于图中 μ→0 点）
disc0 = [ 0.499852, 0.500133, 0.500296]   # env1, env2, env3

# 各策略 fixation fraction（备用观察）
fix0_env1 = {"ALLC": 0.249784, "ALLD": 0.249920, "DIS": 0.249596, "DIS~": 0.250700}
fix0_env2 = {"ALLD_1": 0.249829, "ALLD_2": 0.250037, "DIS": 0.250099, "DIS~": 0.250034}
fix0_env3 = {"ALLD": 0.249681, "ALLD~": 0.250467, "DIS": 0.250640, "DIS~": 0.249212}


def read_csv(path):
    with path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def load_full_run(run_dir):
    curves = []
    limits = []
    for environment in ("env1", "env2", "env3"):
        path = run_dir / environment
        finite = read_csv(path / "finite" / "output" / "averaged.csv")
        fixation = read_csv(path / "fixation" / "output" / "averaged.csv")
        if [float(row["mu"]) for row in finite] != mus:
            raise ValueError(f"{path}: expected all 13 mutation rates from a full run")
        if len(fixation) != 1 or any(int(row["num_seeds"]) != 512 for row in finite + fixation):
            raise ValueError(f"{path}: expected one summary per point from 512 runs")
        curves.append([
            float(row["mean_DIS"]) + float(row["mean_DIS_TILDE"]) for row in finite
        ])
        row = fixation[0]
        limits.append(float(row["mean_fix_DIS"]) + float(row["mean_fix_DIS_TILDE"]))
    return curves, limits


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--runs-dir", type=Path, help="plot CSVs produced by reproduce.sh full")
parser.add_argument("--output", type=Path, help="destination PDF")
args = parser.parse_args()

out_dir = Path(__file__).resolve().parent / "output"
if args.runs_dir:
    (t1, t2, t3), disc0 = load_full_run(args.runs_dir)
    out = args.output or out_dir / "compare_envs_rerun.pdf"
else:
    t1, t2, t3 = tot(env1), tot(env2), tot(env3)
    out = args.output or out_dir / "compare_envs_no_welfare.pdf"

ln = [math.log(m) for m in mus]

x0 = math.log(1e-5)   # 图中 μ→0 点的 x 位置（真实 μ=0 在 -∞）

x_min, x_max = math.log(1e-5) - 0.15, math.log(1.0) + 0.12
y_min, y_max = 0.475, 0.525

plt.rcParams.update({
    "font.family": "DejaVu Sans",
    "mathtext.fontset": "stix",
    "pdf.fonttype": 42,
    "ps.fonttype": 42,
})

fig, ax = plt.subplots(figsize=(10, 6), constrained_layout=True)
ax.set_xlim(x_min, x_max)
ax.set_ylim(y_min, y_max)
ax.set_yticks([0.48, 0.49, 0.50, 0.51, 0.52])
ax.set_xticks([x0, *[math.log(dec) for dec in [1e-4, 1e-3, 1e-2, 1e-1, 1.0]]])
ax.set_xticklabels([r"$\mu\to0$", r"$10^{-4}$", r"$10^{-3}$", r"$10^{-2}$", r"$10^{-1}$", r"$10^{0}$"])
ax.grid(True, color="#e6e6e6", linewidth=1)

# Matplotlib mathtext renders the labels as LaTeX-style mathematics.
ax.set_xlabel(r"mutation rate $\mu$", fontsize=20)
ax.set_ylabel(r"$\langle DIS \rangle + \langle \widetilde{DIS} \rangle$", fontsize=20)
ax.tick_params(axis="both", labelsize=15)

series = [
    (t1, disc0[0], "#1f77b4", "Environment 1"),
    (t2, disc0[1], "#d62728", "Environment 2"),
    (t3, disc0[2], "#2ca02c", "Environment 3"),
]
for values, limit, color, label in series:
    ax.plot(ln, values, color=color, linewidth=2.2, marker="o", markersize=6,
            markeredgecolor="white", markeredgewidth=1, label=label)
    ax.plot(x0, limit, marker="D", color=color, markersize=7,
            markeredgecolor="white", markeredgewidth=1)
    ax.plot([x0, ln[0]], [limit, values[0]], color=color, linewidth=1.6,
            linestyle=(0, (4, 3)))

xsep = math.log(1e-5) + 0.5 * math.log(10)
ax.axvline(xsep, color="#999999", linewidth=1, linestyle=(0, (4, 3)))
ax.plot([], [], marker="D", color="black", linestyle="None", label=r"$\mu\to0$ (fixation limit)")
ax.legend(loc="upper left", fontsize=15, framealpha=0.92, edgecolor="#cccccc")

out.parent.mkdir(parents=True, exist_ok=True)
fig.savefig(out, format="pdf", bbox_inches="tight")
plt.close(fig)
print("wrote", out)
print("points per env:", len(mus), len(mus), len(mus))
print("num_seeds:", num_seeds)
print("DIS+DIS~ range:", round(min(t1+t2+t3), 5), round(max(t1+t2+t3), 5))
