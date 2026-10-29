#ifndef MEMBER4_DSU_H
#define MEMBER4_DSU_H

#include <vector>

using namespace std;

class DSU {
private:
    vector<int> parent;
    vector<int> rank;
    int components;

public:
    explicit DSU(int numberOfRouters = 0);

    int find(int routerId);
    void unite(int routerA, int routerB);
    bool connected(int routerA, int routerB);

    int getComponentCount() const;
    void reset(int numberOfRouters);
};

#endif
