#ifndef NETWORK_SIMULATOR_H
#define NETWORK_SIMULATOR_H

#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>

#include "../common.h"
#include "../Dijkstra/dijkstra.h"
#include "../Member2_Trie/trie.h"
#include "../Member3_PacketHeap/packet_heap.h"
#include "../Member4_RouterQueue_DSU/router_queue.h"
#include "../Member4_RouterQueue_DSU/dsu.h"

using namespace std;

struct NetworkPrefixInfo {
    string router;
    string networkAddress;
    int prefixLength;
};

class NetworkSimulator {
private:
    NetworkGraph graph;
    DSU dsu;
    unordered_map<string, RouterQueue> routerQueues;
    unordered_map<string, NetworkPrefixInfo> destinationPrefixes; // router -> prefix info

    unordered_map<string, int> routerToIndex;
    vector<string> indexToRouter;

    int defaultQueueCapacity;
    vector<pair<pair<string, string>, int>> topologyLinks;

    int getOrAssignRouterIndex(const string& router);

public:
    explicit NetworkSimulator(int queueCapacity = 10);

    // Initialization
    bool loadTopology(const string& topologyFile, string& error);
    bool loadRoutingConfig(const string& configFile, string& error);
    void buildForwardingTables();

    // High-level combined init
    bool initialize(const string& topologyFile,
                    const string& configFile,
                    string& error,
                    int queueCapacity = 10);

    // Dynamic topology / failure operations
    bool simulateLinkFailure(const string& u, const string& v);
    bool simulateLinkRestore(const string& u, const string& v, int cost);
    void rebuildDSU();

    // Packet simulation
    PacketResult simulatePacket(Packet packet, bool verbose = false);
    vector<PacketResult> simulateBatch(const vector<Packet>& packets, bool verbose = false);
    vector<Packet> loadPacketsFromFile(const string& packetsFile, string& error);

    // Status queries
    bool isConnected(const string& routerA, const string& routerB);
    int getRouterCount() const;
    vector<string> getRouters() const;
    const NetworkGraph& getGraph() const { return graph; }
    RouterQueue& getRouterQueue(const string& router) { return routerQueues[router]; }
    void setRouterQueueCapacity(const string& router, int capacity);
    void resetQueues();
};

#endif
