#!/usr/bin/env bash
set -euo pipefail

mode=${1:-quick}
if [[ $# -gt 1 || ( $mode != quick && $mode != full ) ]]; then
    echo "Usage: bash reproduce.sh [quick|full]" >&2
    exit 2
fi

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
work_dir=${IR_WORK_DIR:-"$root"}
threads=${IR_THREADS:-$(nproc)}

mkdir -p "$work_dir/build" "$work_dir/runs"
work_dir="$(cd "$work_dir" && pwd)"
run_dir="$(mktemp -d "$work_dir/runs/$mode.XXXXXXXX")"
echo "Run directory: $run_dir"

finite_projects=(. env2 env3)
finite_seeds=(6501028323579594936 12277528535252757048 1496360029669962621)
fixation_seeds=(1001 1002 1003)
finite_options=(--seeds 512)
fixation_options=(--seeds 512 --measure-fix 1200)
if [[ $mode == quick ]]; then
    finite_options=(--seeds 2 --quick --mu-first 1)
    fixation_options=(--seeds 2 --quick)
fi

run_project() {
    local project=$1 name=$2 base_seed=$3
    shift 3
    local build_dir="$work_dir/build/$name"
    local result_dir="$run_dir/$name"
    cmake -S "$root/$project" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$build_dir" --parallel
    mkdir -p "$result_dir"
    (
        cd "$result_dir"
        "$build_dir/ir_sim" --no-db --rounds 50 --beta 0.0003 \
            --threads "$threads" --base-seed "$base_seed" "$@" | tee run.log
    )
}

for i in "${!finite_projects[@]}"; do
    env="env$((i + 1))"
    run_project "${finite_projects[i]}" "$env/finite" "${finite_seeds[i]}" "${finite_options[@]}"
    run_project "fixation_mutation/$env" "$env/fixation" "${fixation_seeds[i]}" "${fixation_options[@]}"
done

echo "Completed $mode simulation in $run_dir"
