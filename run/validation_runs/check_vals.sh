#!/usr/bin/env bash
set -u

ROOT="/mnt/ncsudrive/s/shebbes/ece721/project_4"
VAL_DIR="$ROOT/run/validation_runs"
EXPECTED_DIR="$VAL_DIR/val_runs"
ASTAR_DIR="$ROOT/run/473.astar_biglakes_ref.1844.0.50.gz"
PERL_DIR="$ROOT/run/400.perlbench_splitmail_ref.2724.0.44.gz"
MICRO_DIR="$VAL_DIR/microbenchmark_runs"
ENV_SCRIPT="/mnt/designkits/spec_2006_2017/O2_fno_bbreorder/activate.bash"

MODE="run"
RUNS=()

usage() {
   cat <<EOF
usage: $0 [--diff-only] [VAL ids...]

Examples:
  $0 19 20 21
  $0 --diff-only 1 12 19
  $0

Default with no VAL ids: run VAL-1 through VAL-22.

Outputs:
  run/validation_runs/results/VAL-N.actual.log
  run/validation_runs/results/VAL-N.run.out
  run/validation_runs/results/VAL-N.diff
EOF
}

while [[ $# -gt 0 ]]; do
   case "$1" in
      --diff-only)
         MODE="diff"
         shift
         ;;
      -h|--help)
         usage
         exit 0
         ;;
      *)
         RUNS+=("${1#VAL-}")
         shift
         ;;
   esac
done

if [[ ${#RUNS[@]} -eq 0 ]]; then
   RUNS=($(seq 1 22))
fi

mkdir -p "$VAL_DIR/results"

source_env() {
   if [[ -f "$ENV_SCRIPT" ]]; then
      set +u
      # shellcheck disable=SC1090
      source "$ENV_SCRIPT" >/dev/null 2>&1
      set -u
   fi
}

bench_dir() {
   local id="$1"
   if (( id >= 1 && id <= 11 )); then
      echo "$ASTAR_DIR"
   elif (( id >= 12 && id <= 18 )); then
      echo "$PERL_DIR"
   elif (( id >= 19 && id <= 22 )); then
      echo "$MICRO_DIR"
   else
      return 1
   fi
}

sim_flags() {
   case "$1" in
      1)  echo "--vp-eligible=1,0,1 --vp-perf=1 --mdp=5,0 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000" ;;
      2)  echo "--vp-eligible=1,0,1 --vp-svp=300,1,10,10,31 --mdp=4,0 --perf=1,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000" ;;
      3)  echo "--vp-eligible=1,0,0 --vp-svp=300,1,10,10,31 --mdp=4,0 --perf=1,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000" ;;
      4)  echo "--vp-eligible=0,0,1 --vp-svp=300,1,10,10,31 --mdp=4,0 --perf=1,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000" ;;
      5)  echo "--vp-eligible=1,0,1 --vp-svp=300,0,10,10,31 --mdp=5,0 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000" ;;
      6)  echo "--vp-eligible=1,0,1 --vp-svp=100,0,10,10,31 --mdp=5,0 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000" ;;
      7)  echo "--vp-eligible=1,0,1 --vp-svp=300,0,7,10,31 --mdp=5,0 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000" ;;
      8)  echo "--vp-eligible=1,0,1 --vp-svp=300,0,7,0,31 --mdp=5,0 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000" ;;
      9)  echo "--vp-eligible=1,0,1 --vp-svp=300,0,8,0,31 --mdp=5,0 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000" ;;
      10) echo "--vp-eligible=1,0,1 --vp-svp=300,0,8,0,15 --mdp=5,0 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000" ;;
      11) echo "--vp-eligible=1,0,1 --vp-svp=300,0,8,0,7 --mdp=5,0 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000" ;;
      12) echo "--vp-eligible=1,0,1 --vp-perf=1 --mdp=5,0 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000" ;;
      13) echo "--vp-eligible=1,0,1 --vp-svp=300,0,10,10,31 --mdp=5,0 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000" ;;
      14) echo "--vp-eligible=1,0,1 --vp-svp=80,0,10,10,31 --mdp=5,0 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000" ;;
      15) echo "--vp-eligible=1,0,1 --vp-svp=300,0,10,0,31 --mdp=5,0 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000" ;;
      16) echo "--vp-eligible=1,0,1 --vp-svp=300,0,14,0,31 --mdp=5,0 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000" ;;
      17) echo "--vp-eligible=1,0,1 --vp-svp=300,0,14,0,15 --mdp=5,0 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000" ;;
      18) echo "--vp-eligible=1,0,1 --vp-svp=300,0,14,0,63 --mdp=5,0 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000" ;;
      19) echo "--vp-eligible=1,0,1 --vp-svp=256,0,4,0,3 --mdp=5,0 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -s4481409 -e4000000 pk array2.riscv" ;;
      20) echo "--vp-eligible=1,0,1 --vp-svp=256,0,4,0,1 --mdp=5,0 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -s4481409 -e4000000 pk array2.riscv" ;;
      21) echo "--vp-eligible=1,0,1 --vp-svp=256,0,4,0,0 --mdp=5,0 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -s4481409 -e4000000 pk array2.riscv" ;;
      22) echo "--vp-eligible=1,0,1 --vp-svp=256,0,4,0,3 --mdp=5,0 --perf=0,0,0,0 --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -s869325 -e45036 pk if2.riscv" ;;
      *) return 1 ;;
   esac
}

run_val() {
   local id="$1"
   local dir expected actual runout diffout flags latest rc

   dir="$(bench_dir "$id")" || { echo "VAL-$id: unknown run id"; return 1; }
   flags="$(sim_flags "$id")" || { echo "VAL-$id: no command"; return 1; }
   expected="$EXPECTED_DIR/VAL-${id}.txt"
   actual="$VAL_DIR/results/VAL-${id}.actual.log"
   runout="$VAL_DIR/results/VAL-${id}.run.out"
   diffout="$VAL_DIR/results/VAL-${id}.diff"

   if [[ ! -f "$expected" ]]; then
      echo "VAL-$id: missing expected file $expected"
      return 1
   fi

   if [[ "$MODE" == "run" ]]; then
      echo "===== VAL-$id RUN ====="
      (
         cd "$dir"
         if (( id <= 18 )); then
            make cleanrun SIM_FLAGS_EXTRA="$flags"
         else
            ./721sim $flags
         fi
      ) >"$runout" 2>&1
      rc=$?
      if [[ $rc -ne 0 ]]; then
         echo "VAL-$id: RUN FAILED rc=$rc (see $runout)"
         return "$rc"
      fi

      latest="$(cd "$dir" && ls -1t stats.*.log | head -1)"
      cp "$dir/$latest" "$actual"
   elif [[ ! -f "$actual" ]]; then
      echo "VAL-$id: missing $actual for --diff-only"
      return 1
   fi

   if diff -iw "$actual" "$expected" >"$diffout"; then
      echo "VAL-$id: CLEAN"
      return 0
   fi

   echo "VAL-$id: DIFF (see $diffout)"
   return 1
}

if [[ "$MODE" == "run" ]]; then
   source_env
fi

fail=0
for id in "${RUNS[@]}"; do
   if ! run_val "$id"; then
      fail=1
   fi
done

exit "$fail"
