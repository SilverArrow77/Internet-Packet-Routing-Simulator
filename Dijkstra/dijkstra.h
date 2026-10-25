#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include <string>
#include <vector>
#include <unordered_map>

#if __has_include("../common.h")
#include "../common.h"
#elif __has_include("common.h")
#include "common.h"
#endif

using namespace std;

struct Edge {
    string destination;
    int cost;
};

class NetworkGraph {
private:
    unordered_map<string, vector<Edge>> adj;

public:
    bool loadFromFile(const string& filename, string& error);

    RouteResult shortestPath(const string& source,
                             const string& destination) const;

    string getNextHop(const string& source,
                      const string& destination) const;

    int getEdgeCost(const string& source,
                    const string& destination) const;

    bool removeEdge(const string& u, const string& v);

    bool hasRouter(const string& router) const;

    int routerCount() const;

    vector<string> getAllRouters() const;

    const unordered_map<string, vector<Edge>>& getAdjList() const { return adj; }

    void displayGraph() const;
};

#endif
