#ifndef BSTARTREE_H
#define BSTARTREE_H

#include <cstdint>
#include <utility>
#include <vector>

class BStarTree {
public:
    explicit BStarTree(int order);
    ~BStarTree();

    BStarTree(const BStarTree&)            = delete;
    BStarTree& operator=(const BStarTree&) = delete;

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

    void insertRec(Node* node, int key, int rid);
    void rebalanceChild(Node* parent, int idx);
    void redistributeWithLeft(Node* parent, int idx);
    void redistributeWithRight(Node* parent, int idx);
    void twoToThreeSplit(Node* parent, int leftIdx);
    void splitChildSimple(Node* parent, int idx);

    bool deleteFrom(Node* node, int key);
    void removeFromLeaf(Node* n, int idx);
    void removeFromInternal(Node* n, int idx);
    std::pair<int,int> getPredecessor(Node* n, int idx);
    void fillChild(Node* n, int idx);
    void borrowFromPrev(Node* n, int idx);
    void borrowFromNext(Node* n, int idx);
    void merge(Node* n, int idx);

    void rangeCollect(Node* n, int lo, int hi,
                      std::vector<std::pair<int,int>>& out) const;

    void freeSubtree(Node* n);
    int  heightOf(Node* n) const;
    long countNodes(Node* n) const;
    long countKeys(Node* n) const;
};

#endif
