#include "btree.h"
#include "bplustree.h"
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

template <typename Tree>
static void buildTree(Tree& t, const std::vector<Student>& rec) {
    for (size_t i = 0; i < rec.size(); ++i)
        t.insert(rec[i].id, (int)i);
}

template <typename Tree>
static void expInsertionOne(const char* tag, const std::vector<Student>& rec) {
    std::cout << "\n----- " << tag << " -----\n";
    std::printf("%-4s %-12s %-7s %-9s %-10s %-10s %-12s\n",
                "d", "time(s)", "height", "nodes", "splits",
                "redists", "utilisation");
    for (int d : { 3, 5, 10, 50, 100 }) {
        Tree t(d);
        auto t0 = clk::now();
        buildTree(t, rec);
        double sec = secsSince(t0);
        std::printf("%-4d %-12.4f %-7d %-9ld %-10ld %-10ld %-12.4f\n",
                    d, sec, t.height(), t.nodeCount(),
                    t.splitCount(), t.redistCount(), t.utilization());
    }
}
static void expInsertion(const std::vector<Student>& rec) {
    std::cout << "\n========== [1] INSERTION & PARAMETER TUNING ==========\n";
    std::cout << "records = " << rec.size() << "\n";
    expInsertionOne<BTree    >("B-tree",  rec);
    expInsertionOne<BPlusTree>("B+-tree", rec);
}

template <typename Tree>
static void expPointSearchOne(const char* tag,
                              const std::vector<Student>& rec,
                              int d, int q,
                              const std::vector<int>& queries) {
    Tree t(d);
    buildTree(t, rec);

    auto t0 = clk::now();
    long hits = 0;
    for (int k : queries) if (t.search(k) >= 0) ++hits;
    double sec = secsSince(t0);

    std::printf("%-8s hits = %ld/%d   total = %.6f s   mean = %.3f us\n",
                tag, hits, q, sec, (sec / q) * 1e6);
}
static void expPointSearch(const std::vector<Student>& rec, int d, int q) {
    std::cout << "\n========== [2] POINT SEARCH (d = " << d
              << ", queries = " << q << ") ==========\n";

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> pick(0, (int)rec.size() - 1);
    std::vector<int> queries; queries.reserve(q);
    for (int i = 0; i < q; ++i) queries.push_back(rec[pick(rng)].id);

    expPointSearchOne<BTree    >("B-tree",  rec, d, q, queries);
    expPointSearchOne<BPlusTree>("B+-tree", rec, d, q, queries);
}

template <typename Tree>
static void expRangeQueryOne(const char* tag,
                             const std::vector<Student>& rec,
                             int d, int LO, int HI) {
    Tree t(d);
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

    std::printf("%-8s hits=%-6zu male=%-5ld avgGPA=%.3f avgH=%.2fcm  "
                "scan=%.6fs  agg=%.6fs  total=%.6fs\n",
                tag, rows.size(), cnt,
                cnt ? sumGpa/cnt : 0.0, cnt ? sumH/cnt : 0.0,
                scanSec, aggSec, scanSec + aggSec);
}
static void expRangeQuery(const std::vector<Student>& rec, int d) {
    std::cout << "\n========== [3] RANGE QUERY (d = " << d << ") ==========\n";
    constexpr int LO = 202000000;
    constexpr int HI = 202100000;
    std::printf("range : [%d, %d]\n", LO, HI);
    expRangeQueryOne<BTree    >("B-tree",  rec, d, LO, HI);
    expRangeQueryOne<BPlusTree>("B+-tree", rec, d, LO, HI);
}

template <typename Tree>
static void expDeletionOne(const char* tag,
                           const std::vector<Student>& rec,
                           int d, const std::vector<int>& delIdx) {
    Tree t(d);
    buildTree(t, rec);

    long beforeNodes = t.nodeCount();
    int  beforeH    = t.height();

    auto t0 = clk::now();
    long ok = 0;
    for (int i : delIdx) if (t.remove(rec[i].id)) ++ok;
    double sec = secsSince(t0);

    long afterNodes = t.nodeCount();
    int  afterH    = t.height();

    long missingHits = 0;
    for (int i : delIdx) if (t.search(rec[i].id) >= 0) ++missingHits;

    std::printf("%-8s ok=%ld/%zu  nodes %ld->%ld  h %d->%d  "
                "merges=%ld  redists=%ld  time=%.6fs (%.3f us/op)  "
                "deleted-still-present=%ld\n",
                tag, ok, delIdx.size(), beforeNodes, afterNodes,
                beforeH, afterH, t.mergeCount(), t.redistCount(),
                sec, (sec / delIdx.size()) * 1e6, missingHits);
}
static void expDeletion(const std::vector<Student>& rec, int d, int nDel) {
    std::cout << "\n========== [4] DELETION (d = " << d
              << ", delete " << nDel << " records) ==========\n";

    std::mt19937 rng(2026);
    std::vector<int> idx((size_t)rec.size());
    for (size_t i = 0; i < idx.size(); ++i) idx[i] = (int)i;
    std::shuffle(idx.begin(), idx.end(), rng);
    idx.resize(nDel);

    expDeletionOne<BTree    >("B-tree",  rec, d, idx);
    expDeletionOne<BPlusTree>("B+-tree", rec, d, idx);
}

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
    expPointSearch(rec, 10, 10000);
    expRangeQuery (rec, 10);
    expDeletion   (rec, 10, 2000);

    return 0;
}
