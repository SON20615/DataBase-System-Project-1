#include "bstartree.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>

BStarTree::BStarTree(int order) : d_(order) {
    if (order < 3) throw std::invalid_argument("B*-tree order must be >= 3");
    maxKeys_ = d_ - 1;
    minKeys_ = (d_ + 1) / 2 - 1;
    root_    = new Node{true, {}, {}, {}};
}

BStarTree::~BStarTree() { freeSubtree(root_); }

void BStarTree::freeSubtree(Node* n) {
    if (!n) return;
    for (auto* c : n->ch) freeSubtree(c);
    delete n;
}

int BStarTree::lowerBoundIdx(const std::vector<int>& v, int k) {
    return int(std::lower_bound(v.begin(), v.end(), k) - v.begin());
}
int BStarTree::upperBoundIdx(const std::vector<int>& v, int k) {
    return int(std::upper_bound(v.begin(), v.end(), k) - v.begin());
}

int BStarTree::search(int key) const {
    Node* n = root_;
    while (n) {
        int i = lowerBoundIdx(n->keys, key);
        if (i < (int)n->keys.size() && n->keys[i] == key) return n->rids[i];
        if (n->leaf) return -1;
        n = n->ch[i];
    }
    return -1;
}

void BStarTree::insert(int key, int rid) {
    insertRec(root_, key, rid);
    if ((int)root_->keys.size() > maxKeys_) {
        Node* old = root_;
        Node* newRoot = new Node{false, {}, {}, { old }};
        splitChildSimple(newRoot, 0);
        root_ = newRoot;
    }
}

void BStarTree::insertRec(Node* node, int key, int rid) {
    int i = lowerBoundIdx(node->keys, key);

    if (i < (int)node->keys.size() && node->keys[i] == key) {
        node->rids[i] = rid;
        return;
    }

    if (node->leaf) {
        node->keys.insert(node->keys.begin() + i, key);
        node->rids.insert(node->rids.begin() + i, rid);
        return;
    }

    insertRec(node->ch[i], key, rid);
    if ((int)node->ch[i]->keys.size() > maxKeys_) rebalanceChild(node, i);
}

void BStarTree::rebalanceChild(Node* parent, int idx) {
    Node* lsib = (idx > 0)                          ? parent->ch[idx-1] : nullptr;
    Node* rsib = (idx + 1 < (int)parent->ch.size()) ? parent->ch[idx+1] : nullptr;

    if (lsib && (int)lsib->keys.size() < maxKeys_) {
        redistributeWithLeft(parent, idx);
        return;
    }
    if (rsib && (int)rsib->keys.size() < maxKeys_) {
        redistributeWithRight(parent, idx);
        return;
    }
    if (lsib)      twoToThreeSplit(parent, idx - 1);
    else if (rsib) twoToThreeSplit(parent, idx);
    else           splitChildSimple(parent, idx);
}

namespace {
struct Slab {
    std::vector<int>            keys;
    std::vector<int>            rids;
    std::vector<void*>          ch;
    bool                        leaf;
};
}

void BStarTree::redistributeWithLeft(Node* parent, int idx) {
    Node* a = parent->ch[idx-1];
    Node* b = parent->ch[idx];

    std::vector<int> K = a->keys, R = a->rids;
    K.push_back(parent->keys[idx-1]);
    R.push_back(parent->rids[idx-1]);
    K.insert(K.end(), b->keys.begin(), b->keys.end());
    R.insert(R.end(), b->rids.begin(), b->rids.end());

    std::vector<Node*> C;
    if (!a->leaf) {
        C = a->ch;
        C.insert(C.end(), b->ch.begin(), b->ch.end());
    }

    int total = (int)K.size();
    int leftSize = total / 2;

    a->keys.assign(K.begin(), K.begin() + leftSize);
    a->rids.assign(R.begin(), R.begin() + leftSize);
    parent->keys[idx-1] = K[leftSize];
    parent->rids[idx-1] = R[leftSize];
    b->keys.assign(K.begin() + leftSize + 1, K.end());
    b->rids.assign(R.begin() + leftSize + 1, R.end());

    if (!a->leaf) {
        a->ch.assign(C.begin(), C.begin() + leftSize + 1);
        b->ch.assign(C.begin() + leftSize + 1, C.end());
    }
    ++redists_;
}

void BStarTree::redistributeWithRight(Node* parent, int idx) {
    Node* a = parent->ch[idx];
    Node* b = parent->ch[idx+1];

    std::vector<int> K = a->keys, R = a->rids;
    K.push_back(parent->keys[idx]);
    R.push_back(parent->rids[idx]);
    K.insert(K.end(), b->keys.begin(), b->keys.end());
    R.insert(R.end(), b->rids.begin(), b->rids.end());

    std::vector<Node*> C;
    if (!a->leaf) {
        C = a->ch;
        C.insert(C.end(), b->ch.begin(), b->ch.end());
    }

    int total = (int)K.size();
    int leftSize = total / 2;

    a->keys.assign(K.begin(), K.begin() + leftSize);
    a->rids.assign(R.begin(), R.begin() + leftSize);
    parent->keys[idx] = K[leftSize];
    parent->rids[idx] = R[leftSize];
    b->keys.assign(K.begin() + leftSize + 1, K.end());
    b->rids.assign(R.begin() + leftSize + 1, R.end());

    if (!a->leaf) {
        a->ch.assign(C.begin(), C.begin() + leftSize + 1);
        b->ch.assign(C.begin() + leftSize + 1, C.end());
    }
    ++redists_;
}

void BStarTree::twoToThreeSplit(Node* parent, int leftIdx) {
    Node* a = parent->ch[leftIdx];
    Node* b = parent->ch[leftIdx + 1];

    std::vector<int> K = a->keys, R = a->rids;
    K.push_back(parent->keys[leftIdx]);
    R.push_back(parent->rids[leftIdx]);
    K.insert(K.end(), b->keys.begin(), b->keys.end());
    R.insert(R.end(), b->rids.begin(), b->rids.end());

    std::vector<Node*> C;
    if (!a->leaf) {
        C = a->ch;
        C.insert(C.end(), b->ch.begin(), b->ch.end());
    }

    int total = (int)K.size();
    int m1 = (total - 2) / 3;
    int m2 = (2 * total - 1) / 3;
    if (m1 < 1) m1 = 1;
    if (m2 <= m1) m2 = m1 + 1;
    if (m2 >= total - 1) m2 = total - 2;

    Node* c = new Node{ a->leaf, {}, {}, {} };

    a->keys.assign(K.begin(),                K.begin() + m1);
    a->rids.assign(R.begin(),                R.begin() + m1);
    b->keys.assign(K.begin() + m1 + 1,       K.begin() + m2);
    b->rids.assign(R.begin() + m1 + 1,       R.begin() + m2);
    c->keys.assign(K.begin() + m2 + 1,       K.end());
    c->rids.assign(R.begin() + m2 + 1,       R.end());

    if (!a->leaf) {
        a->ch.assign(C.begin(),              C.begin() + m1 + 1);
        b->ch.assign(C.begin() + m1 + 1,     C.begin() + m2 + 1);
        c->ch.assign(C.begin() + m2 + 1,     C.end());
    }

    parent->keys[leftIdx] = K[m1];
    parent->rids[leftIdx] = R[m1];
    parent->keys.insert(parent->keys.begin() + leftIdx + 1, K[m2]);
    parent->rids.insert(parent->rids.begin() + leftIdx + 1, R[m2]);
    parent->ch  .insert(parent->ch  .begin() + leftIdx + 2, c);

    ++splits_;
}

void BStarTree::splitChildSimple(Node* parent, int idx) {
    Node* node = parent->ch[idx];
    int mid = (int)node->keys.size() / 2;

    Node* right = new Node{ node->leaf, {}, {}, {} };
    right->keys.assign(node->keys.begin() + mid + 1, node->keys.end());
    right->rids.assign(node->rids.begin() + mid + 1, node->rids.end());
    if (!node->leaf) {
        right->ch.assign(node->ch.begin() + mid + 1, node->ch.end());
        node->ch.resize(mid + 1);
    }
    int sepK = node->keys[mid], sepR = node->rids[mid];
    node->keys.resize(mid);
    node->rids.resize(mid);

    parent->keys.insert(parent->keys.begin() + idx,     sepK);
    parent->rids.insert(parent->rids.begin() + idx,     sepR);
    parent->ch  .insert(parent->ch  .begin() + idx + 1, right);
    ++splits_;
}

bool BStarTree::remove(int key) {
    if (!root_ || root_->keys.empty()) return false;
    bool ok = deleteFrom(root_, key);
    if (!root_->leaf && root_->keys.empty()) {
        Node* old = root_;
        root_ = root_->ch[0];
        old->ch.clear();
        delete old;
    }
    return ok;
}

bool BStarTree::deleteFrom(Node* node, int key) {
    int idx = lowerBoundIdx(node->keys, key);
    bool found = (idx < (int)node->keys.size() && node->keys[idx] == key);

    if (found) {
        if (node->leaf) { removeFromLeaf(node, idx);     return true; }
        else            { removeFromInternal(node, idx); return true; }
    }
    if (node->leaf) return false;

    bool lastChild = (idx == (int)node->keys.size());
    if ((int)node->ch[idx]->keys.size() <= minKeys_) fillChild(node, idx);

    if (lastChild && idx > (int)node->keys.size())
        return deleteFrom(node->ch[idx - 1], key);
    return deleteFrom(node->ch[idx], key);
}

void BStarTree::removeFromLeaf(Node* n, int idx) {
    n->keys.erase(n->keys.begin() + idx);
    n->rids.erase(n->rids.begin() + idx);
}

void BStarTree::removeFromInternal(Node* n, int idx) {
    int k = n->keys[idx];
    if ((int)n->ch[idx]->keys.size() > minKeys_) {
        auto pred = getPredecessor(n, idx);
        n->keys[idx] = pred.first;
        n->rids[idx] = pred.second;
        deleteFrom(n->ch[idx], pred.first);
    } else if ((int)n->ch[idx + 1]->keys.size() > minKeys_) {
        Node* cur = n->ch[idx + 1];
        while (!cur->leaf) cur = cur->ch.front();
        int succK = cur->keys.front();
        int succR = cur->rids.front();
        n->keys[idx] = succK;
        n->rids[idx] = succR;
        deleteFrom(n->ch[idx + 1], succK);
    } else {
        merge(n, idx);
        deleteFrom(n->ch[idx], k);
    }
}

std::pair<int,int> BStarTree::getPredecessor(Node* n, int idx) {
    Node* cur = n->ch[idx];
    while (!cur->leaf) cur = cur->ch.back();
    return { cur->keys.back(), cur->rids.back() };
}

void BStarTree::fillChild(Node* n, int idx) {
    if (idx > 0 && (int)n->ch[idx - 1]->keys.size() > minKeys_) {
        borrowFromPrev(n, idx);
    } else if (idx < (int)n->keys.size() &&
               (int)n->ch[idx + 1]->keys.size() > minKeys_) {
        borrowFromNext(n, idx);
    } else {
        if (idx == (int)n->keys.size()) merge(n, idx - 1);
        else                            merge(n, idx);
    }
}

void BStarTree::borrowFromPrev(Node* n, int idx) {
    Node* child = n->ch[idx];
    Node* left  = n->ch[idx - 1];

    child->keys.insert(child->keys.begin(), n->keys[idx - 1]);
    child->rids.insert(child->rids.begin(), n->rids[idx - 1]);
    if (!child->leaf) {
        child->ch.insert(child->ch.begin(), left->ch.back());
        left->ch.pop_back();
    }
    n->keys[idx - 1] = left->keys.back();
    n->rids[idx - 1] = left->rids.back();
    left->keys.pop_back();
    left->rids.pop_back();
    ++redists_;
}

void BStarTree::borrowFromNext(Node* n, int idx) {
    Node* child = n->ch[idx];
    Node* right = n->ch[idx + 1];

    child->keys.push_back(n->keys[idx]);
    child->rids.push_back(n->rids[idx]);
    if (!child->leaf) {
        child->ch.push_back(right->ch.front());
        right->ch.erase(right->ch.begin());
    }
    n->keys[idx] = right->keys.front();
    n->rids[idx] = right->rids.front();
    right->keys.erase(right->keys.begin());
    right->rids.erase(right->rids.begin());
    ++redists_;
}

void BStarTree::merge(Node* n, int idx) {
    Node* left  = n->ch[idx];
    Node* right = n->ch[idx + 1];

    left->keys.push_back(n->keys[idx]);
    left->rids.push_back(n->rids[idx]);
    left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.end());
    left->rids.insert(left->rids.end(), right->rids.begin(), right->rids.end());
    if (!left->leaf)
        left->ch.insert(left->ch.end(), right->ch.begin(), right->ch.end());

    n->keys.erase(n->keys.begin() + idx);
    n->rids.erase(n->rids.begin() + idx);
    n->ch  .erase(n->ch  .begin() + idx + 1);
    right->ch.clear();
    delete right;
    ++merges_;
}

std::vector<std::pair<int,int>> BStarTree::rangeSearch(int lo, int hi) const {
    std::vector<std::pair<int,int>> out;
    if (lo > hi) return out;
    rangeCollect(root_, lo, hi, out);
    return out;
}

void BStarTree::rangeCollect(Node* n, int lo, int hi,
                             std::vector<std::pair<int,int>>& out) const {
    if (!n) return;
    int i = lowerBoundIdx(n->keys, lo);
    while (i < (int)n->keys.size() && n->keys[i] <= hi) {
        if (!n->leaf) rangeCollect(n->ch[i], lo, hi, out);
        out.emplace_back(n->keys[i], n->rids[i]);
        ++i;
    }
    if (!n->leaf && i < (int)n->ch.size()) rangeCollect(n->ch[i], lo, hi, out);
}

int  BStarTree::height()    const { return heightOf(root_); }
long BStarTree::nodeCount() const { return countNodes(root_); }
long BStarTree::keyCount()  const { return countKeys(root_); }

int BStarTree::heightOf(Node* n) const {
    if (!n) return 0;
    int h = 1;
    while (!n->leaf) { n = n->ch.front(); ++h; }
    return h;
}
long BStarTree::countNodes(Node* n) const {
    if (!n) return 0;
    long c = 1;
    for (auto* k : n->ch) c += countNodes(k);
    return c;
}
long BStarTree::countKeys(Node* n) const {
    if (!n) return 0;
    long c = (long)n->keys.size();
    for (auto* k : n->ch) c += countKeys(k);
    return c;
}
double BStarTree::utilization() const {
    long nodes = nodeCount();
    if (nodes == 0) return 0.0;
    return (double)keyCount() / (double)(nodes * maxKeys_);
}
