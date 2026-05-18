# ECE721 Project 4 - Hybrid Value Prediction Architecture

## Overview

This project extends the baseline 721sim Value Prediction Unit (VPU) with a hybrid value prediction architecture designed to improve IPC under a fixed 32 KiB storage budget.

The final design combines:

* A **Base Stride Value Predictor (SVP)**
* A **Path-Sensitive Stride Value Predictor (Path-SVP)**
* A **Per-PC Blacklist Recovery Filter**

The implementation is fully integrated into the out-of-order pipeline and follows all Project 4 requirements:

* Real value prediction
* Real confidence
* Real dependency breaking
* Real recovery on value misprediction
* VPQ-based in-flight tracking and retire-time training

The final submitted configuration achieved:

```text
Harmonic Mean IPC = 2.0615
```

compared to:

```text
Baseline superscalar processor     : 1.6585
Reference SVP predictor            : 2.0195
```

---

# Final Predictor Architecture

The final design uses a staged hybrid predictor pipeline:

```text
Base SVP -> Path-SVP -> Blacklist Veto -> Prediction Injection
```

## Motivation

Early experiments showed that the baseline SVP suffered from two major limitations:

### 1. Toxic PCs

Some PCs repeatedly generated confident value mispredictions.

These predictions caused:

* pipeline squashes
* dependency replay
* expensive recovery penalties

Even a relatively small number of confident wrong predictions significantly reduced IPC.

---

### 2. Path-Dependent Value Streams

The baseline SVP indexed only by PC.

However, several benchmarks exhibited:

* different stride streams
* under the same PC
* depending on execution path

This caused:

* aliasing between unrelated value streams
* confidence instability
* large numbers of `unconf_corr` events

The predictor often learned the correct values, but never became confident enough to inject them.

---

# Phase 1 - Baseline SVP

The baseline predictor is a stride value predictor integrated into the rename/dispatch/execute/retire pipeline.

## Prediction Flow

At Rename:

```text
prediction = retired_value + instance * stride
```

If:

* instruction is eligible
* confidence is maximal

then:

* prediction is injected
* destination register is marked ready
* dependent instructions may wake up early

---

## VPQ Integration

A Value Prediction Queue (VPQ) tracks:

* in-flight predictions
* prediction metadata
* instance counters
* recovery information

The VPQ acts as:

* a rename-stage structural hazard
* the authoritative source for retire-time training

Training is performed strictly at retirement.

---

## Base SVP Structure

| Field | Bits |
| ----- | ---- |
| Tag | 10 |
| Confidence | 5 |
| Retired Value | 64 |
| Stride | 64 |
| Instance Counter | 9 |

### Total

```text
1024 entries
152 bits/entry
155648 bits
19456 bytes
19.0 KiB
```

---

# Phase 2 - Blacklist Recovery Filter

## Problem

Some PCs consistently produced harmful confident mispredictions.

Examples included:

* hmmer
* sphinx3
* povray
* bzip2

These PCs caused repeated pipeline flushes.

---

## Design

A small per-PC blacklist table was added.

Each entry tracks:

| Field | Bits |
| ----- | ---- |
| Valid | 1 |
| Tag | 16 |
| Bad Counter | 4 |
| Cooldown | 16 |

If a PC repeatedly causes confident wrong predictions:

```text
bad >= threshold
```

then the PC enters cooldown.

During cooldown:

* predictions from that PC are suppressed
* VPQ allocation still occurs
* retire-time training still continues

This is important because:

* suppression only blocks injection
* the predictor still learns future behavior

---

## Blacklist Behavior

The blacklist acts as:

* a recovery-oriented confidence filter
* not a replacement predictor

It reduces:

* expensive squashes
* harmful speculation

while preserving:

* predictor training
* future convergence

---

## Blacklist Results

The blacklist significantly reduced:

* confident incorrect predictions
* recovery overhead

while improving IPC stability across difficult workloads.

Best blacklist configuration:

```text
Cooldown = 96 cycles
```

---

# Confidence Hysteresis Experiments

Several experiments explored:

* lowering confidence thresholds
* relaxing confidence requirements
* modifying confidence decay behavior

Examples:

* lower confmax
* trust-relax
* hysteresis tuning

---

## Observation

Lowering confidence globally increased:

* prediction coverage

but also increased:

* confident wrong predictions

This produced:

* more squashes
* more recovery events
* lower harmonic mean IPC

---

## Key Insight

Global confidence tuning was too coarse.

The predictor needed:

* selective confidence improvement
* not universal early prediction

This directly motivated Path-SVP.

---

# Phase 3 - Path-SVP

## Motivation

The baseline SVP learns only:

* one stride stream per PC

However:

* many PCs exhibit multiple value streams
* depending on recent control-flow path

PC-only indexing mixes these streams together.

---

## Path-SVP Concept

Path-SVP introduces:

* lightweight path context
* into predictor indexing

This allows:

* the same PC
* to map to different entries
* under different execution paths

---

# Path-SVP Architecture

Path-SVP is implemented as:

* a secondary stride predictor
* used only when base SVP is not confident

---

## Indexing

Path-SVP index:

```text
index = (PC XOR Path_History)
```

Path history is:

* updated speculatively at rename
* checkpointed on branches
* restored on rollback/squash

This keeps predictor state aligned with speculative execution.

---

## Path History

Path history is updated using recent PCs:

```text
Path_History = ((Path_History << shift) ^ (PC >> 2)) & mask
```

This creates a compact speculative execution signature.

---

## Path-SVP Entry

| Field | Bits |
| ----- | ---- |
| Valid | 1 |
| Tag | 10 |
| Confidence | 5 |
| Retired Value | 64 |
| Stride | 64 |
| Instance Counter | 9 |

---

## Path-SVP Prediction

Prediction formula:

```text
prediction = retired_value + instance * stride
```

Each Path-SVP entry maintains:

* its own instance counter
* VPQ metadata
* rollback repair state

This prevents:

* loop-stream corruption
* overlapping iteration aliasing

---

## VPQ Integration

Path-SVP metadata is stored inside normal VPQ entries:

* path index
* path tag
* path instance increment
* path predictor usage

Training still occurs:

* strictly at retirement
* using committed architectural values

---

## Recovery Support

Path-SVP fully supports:

* branch rollback
* load replay recovery
* full squash recovery

Rollback repairs:

* instance counters
* speculative path history

---

# Hybrid Predictor Flow

Final prediction pipeline:

```text
1. Try Base SVP
2. If not confident -> try Path-SVP
3. Blacklist performs final veto
4. Inject prediction
```

This allows:

* normal stride locality
* path-sensitive stride locality
* suppression of toxic PCs

simultaneously.

---

# Experimental Results

## Final IPC

| Predictor | Harmonic Mean IPC |
| --------- | ----------------- |
| Baseline Processor | 1.6585 |
| Reference SVP | 2.0195 |
| Final Hybrid Predictor | 2.0615 |

---

## IPC Improvements

Large gains were observed on:

| Benchmark | IPC |
| --------- | --- |
| xalancbmk | 2.27 |
| perlbench | 5.30 |
| astar | 2.86 |
| leela | 2.41 |
| hmmer | 2.38 |

---

## Ablation Study

Removing Path-SVP reduced IPC from:

```text
2.0615 -> 2.0343
```

This demonstrated that:

* Path-SVP contributes real performance gains
* improvements are architectural
* gains are not caused by noise or reporting artifacts

---

# Storage Accounting

## Base SVP

```text
1024 entries
152 bits/entry
155648 bits
19456 B
19.0 KiB
```

---

## Path-SVP

```text
512 entries
153 bits/entry
78336 bits
9792 B
9.563 KiB
```

---

## Blacklist

```text
512 entries
37 bits/entry
18944 bits
2368 B
2.313 KiB
```

---

# Total Storage

```text
252928 bits
31616 bytes
30.875 KiB
```

Budget:

```text
32 KiB
```

Remaining margin:

```text
1152 bytes
```

VPQ storage is excluded from the competition storage budget per project specification.

---

# Final Configuration

```bash
make cleanrun SIM_FLAGS_EXTRA='--vp-eligible=1,0,1 --vp-svp=300,0,10,10,31 --vp-phase2=1 --mdp=5,0 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000'
```

---

# Key Takeaways

This project demonstrates that:

* simple stride prediction alone is insufficient for many SPEC workloads
* toxic PCs must be selectively suppressed
* path-sensitive value locality exists in several benchmarks
* lightweight path context can improve confidence stability
* recovery-aware hybrid predictors can significantly improve IPC under tight storage budgets

The final architecture achieved:

* real prediction
* real confidence
* real recovery
* real pipeline integration

while remaining fully compliant with Project 4 requirements.
