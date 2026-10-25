#include "dsu.h"

DSU::DSU(int numberOfRouters) {
    reset(numberOfRouters);
}

int DSU::find(int routerId) {
    if (routerId < 0 || routerId >= static_cast<int>(parent.size())) {
        return -1;
    }

    if (parent[routerId] != routerId) {
        parent[routerId] = find(parent[routerId]); // Path compression
    }

    return parent[routerId];
}

void DSU::unite(int routerA, int routerB) {
    int rootA = find(routerA);
    int rootB = find(routerB);

    if (rootA == -1 || rootB == -1 || rootA == rootB) {
        return;
    }

    // Union by rank
    if (rank[rootA] < rank[rootB]) {
        parent[rootA] = rootB;
    } else if (rank[rootA] > rank[rootB]) {
        parent[rootB] = rootA;
    } else {
        parent[rootB] = rootA;
        rank[rootA]++;
    }

    components--;
}

bool DSU::connected(int routerA, int routerB) {
    int rootA = find(routerA);
    int rootB = find(routerB);

    if (rootA == -1 || rootB == -1) {
        return false;
    }

    return rootA == rootB;
}

int DSU::getComponentCount() const {
    return components;
}

void DSU::reset(int numberOfRouters) {
    int n = numberOfRouters > 0 ? numberOfRouters : 0;
    parent.resize(n);
    rank.assign(n, 0);
    components = n;

    for (int i = 0; i < n; ++i) {
        parent[i] = i;
    }
}
