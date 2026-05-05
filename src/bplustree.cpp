#include "bplustree.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>

BPlusTree::BPlusTree(int order) : d_(order) {
    if (order < 3) throw std::invalid_argument("B+-tree order must be >= 3");
    maxKeys_ = d_ - 1;
    minKeys_ = (d_ + 1) / 2 - 1;
    root_    = new Node{true, {}, {}, nullptr, {}};
}

BPlusTree::~BPlusTree() { freeSubtree(root_); }

void BPlusTree::freeSubtree(Node* n) {
    if (!n) return;
    if (!n->leaf) for (auto* c : n->ch) freeSubtree(c);
    delete n;
}

int BPlusTree::lowerBoundIdx(const std::vector<int>& v, int k) {
    return int(std::lower_bound(v.begin(), v.end(), k) - v.begin());
}
int BPlusTree::upperBoundIdx(const std::vector<int>& v, int k) {
    return int(std::upper_bound(v.begin(), v.end(), k) - v.begin());
}

int BPlusTree::leftmostKey(Node* n) const {
    while (!n->leaf) n = n->ch.front();
    return n->keys.front();
}

BPlusTree::Node* BPlusTree::findLeaf(int key) const {
    Node* n = root_;
    while (!n->leaf) {
        int i = upperBoundIdx(n->keys, key);
        n = n->ch[i];
    }
    return n;
}

int BPlusTree::search(int key) const {
    Node* leaf = findLeaf(key);
    int i = lowerBoundIdx(leaf->keys, key);
    if (i < (int)leaf->keys.size() && leaf->keys[i] == key) return leaf->rids[i];
    return -1;
}

void BPlusTree::insert(int key, int rid) {
    Push p = insertRec(root_, key, rid);
    if (p.happened) {
        Node* newRoot = new Node{false, { p.sepKey }, {}, nullptr, { root_, p.right }};
        root_ = newRoot;
    }
}

BPlusTree::Push BPlusTree::insertRec(Node* node, int key, int rid) {
    if (node->leaf) {
        int i = lowerBoundIdx(node->keys, key);
        if (i < (int)node->keys.size() && node->keys[i] == key) {
            node->rids[i] = rid;
            return {};
        }
        node->keys.insert(node->keys.begin() + i, key);
        node->rids.insert(node->rids.begin() + i, rid);
        if ((int)node->keys.size() > maxKeys_) return splitLeaf(node);
        return {};
    }

    int i = upperBoundIdx(node->keys, key);
    Push cr = insertRec(node->ch[i], key, rid);
    if (cr.happened) {
        node->keys.insert(node->keys.begin() + i,     cr.sepKey);
        node->ch  .insert(node->ch  .begin() + i + 1, cr.right);
        if ((int)node->keys.size() > maxKeys_) return splitInternal(node);
    }
    return {};
}

BPlusTree::Push BPlusTree::splitLeaf(Node* leaf) {
    int n   = (int)leaf->keys.size();
    int mid = n / 2;
    Node* right = new Node{true, {}, {}, leaf->next, {}};
    right->keys.assign(leaf->keys.begin() + mid, leaf->keys.end());
    right->rids.assign(leaf->rids.begin() + mid, leaf->rids.end());
    leaf->keys.resize(mid);
    leaf->rids.resize(mid);
    leaf->next = right;

    ++splits_;
    return { true, right->keys.front(), right };
}

BPlusTree::Push BPlusTree::splitInternal(Node* node) {
    int n   = (int)node->keys.size();
    int mid = n / 2;
    Node* right = new Node{false, {}, {}, nullptr, {}};
    right->keys.assign(node->keys.begin() + mid + 1, node->keys.end());
    right->ch  .assign(node->ch  .begin() + mid + 1, node->ch  .end());
    int sep = node->keys[mid];
    node->keys.resize(mid);
    node->ch  .resize(mid + 1);

    ++splits_;
    return { true, sep, right };
}

bool BPlusTree::remove(int key) {
    if (!root_ || root_->keys.empty()) return false;
    DelRet r = deleteRec(root_, key);
    if (!r.removed) return false;

    if (!root_->leaf && root_->keys.empty()) {
        Node* old = root_;
        root_ = root_->ch[0];
        old->ch.clear();
        delete old;
    }
    return true;
}

BPlusTree::DelRet BPlusTree::deleteRec(Node* node, int key) {
    if (node->leaf) {
        int i = lowerBoundIdx(node->keys, key);
        if (i == (int)node->keys.size() || node->keys[i] != key)
            return { false, false };
        node->keys.erase(node->keys.begin() + i);
        node->rids.erase(node->rids.begin() + i);

        return { true, (int)node->keys.size() < minKeys_ };
    }

    int idx = upperBoundIdx(node->keys, key);
    DelRet r = deleteRec(node->ch[idx], key);
    if (!r.removed) return { false, false };

    if (r.underflow) fixUnderflow(node, idx);

    for (int k = 0; k + 1 < (int)node->ch.size(); ++k)
        node->keys[k] = leftmostKey(node->ch[k + 1]);

    return { true, (int)node->keys.size() < minKeys_ };
}

void BPlusTree::fixUnderflow(Node* parent, int idx) {
    Node* child = parent->ch[idx];
    Node* lsib  = (idx > 0)                              ? parent->ch[idx-1] : nullptr;
    Node* rsib  = (idx + 1 < (int)parent->ch.size())     ? parent->ch[idx+1] : nullptr;

    if (lsib && (int)lsib->keys.size() > minKeys_) {
        if (child->leaf) borrowLeafFromPrev(parent, idx);
        else             borrowInternalFromPrev(parent, idx);
        return;
    }
    if (rsib && (int)rsib->keys.size() > minKeys_) {
        if (child->leaf) borrowLeafFromNext(parent, idx);
        else             borrowInternalFromNext(parent, idx);
        return;
    }

    if (lsib) {
        if (child->leaf) mergeLeaves(parent, idx - 1);
        else             mergeInternals(parent, idx - 1);
    } else {
        if (child->leaf) mergeLeaves(parent, idx);
        else             mergeInternals(parent, idx);
    }
}

void BPlusTree::borrowLeafFromPrev(Node* parent, int idx) {
    Node* child = parent->ch[idx];
    Node* lsib  = parent->ch[idx-1];

    child->keys.insert(child->keys.begin(), lsib->keys.back());
    child->rids.insert(child->rids.begin(), lsib->rids.back());
    lsib->keys.pop_back();
    lsib->rids.pop_back();
    parent->keys[idx-1] = child->keys.front();
    ++redists_;
}

void BPlusTree::borrowLeafFromNext(Node* parent, int idx) {
    Node* child = parent->ch[idx];
    Node* rsib  = parent->ch[idx+1];

    child->keys.push_back(rsib->keys.front());
    child->rids.push_back(rsib->rids.front());
    rsib->keys.erase(rsib->keys.begin());
    rsib->rids.erase(rsib->rids.begin());
    parent->keys[idx] = rsib->keys.front();
    ++redists_;
}

void BPlusTree::mergeLeaves(Node* parent, int idx) {
    Node* left  = parent->ch[idx];
    Node* right = parent->ch[idx+1];

    left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.end());
    left->rids.insert(left->rids.end(), right->rids.begin(), right->rids.end());
    left->next = right->next;

    parent->keys.erase(parent->keys.begin() + idx);
    parent->ch  .erase(parent->ch  .begin() + idx + 1);
    delete right;
    ++merges_;
}

void BPlusTree::borrowInternalFromPrev(Node* parent, int idx) {
    Node* child = parent->ch[idx];
    Node* lsib  = parent->ch[idx-1];

    child->keys.insert(child->keys.begin(), parent->keys[idx-1]);
    child->ch  .insert(child->ch  .begin(), lsib->ch.back());
    parent->keys[idx-1] = lsib->keys.back();
    lsib->keys.pop_back();
    lsib->ch  .pop_back();
    ++redists_;
}

void BPlusTree::borrowInternalFromNext(Node* parent, int idx) {
    Node* child = parent->ch[idx];
    Node* rsib  = parent->ch[idx+1];

    child->keys.push_back(parent->keys[idx]);
    child->ch  .push_back(rsib->ch.front());
    parent->keys[idx] = rsib->keys.front();
    rsib->keys.erase(rsib->keys.begin());
    rsib->ch  .erase(rsib->ch  .begin());
    ++redists_;
}

void BPlusTree::mergeInternals(Node* parent, int idx) {
    Node* left  = parent->ch[idx];
    Node* right = parent->ch[idx+1];

    left->keys.push_back(parent->keys[idx]);
    left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.end());
    left->ch  .insert(left->ch  .end(), right->ch  .begin(), right->ch  .end());

    parent->keys.erase(parent->keys.begin() + idx);
    parent->ch  .erase(parent->ch  .begin() + idx + 1);
    right->ch.clear();
    delete right;
    ++merges_;
}

std::vector<std::pair<int,int>> BPlusTree::rangeSearch(int lo, int hi) const {
    std::vector<std::pair<int,int>> out;
    if (lo > hi) return out;
    Node* leaf = findLeaf(lo);
    int i = lowerBoundIdx(leaf->keys, lo);
    while (leaf) {
        for (; i < (int)leaf->keys.size(); ++i) {
            if (leaf->keys[i] > hi) return out;
            out.emplace_back(leaf->keys[i], leaf->rids[i]);
        }
        leaf = leaf->next;
        i = 0;
    }
    return out;
}

int  BPlusTree::height()    const { return heightOf(root_); }
long BPlusTree::nodeCount() const { return countNodes(root_); }
long BPlusTree::keyCount()  const { return countKeys(root_); }

int BPlusTree::heightOf(Node* n) const {
    if (!n) return 0;
    int h = 1;
    while (!n->leaf) { n = n->ch.front(); ++h; }
    return h;
}
long BPlusTree::countNodes(Node* n) const {
    if (!n) return 0;
    long c = 1;
    if (!n->leaf) for (auto* k : n->ch) c += countNodes(k);
    return c;
}
long BPlusTree::countKeys(Node* n) const {
    if (!n) return 0;
    if (n->leaf) return (long)n->keys.size();
    long c = 0;
    for (auto* k : n->ch) c += countKeys(k);
    return c;
}
double BPlusTree::utilization() const {
    long nodes = nodeCount();
    if (nodes == 0) return 0.0;
    long slotsUsed = 0;

    std::vector<Node*> stack{ root_ };
    while (!stack.empty()) {
        Node* n = stack.back(); stack.pop_back();
        slotsUsed += (long)n->keys.size();
        if (!n->leaf) for (auto* c : n->ch) stack.push_back(c);
    }
    return (double)slotsUsed / (double)(nodes * maxKeys_);
}
