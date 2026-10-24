#ifndef MEMBER4_DSU_H
#define MEMBER4_DSU_H

class DSU {
public:
    explicit DSU(int numberOfRouters);

    int find(int routerId);
    void unite(int routerA, int routerB);
    bool connected(int routerA, int routerB);
};

#endif
