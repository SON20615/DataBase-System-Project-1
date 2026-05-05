
#include "btree.h"
#include "bplustree.h"
#include "bstartree.h"
#include "student.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <numeric>
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
    expInsertionOne<BStarTree>("B*-tree", rec);
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
    expPointSearchOne<BStarTree>("B*-tree", rec, d, q, queries);
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
    expRangeQueryOne<BStarTree>("B*-tree", rec, d, LO, HI);
    expRangeQueryOne<BPlusTree>("B+-tree", rec, d, LO, HI);
}

template <typename Tree>
static void expProgDeletionOne(const char* tag,
                               const std::vector<Student>& rec,
                               int d,
                               const std::vector<int>& delOrder,
                               int batches,
                               int probeQueriesPerBatch,
                               unsigned probeSeed) {
    Tree t(d);
    buildTree(t, rec);

    const int N        = (int)rec.size();
    const int totalDel = (int)delOrder.size();
    const int perBatch = totalDel / batches;

    long prevMerges = t.mergeCount();
    long prevRedist = t.redistCount();

    std::printf("\n--- %s ---\n", tag);
    std::printf("%-7s %-9s %-7s %-9s %-9s %-9s %-12s %-12s\n",
                "del%", "nodes", "height", "util", "+merges",
                "+redists", "delTime(us)", "srch(us/op)");
    std::printf("%-7s %-9ld %-7d %-9.4f %-9s %-9s %-12s %-12s\n",
                "0", t.nodeCount(), t.height(), t.utilization(),
                "-", "-", "-", "-");

    int firstMergeAt = -1;

    for (int b = 1; b <= batches; ++b) {
        int lo = (b - 1) * perBatch;
        int hi = b * perBatch;

        auto t0 = clk::now();
        for (int i = lo; i < hi; ++i) t.remove(rec[delOrder[i]].id);
        double delSec = secsSince(t0);

        std::mt19937 prng(probeSeed + (unsigned)b);
        std::uniform_int_distribution<int> pick(hi, totalDel - 1);
        std::vector<int> probes; probes.reserve(probeQueriesPerBatch);

        std::uniform_int_distribution<int> survPick(totalDel, N - 1);
        for (int q = 0; q < probeQueriesPerBatch; ++q) {
            int idx = (q & 1 && hi < totalDel)
                          ? delOrder[pick(prng)]
                          : survPick(prng);
            probes.push_back(rec[idx].id);
        }
        auto s0 = clk::now();
        long hits = 0;
        for (int k : probes) if (t.search(k) >= 0) ++hits;
        double srchSec = secsSince(s0);

        long curMerges = t.mergeCount();
        long curRedist = t.redistCount();

        long dMerges = curMerges - prevMerges;
        long dRedist = curRedist - prevRedist;
        if (dMerges > 0 && firstMergeAt < 0)
            firstMergeAt = (b * 100) / batches;

        std::printf("%-7d %-9ld %-7d %-9.4f %-9ld %-9ld %-12.1f %-12.3f\n",
                    (b * 100) / batches,
                    t.nodeCount(), t.height(), t.utilization(),
                    dMerges, dRedist,
                    delSec * 1e6,
                    (srchSec / probeQueriesPerBatch) * 1e6);

        (void)hits;
        prevMerges = curMerges;
        prevRedist = curRedist;
    }
    std::printf("first-merge-trigger : %s\n",
                firstMergeAt < 0 ? "none in sweep"
                                 : (std::to_string(firstMergeAt) + "%-deleted").c_str());
}

static void expProgDeletion(const std::vector<Student>& rec,
                            int d, int batches) {
    std::cout << "\n========== [4] PROGRESSIVE DELETION (d = " << d
              << ", " << batches << " batches up to "
              << (batches * (100 / batches)) << "%) ==========\n";

    std::mt19937 rng(2026);
    std::vector<int> order((size_t)rec.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = (int)i;
    std::shuffle(order.begin(), order.end(), rng);

    int totalDel = (int)((double)rec.size() * batches * 0.10);
    order.resize(totalDel);

    expProgDeletionOne<BTree    >("B-tree",  rec, d, order, batches, 1000, 7);
    expProgDeletionOne<BStarTree>("B*-tree", rec, d, order, batches, 1000, 7);
    expProgDeletionOne<BPlusTree>("B+-tree", rec, d, order, batches, 1000, 7);
}

template <typename Tree>
static void expInsOrderOne(const char* tag, int d,
                           const std::vector<Student>& rec,
                           const std::vector<int>& order,
                           const char* orderTag) {
    Tree t(d);
    auto t0 = clk::now();
    for (int i : order) t.insert(rec[i].id, i);
    double sec = secsSince(t0);
    std::printf("%-8s %-10s time=%.4fs h=%-3d nodes=%-7ld splits=%-7ld "
                "redists=%-7ld util=%.4f\n",
                tag, orderTag, sec, t.height(), t.nodeCount(),
                t.splitCount(), t.redistCount(), t.utilization());
}

static void expInsertionOrder(const std::vector<Student>& rec, int d) {
    std::cout << "\n========== [5] INSERTION-ORDER PATHOLOGY (d = "
              << d << ") ==========\n";

    const int N = (int)rec.size();
    std::vector<int> idx(N), idxRev(N), idxRand(N);
    for (int i = 0; i < N; ++i) idx[i] = i;

    std::vector<int> idxAsc = idx;
    std::sort(idxAsc.begin(), idxAsc.end(),
              [&](int a, int b){ return rec[a].id < rec[b].id; });

    idxRev = idxAsc;
    std::reverse(idxRev.begin(), idxRev.end());

    idxRand = idx;

    auto runOne = [&](auto trees, const char* otag, const std::vector<int>& ord){
        (void)trees;
        expInsOrderOne<BTree    >("B-tree",  d, rec, ord, otag);
        expInsOrderOne<BStarTree>("B*-tree", d, rec, ord, otag);
        expInsOrderOne<BPlusTree>("B+-tree", d, rec, ord, otag);
    };
    runOne(0, "ascending",  idxAsc);
    runOne(0, "descending", idxRev);
    runOne(0, "random",     idxRand);
}

static void expRedistEffectiveness(const std::vector<Student>& rec, int d) {
    std::cout << "\n========== [7] B*-TREE REDISTRIBUTION EFFECTIVENESS"
                 " (d = " << d << ") ==========\n";

    BStarTree t(d);
    long prevR = 0, prevT3 = 0, prevS = 0;
    const int chunk = 10000;

    std::printf("%-6s %-10s %-10s %-9s %-9s %-9s %-9s %-9s %-7s\n",
                "chunk", "inserts", "+overflow", "+redist",
                "+2-to-3", "+1-to-2", "redist%", "util", "height");

    for (size_t i = 0; i < rec.size(); ++i) {
        t.insert(rec[i].id, (int)i);
        if ((i + 1) % chunk == 0) {
            long curR  = t.redistCount();
            long curT3 = t.twoToThreeSplitCount();
            long curS  = t.simpleSplitCount();
            long dR    = curR  - prevR;
            long dT3   = curT3 - prevT3;
            long dS    = curS  - prevS;
            long dOv   = dR + dT3 + dS;
            double pct = dOv ? (100.0 * dR / dOv) : 0.0;

            std::printf("%-6zu %-10zu %-10ld %-9ld %-9ld %-9ld %-8.2f%% %-9.4f %-7d\n",
                        (i + 1) / chunk, i + 1,
                        dOv, dR, dT3, dS, pct,
                        t.utilization(), t.height());

            prevR = curR; prevT3 = curT3; prevS = curS;
        }
    }

    BTree bt(d);
    for (size_t i = 0; i < rec.size(); ++i) bt.insert(rec[i].id, (int)i);

    std::printf("\nbaseline   B-tree  splits=%-7ld  util=%.4f  height=%d\n",
                bt.splitCount(), bt.utilization(), bt.height());
    std::printf("           B*-tree redists=%-6ld 2-to-3=%-5ld 1-to-2=%-5ld "
                "(splits=%ld)  util=%.4f  height=%d\n",
                t.redistCount(), t.twoToThreeSplitCount(),
                t.simpleSplitCount(), t.splitCount(),
                t.utilization(), t.height());
    std::printf("           B*-tree avoided %ld split events through redistribution\n",
                t.redistCount());
    long btNew = bt.splitCount();
    long stNew = t.twoToThreeSplitCount() + t.simpleSplitCount();
    std::printf("           net new nodes:  B-tree=%ld  vs  B*-tree=%ld  "
                "(%.1f%% reduction)\n",
                btNew, stNew, 100.0 * (btNew - stNew) / std::max(1L, btNew));
}

template <typename Tree>
static void expTailLatencyOne(const char* tag, Tree& t,
                              const std::vector<int>& queries) {
    const int Q = (int)queries.size();
    std::vector<long long> lat(Q);

    volatile long sink = 0;
    for (int i = 0; i < 1000; ++i) sink ^= t.search(queries[i % Q]);
    (void)sink;

    for (int i = 0; i < Q; ++i) {
        auto t0 = clk::now();
        volatile int r = t.search(queries[i]);
        auto t1 = clk::now();
        lat[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        (void)r;
    }

    std::sort(lat.begin(), lat.end());
    auto pct = [&](double p) {
        int idx = std::min(Q - 1, (int)(Q * p));
        return lat[idx];
    };
    double mean = std::accumulate(lat.begin(), lat.end(), 0.0) / Q;
    double var  = 0.0;
    for (auto x : lat) var += (x - mean) * (x - mean);
    double sd   = std::sqrt(var / Q);
    double p99_50 = pct(0.50) > 0 ? (double)pct(0.99) / pct(0.50) : 0.0;

    std::printf("%-8s mean=%5.0f sd=%5.0f  min=%-5lld p50=%-5lld p90=%-5lld "
                "p95=%-5lld p99=%-6lld p99.9=%-6lld max=%-7lld  p99/p50=%.2fx\n",
                tag, mean, sd, lat.front(), pct(0.50), pct(0.90),
                pct(0.95), pct(0.99), pct(0.999), lat.back(), p99_50);
}

static void expTailLatency(const std::vector<Student>& rec, int d, int Q) {
    std::cout << "\n========== [8] TAIL LATENCY DISTRIBUTION (d = " << d
              << ", queries = " << Q << ") ==========\n";

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> pick(0, (int)rec.size() - 1);
    std::vector<int> queries; queries.reserve(Q);
    for (int i = 0; i < Q; ++i) queries.push_back(rec[pick(rng)].id);

    BTree     bt(d);  buildTree(bt, rec);
    BStarTree st(d);  buildTree(st, rec);
    BPlusTree pt(d);  buildTree(pt, rec);

    std::printf("(latencies in nanoseconds; clock resolution ~40 ns on macOS)\n");
    expTailLatencyOne("B-tree",  bt, queries);
    expTailLatencyOne("B*-tree", st, queries);
    expTailLatencyOne("B+-tree", pt, queries);
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

    expInsertion    (rec);
    expPointSearch  (rec, 10, 10000);
    expRangeQuery   (rec, 10);
    expProgDeletion (rec, 10, 9);
    expInsertionOrder(rec, 10);
    expRedistEffectiveness(rec, 10);
    expTailLatency  (rec, 10, 100000);

    return 0;
}
