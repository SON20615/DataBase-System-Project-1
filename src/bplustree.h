#ifndef BPLUSTREE_H
#define BPLUSTREE_H

#include <cstdint>
#include <utility>
#include <vector>

class BPlusTree {
public:
    explicit BPlusTree(int order);
    ~BPlusTree();

    BPlusTree(const BPlusTree&)            = delete;
    BPlusTree& operator=(const BPlusTree&) = delete;

    int  search(int key) const;

    void insert(int key, int rid);

    bool remove(int key);

    std::vector<std::pair<int,int>> rangeSearch(int lo, int hi) const;

    int    order()         const { return d_; }
    int    height()        const;
    long   nodeCount()     const;
    long   keyCount()      const;
    double utilization()   const;

    long   splitCount()    const { return splits_; }
    long   mergeCount()    const { return merges_; }
    long   redistCount()   const { return redists_; }

private:
    struct Node {
        bool                 leaf;
        std::vector<int>     keys;
        std::vector<int>     rids;
        Node*                next;
        std::vector<Node*>   ch;
    };

    Node* root_     = nullptr;
    int   d_;
    int   maxKeys_;
    int   minKeys_;

    mutable long splits_  = 0;
    mutable long merges_  = 0;
    mutable long redists_ = 0;

    static int  upperBoundIdx(const std::vector<int>& v, int k);
    static int  lowerBoundIdx(const std::vector<int>& v, int k);

    struct Push {
        bool  happened = false;
        int   sepKey   = 0;
        Node* right    = nullptr;
    };
    Push insertRec(Node* node, int key, int rid);
    Push splitLeaf(Node* leaf);
    Push splitInternal(Node* internal);

    struct DelRet {
        bool removed   = false;
        bool underflow = false;
    };
    DelRet deleteRec(Node* node, int key);
    void   fixUnderflow(Node* parent, int idx);
    void   borrowLeafFromPrev(Node* parent, int idx);
    void   borrowLeafFromNext(Node* parent, int idx);
    void   mergeLeaves(Node* parent, int idx);
    void   borrowInternalFromPrev(Node* parent, int idx);
    void   borrowInternalFromNext(Node* parent, int idx);
    void   mergeInternals(Node* parent, int idx);

    Node*  findLeaf(int key) const;
    int    leftmostKey(Node* n) const;

    void   freeSubtree(Node* n);
    int    heightOf(Node* n) const;
    long   countNodes(Node* n) const;
    long   countKeys(Node* n) const;
};

#endif
