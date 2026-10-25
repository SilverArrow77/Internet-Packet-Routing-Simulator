#include "dijkstra.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <queue>
#include <functional>
#include <limits>
#include <algorithm>

using namespace std;

bool NetworkGraph::loadFromFile(const string& filename, string& error) {
    adj.clear();
    error.clear();

    ifstream file(filename);

    if (!file.is_open()) {
        error = "Unable to open input file: " + filename;
        return false;
    }

    int n, m;

    if (!(file >> n >> m) || n <= 0 || m < 0) {
        error = "Invalid router or link count.";
        return false;
    }

    // The supplied dataset uses sequential IDs: R1, R2, ..., Rn.
    // Create all routers so isolated routers are preserved.
    for (int i = 1; i <= n; i++) {
        adj["R" + to_string(i)];
    }

    for (int i = 0; i < m; i++) {
        string u, v;
        int cost;

        if (!(file >> u >> v >> cost)) {
            error = "Missing or malformed link at line " +
                    to_string(i + 2) + ".";
            adj.clear();
            return false;
        }

        if (adj.find(u) == adj.end() ||
            adj.find(v) == adj.end()) {
            error = "Unknown router in link: " + u + " " + v;
            adj.clear();
            return false;
        }

        if (u == v || cost <= 0) {
            error = "Invalid link: self-loops and non-positive costs "
                    "are not allowed.";
            adj.clear();
            return false;
        }

        adj[u].push_back({v, cost});
        adj[v].push_back({u, cost});
    }

    string extra;

    if (file >> extra) {
        error = "Unexpected extra data after the declared links.";
        adj.clear();
        return false;
    }

    return true;
}

RouteResult NetworkGraph::shortestPath(
    const string& source,
    const string& destination) const {

    RouteResult result;
    result.reachable = false;
    result.totalCost = 0;

    if (!hasRouter(source) || !hasRouter(destination)) {
        return result;
    }

    const long long INF = numeric_limits<long long>::max();

    unordered_map<string, long long> dist;
    unordered_map<string, string> parent;

    for (const auto& entry : adj) {
        dist[entry.first] = INF;
    }

    using State = pair<long long, string>;

    priority_queue<State, vector<State>, greater<State>> pq;

    dist[source] = 0;
    pq.push({0, source});

    while (!pq.empty()) {
        long long currentCost = pq.top().first;
        string u = pq.top().second;
        pq.pop();

        if (currentCost != dist[u]) {
            continue;
        }

        if (u == destination) {
            break;
        }

        for (const Edge& edge : adj.at(u)) {
            long long newCost = currentCost + edge.cost;

            if (newCost < dist[edge.destination]) {
                dist[edge.destination] = newCost;
                parent[edge.destination] = u;
                pq.push({newCost, edge.destination});
            }
        }
    }

    if (dist[destination] == INF) {
        return result;
    }

    vector<string> path;
    string current = destination;

    while (current != source) {
        path.push_back(current);

        auto it = parent.find(current);

        if (it == parent.end()) {
            return result;
        }

        current = it->second;
    }

    path.push_back(source);
    reverse(path.begin(), path.end());

    result.reachable = true;
    result.totalCost = dist[destination];
    result.path = path;

    return result;
}

bool NetworkGraph::hasRouter(const string& router) const {
    return adj.find(router) != adj.end();
}

bool NetworkGraph::removeLink(
    const string& routerA,
    const string& routerB) {
    auto first = adj.find(routerA);
    auto second = adj.find(routerB);
    if (first == adj.end() || second == adj.end()) {
        return false;
    }

    auto removeDestination = [](vector<Edge>& edges, const string& target) {
        for (vector<Edge>::iterator edge = edges.begin();
             edge != edges.end();
             ++edge) {
            if (edge->destination == target) {
                edges.erase(edge);
                return true;
            }
        }
        return false;
    };

    bool removedForward = removeDestination(first->second, routerB);
    bool removedBackward = removeDestination(second->second, routerA);
    return removedForward && removedBackward;
}

int NetworkGraph::routerCount() const {
    return static_cast<int>(adj.size());
}

void NetworkGraph::displayGraph() const {
    for (const auto& entry : adj) {
        cout << entry.first << ": ";

        for (const Edge& edge : entry.second) {
            cout << "(" << edge.destination
                 << ", cost " << edge.cost << ") ";
        }

        cout << '\n';
    }
}