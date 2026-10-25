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
        State current = pq.top();
        pq.pop();

        long long currentDist = current.first;
        string u = current.second;

        if (currentDist > dist[u]) {
            continue;
        }

        if (u == destination) {
            break;
        }

        auto it = adj.find(u);

        if (it == adj.end()) {
            continue;
        }

        for (const Edge& edge : it->second) {
            long long newCost = currentDist + edge.cost;

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

string NetworkGraph::getNextHop(const string& source,
                                const string& destination) const {
    if (source == destination) {
        return "";
    }

    RouteResult result = shortestPath(source, destination);

    if (!result.reachable || result.path.size() < 2) {
        return "";
    }

    return result.path[1];
}

int NetworkGraph::getEdgeCost(const string& source,
                              const string& destination) const {
    auto it = adj.find(source);
    if (it == adj.end()) {
        return -1;
    }

    for (const Edge& edge : it->second) {
        if (edge.destination == destination) {
            return edge.cost;
        }
    }

    return -1;
}

bool NetworkGraph::removeEdge(const string& u, const string& v) {
    bool removed = false;

    auto itU = adj.find(u);
    if (itU != adj.end()) {
        for (auto it = itU->second.begin(); it != itU->second.end(); ++it) {
            if (it->destination == v) {
                itU->second.erase(it);
                removed = true;
                break;
            }
        }
    }

    auto itV = adj.find(v);
    if (itV != adj.end()) {
        for (auto it = itV->second.begin(); it != itV->second.end(); ++it) {
            if (it->destination == u) {
                itV->second.erase(it);
                removed = true;
                break;
            }
        }
    }

    return removed;
}

bool NetworkGraph::hasRouter(const string& router) const {
    return adj.find(router) != adj.end();
}

int NetworkGraph::routerCount() const {
    return static_cast<int>(adj.size());
}

vector<string> NetworkGraph::getAllRouters() const {
    vector<string> routers;
    for (const auto& entry : adj) {
        routers.push_back(entry.first);
    }
    sort(routers.begin(), routers.end(), [](const string& a, const string& b) {
        if (a.size() != b.size()) return a.size() < b.size();
        return a < b;
    });
    return routers;
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
