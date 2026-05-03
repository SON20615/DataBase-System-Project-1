// ===========================================================================
//  CSE321 Project #1  -- B-tree experiments driver
// ===========================================================================
//  Loads the 100,000-record student.csv into an in-memory array, then
//  runs the four required workloads:
//      (1) Insertion + parameter tuning (d in {3, 5, 10})
//      (2) Point search   (10,000 random keys)
//      (3) Range query    ("average GPA and height of male students
//                           whose IDs are between 202000000 and 202100000")
//      (4) Deletion       (2,000 random records)
//
//  Usage:   ./btree_exp [csv_path]
// ===========================================================================

#include "btree.h"
#include "student.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using clk = std::chrono::high_resolution_clock;

static double secsSince(clk::time_point t0) {
    return std::chrono::duration<double>(clk::now() - t0).count();
}

// ---------------------------------------------------------------------------
//  helper: build a tree of order d by inserting every record
// ---------------------------------------------------------------------------
static void buildTree(BTree& t, const std::vector<Student>& rec) {
    for (size_t i = 0; i < rec.size(); ++i)
        t.insert(rec[i].id, (int)i);
}

// ---------------------------------------------------------------------------
//  Experiment 1 - Insertion & parameter tuning
// ---------------------------------------------------------------------------
static void expInsertion(const std::vector<Student>& rec) {
    std::cout << "\n========== [1] INSERTION & PARAMETER TUNING ==========\n";
    std::cout << "records = " << rec.size() << "\n";
    std::printf("%-4s %-12s %-7s %-8s %-9s %-12s\n",
                "d", "time(s)", "height", "nodes", "splits", "utilisation");

    for (int d : { 3, 5, 10, 50, 100 }) {
        BTree t(d);
        auto t0 = clk::now();
        buildTree(t, rec);
        double sec = secsSince(t0);
        std::printf("%-4d %-12.4f %-7d %-8ld %-9ld %-12.4f\n",
                    d, sec, t.height(), t.nodeCount(),
                    t.splitCount(), t.utilization());
    }
}

// ---------------------------------------------------------------------------
//  Experiment 2 - Point search
// ---------------------------------------------------------------------------
static void expPointSearch(const std::vector<Student>& rec, int d, int q) {
    std::cout << "\n========== [2] POINT SEARCH (d = " << d
              << ", queries = " << q << ") ==========\n";

    BTree t(d);
    buildTree(t, rec);

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> pick(0, (int)rec.size() - 1);
    std::vector<int> queries; queries.reserve(q);
    for (int i = 0; i < q; ++i) queries.push_back(rec[pick(rng)].id);

    auto t0 = clk::now();
    long hits = 0;
    for (int k : queries) if (t.search(k) >= 0) ++hits;
    double sec = secsSince(t0);

    std::printf("hits          : %ld / %d\n",  hits, q);
    std::printf("total time    : %.6f s\n",    sec);
    std::printf("mean per query: %.3f us\n",   (sec / q) * 1e6);
}

// ---------------------------------------------------------------------------
//  Experiment 3 - Range query
//      "Average GPA and height of MALE students with 202000000 <= id <= 202100000"
//
//  NB the project manual writes the bounds as 8-digit numbers (20200000 /
//  20210000); the actual CSV uses 9-digit IDs (e.g. 202038411).  We use the
//  9-digit equivalents so the predicate is non-empty over the real data.
// ---------------------------------------------------------------------------
static void expRangeQuery(const std::vector<Student>& rec, int d) {
    std::cout << "\n========== [3] RANGE QUERY (d = " << d << ") ==========\n";

    constexpr int LO = 202000000;
    constexpr int HI = 202100000;

    BTree t(d);
    buildTree(t, rec);

    auto t0 = clk::now();
    auto rows = t.rangeSearch(LO, HI);
    double scanSec = secsSince(t0);

    auto t1 = clk::now();
    long  cnt = 0;
    double sumGpa = 0.0, sumH = 0.0;
    for (auto [k, rid] : rows) {
        const Student& s = rec[rid];
        if (s.gender == "Male") {
            ++cnt; sumGpa += s.gpa; sumH += s.height;
        }
    }
    double aggSec = secsSince(t1);

    std::printf("range            : [%d, %d]\n",   LO, HI);
    std::printf("hits in range    : %zu\n",        rows.size());
    std::printf("male hits        : %ld\n",        cnt);
    std::printf("avg GPA  (male)  : %.4f\n",       cnt ? sumGpa/cnt : 0.0);
    std::printf("avg Height(male) : %.4f cm\n",    cnt ? sumH  /cnt : 0.0);
    std::printf("scan time        : %.6f s\n",     scanSec);
    std::printf("aggregation time : %.6f s\n",     aggSec);
    std::printf("total            : %.6f s\n",     scanSec + aggSec);
}

// ---------------------------------------------------------------------------
//  Experiment 4 - Deletion
// ---------------------------------------------------------------------------
static void expDeletion(const std::vector<Student>& rec, int d, int nDel) {
    std::cout << "\n========== [4] DELETION (d = " << d
              << ", delete " << nDel << " records) ==========\n";

    BTree t(d);
    buildTree(t, rec);

    long beforeKeys = t.keyCount();
    long beforeNodes = t.nodeCount();
    int  beforeH    = t.height();

    // pick `nDel` distinct RIDs at random
    std::mt19937 rng(2026);
    std::vector<int> idx((size_t)rec.size());
    for (size_t i = 0; i < idx.size(); ++i) idx[i] = (int)i;
    std::shuffle(idx.begin(), idx.end(), rng);
    idx.resize(nDel);

    auto t0 = clk::now();
    long ok = 0;
    for (int i : idx) if (t.remove(rec[i].id)) ++ok;
    double sec = secsSince(t0);

    long afterKeys = t.keyCount();
    long afterNodes = t.nodeCount();
    int  afterH    = t.height();

    // structural-integrity sanity check: every deleted key must now be absent,
    // every other key must still be present (spot-check 5,000 of them)
    long missingHits = 0;
    for (int i : idx) if (t.search(rec[i].id) >= 0) ++missingHits;

    std::vector<int> survivors;
    survivors.reserve(rec.size() - nDel);
    {
        std::vector<char> deleted(rec.size(), 0);
        for (int i : idx) deleted[i] = 1;
        for (size_t i = 0; i < rec.size(); ++i)
            if (!deleted[i]) survivors.push_back((int)i);
    }
    std::shuffle(survivors.begin(), survivors.end(), rng);
    int probe = std::min<int>(5000, (int)survivors.size());
    long survivorMisses = 0;
    for (int j = 0; j < probe; ++j)
        if (t.search(rec[survivors[j]].id) < 0) ++survivorMisses;

    std::printf("deletions reported successful : %ld / %d\n", ok, nDel);
    std::printf("keys before / after           : %ld / %ld\n",
                beforeKeys, afterKeys);
    std::printf("nodes before / after          : %ld / %ld\n",
                beforeNodes, afterNodes);
    std::printf("height before / after         : %d / %d\n",
                beforeH, afterH);
    std::printf("merges / redistributes        : %ld / %ld\n",
                t.mergeCount(), t.redistCount());
    std::printf("deletion time                 : %.6f s   (%.3f us/op)\n",
                sec, (sec / nDel) * 1e6);
    std::printf("integrity: deleted-keys still present  = %ld  (expect 0)\n",
                missingHits);
    std::printf("integrity: surviving-keys missing      = %ld / %d  (expect 0)\n",
                survivorMisses, probe);
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    std::string csv = (argc > 1) ? argv[1] : "student.csv";

    std::cout << "Loading " << csv << " ...\n";
    auto t0 = clk::now();
    std::vector<Student> rec;
    if (!loadStudentsCSV(csv, rec)) {
        std::cerr << "Failed to load CSV.\n";
        return 1;
    }
    std::cout << "Loaded " << rec.size() << " records in "
              << secsSince(t0) << " s\n";

    expInsertion (rec);
    expPointSearch(rec, /*d=*/10, /*queries=*/10000);
    expRangeQuery (rec, /*d=*/10);
    expDeletion   (rec, /*d=*/10, /*nDel=*/2000);

    return 0;
}
