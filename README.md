# B-tree, B+-tree, B*-tree Experiments

CSE321 Project #1

## Build & Run

Requires a C++17 compiler (`g++` or `clang++`) and `make`.

```bash
make            # builds ./btree_exp
make run        # runs ./btree_exp student.csv
make clean
```

The driver expects `student.csv` (header + 100,000 rows) in the working
directory, or the path can be passed as `argv[1]`:

```bash
./btree_exp path/to/student.csv
```

CSV schema: `Student ID,Name,Gender,GPA,Height,Weight` — the 9-digit student
ID is the index key and the row index is stored as the RID.

## Project Layout

```
src/
  student.{h,cpp}     # CSV loader and Student record struct
  btree.{h,cpp}       # plain B-tree
  bplustree.{h,cpp}   # B+-tree (separators-only internals, linked leaves)
  bstartree.{h,cpp}   # B*-tree (redistribute-before-split, 2-to-3 split)
  main.cpp            # templated driver — runs every workload on each tree
Makefile
student.csv           # 100,000-row dataset
```

## Experiments

The driver runs seven experiments back-to-back: four mandated by the
assignment plus three additional ones. All three trees use the same
input order and the same RNG seeds for queries / deletions, so results are
directly comparable. Experiment numbering follows the source code (the
gap at #6 reflects an early selectivity-sweep experiment that was dropped
in favor of the two distributional ones).

### Mandated workloads

| # | Experiment       | Parameters                                                  |
|---|------------------|-------------------------------------------------------------|
| 1 | Insertion sweep  | d ∈ {3, 5, 10, 50, 100}; reports time, height, nodes, splits, redists, utilization |
| 2 | Point search     | d = 10, 10,000 random IDs (seed 42)                          |
| 3 | Range query      | d = 10, ID ∈ [202000000, 202100000], aggregates GPA & height of male students |
| 4 | Progressive deletion | d = 10, 9 batches of 10% each (90% total) shuffled with seed 2026; per-batch latency, structural metrics, post-batch search probe (1,000 keys, seed 7) |

### Additional workloads

| # | Experiment       | Parameters                                                  |
|---|------------------|-------------------------------------------------------------|
| 5 | Insertion-order pathology | d = 10, all 3 trees rebuilt under ascending / descending / CSV-order input |
| 7 | B*-tree redistribution effectiveness | d = 10, snapshots the redistribute / 2-to-3 split / 1-to-2 split breakdown after every 10,000 inserts; ends with a vanilla-B-tree baseline for the avoided-splits count |
| 8 | Point-search tail latency | d = 10, 100,000 random hits (seed 42, same as #2); reports mean, stdev, p50/p90/p95/p99/p99.9, and per-tree maximum |
