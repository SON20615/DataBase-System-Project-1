#include "btree.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>

BTree::BTree(int order) : d_(order) {
    if (order < 3) throw std::invalid_argument("B-tree order must be >= 3");
    maxKeys_ = d_ - 1;
    minKeys_ = (d_ + 1) / 2 - 1;
    root_    = new Node{true, {}, {}, {}};
}

BTree::~BTree() { freeSubtree(root_); }

void BTree::freeSubtree(Node* n) {
    if (!n) return;
    for (auto* c : n->ch) freeSubtree(c);
    delete n;
}

int BTree::lowerBoundIdx(const std::vector<int>& v, int k) {
    return int(std::lower_bound(v.begin(), v.end(), k) - v.begin());
}
int BTree::upperBoundIdx(const std::vector<int>& v, int k) {
    return int(std::upper_bound(v.begin(), v.end(), k) - v.begin());
}

int BTree::search(int key) const {
    Node* n = root_;
    while (n) {
        int i = lowerBoundIdx(n->keys, key);
        if (i < (int)n->keys.size() && n->keys[i] == key) return n->rids[i];
        if (n->leaf) return -1;
        n = n->ch[i];
    }
    return -1;
}

void BTree::insert(int key, int rid) {
    Split s = insertRec(root_, key, rid);
    if (s.happened) {
        Node* newRoot = new Node{false, { s.midKey }, { s.midRid }, { root_, s.right }};
        root_ = newRoot;
    }
}

BTree::Split BTree::insertRec(Node* node, int key, int rid) {
    int i = lowerBoundIdx(node->keys, key);

    if (i < (int)node->keys.size() && node->keys[i] == key) {
        node->rids[i] = rid;
        return {};
    }

    if (node->leaf) {
        node->keys.insert(node->keys.begin() + i, key);
        node->rids.insert(node->rids.begin() + i, rid);
    } else {
        Split cr = insertRec(node->ch[i], key, rid);
        if (cr.happened) {
            node->keys.insert(node->keys.begin() + i,     cr.midKey);
            node->rids.insert(node->rids.begin() + i,     cr.midRid);
            node->ch  .insert(node->ch  .begin() + i + 1, cr.right);
        }
    }

    if ((int)node->keys.size() > maxKeys_) return splitNode(node);
    return {};
}

BTree::Split BTree::splitNode(Node* node) {
    int mid = (int)node->keys.size() / 2;

    Node* right = new Node{ node->leaf, {}, {}, {} };
    right->keys.assign(node->keys.begin() + mid + 1, node->keys.end());
    right->rids.assign(node->rids.begin() + mid + 1, node->rids.end());
    if (!node->leaf) {
        right->ch.assign(node->ch.begin() + mid + 1, node->ch.end());
        node->ch.resize(mid + 1);
    }

    int midKey = node->keys[mid];
    int midRid = node->rids[mid];
    node->keys.resize(mid);
    node->rids.resize(mid);

    ++splits_;
    return { true, midKey, midRid, right };
}

bool BTree::remove(int key) {
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

bool BTree::deleteFrom(Node* node, int key) {
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

void BTree::removeFromLeaf(Node* n, int idx) {
    n->keys.erase(n->keys.begin() + idx);
    n->rids.erase(n->rids.begin() + idx);
}

void BTree::removeFromInternal(Node* n, int idx) {
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

std::pair<int,int> BTree::getPredecessor(Node* n, int idx) {
    Node* cur = n->ch[idx];
    while (!cur->leaf) cur = cur->ch.back();
    return { cur->keys.back(), cur->rids.back() };
}

void BTree::fillChild(Node* n, int idx) {
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

void BTree::borrowFromPrev(Node* n, int idx) {
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

void BTree::borrowFromNext(Node* n, int idx) {
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

void BTree::merge(Node* n, int idx) {
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

std::vector<std::pair<int,int>> BTree::rangeSearch(int lo, int hi) const {
    std::vector<std::pair<int,int>> out;
    if (lo > hi) return out;
    rangeCollect(root_, lo, hi, out);
    return out;
}

void BTree::rangeCollect(Node* n, int lo, int hi,
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

int  BTree::height()     const { return heightOf(root_); }
long BTree::nodeCount()  const { return countNodes(root_); }
long BTree::keyCount()   const { return countKeys(root_); }

int BTree::heightOf(Node* n) const {
    if (!n) return 0;
    int h = 1;
    while (!n->leaf) { n = n->ch.front(); ++h; }
    return h;
}
long BTree::countNodes(Node* n) const {
    if (!n) return 0;
    long c = 1;
    for (auto* k : n->ch) c += countNodes(k);
    return c;
}
long BTree::countKeys(Node* n) const {
    if (!n) return 0;
    long c = (long)n->keys.size();
    for (auto* k : n->ch) c += countKeys(k);
    return c;
}
double BTree::utilization() const {
    long nodes = nodeCount();
    if (nodes == 0) return 0.0;
    return (double)keyCount() / (double)(nodes * maxKeys_);
}
