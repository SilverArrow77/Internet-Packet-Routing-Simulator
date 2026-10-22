
#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include <string>
#include <vector>
#include <unordered_map>

using namespace std;

struct Edge {
    string destination;
    int cost;
};

struct RouteResult {
    bool reachable;
    long long totalCost;
    vector<string> path;
};

class NetworkGraph {
private:
    unordered_map<string, vector<Edge>> adj;

public:
    bool loadFromFile(const string& filename, string& error);

    RouteResult shortestPath(const string& source,
                             const string& destination) const;

    bool hasRouter(const string& router) const;

    int routerCount() const;

    void displayGraph() const;
};

#endif
