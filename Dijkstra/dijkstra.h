
#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include "../common.h"

#include <string>
#include <vector>
#include <unordered_map>

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

    bool hasRouter(const string& router) const;

    bool removeLink(const string& routerA, const string& routerB);

    int routerCount() const;

    void displayGraph() const;
};

#endif
