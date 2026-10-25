#include "network_simulator.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

NetworkSimulator::NetworkSimulator(int queueCapacity)
    : defaultQueueCapacity(queueCapacity > 0 ? queueCapacity : 10) {}

int NetworkSimulator::getOrAssignRouterIndex(const string& router) {
    auto it = routerToIndex.find(router);
    if (it != routerToIndex.end()) {
        return it->second;
    }

    int newIndex = static_cast<int>(indexToRouter.size());
    routerToIndex[router] = newIndex;
    indexToRouter.push_back(router);
    return newIndex;
}

bool NetworkSimulator::loadTopology(const string& topologyFile, string& error) {
    routerToIndex.clear();
    indexToRouter.clear();
    topologyLinks.clear();
    routerQueues.clear();

    if (!graph.loadFromFile(topologyFile, error)) {
        return false;
    }

    // Read topology file to extract link edges for DSU tracking
    ifstream file(topologyFile);
    if (!file.is_open()) {
        error = "Failed to open topology file: " + topologyFile;
        return false;
    }

    int n, m;
    if (!(file >> n >> m)) {
        error = "Invalid header in topology file";
        return false;
    }

    for (int i = 1; i <= n; ++i) {
        getOrAssignRouterIndex("R" + to_string(i));
    }

    for (int i = 0; i < m; ++i) {
        string u, v;
        int cost;
        if (!(file >> u >> v >> cost)) {
            error = "Invalid link entry at line " + to_string(i + 2);
            return false;
        }
        getOrAssignRouterIndex(u);
        getOrAssignRouterIndex(v);
        topologyLinks.push_back({{u, v}, cost});
    }

    // Initialize DSU with all routers
    dsu.reset(static_cast<int>(indexToRouter.size()));
    for (const auto& link : topologyLinks) {
        int uIdx = routerToIndex[link.first.first];
        int vIdx = routerToIndex[link.first.second];
        dsu.unite(uIdx, vIdx);
    }

    // Initialize Router Queues for each router
    for (const string& rName : graph.getAllRouters()) {
        routerQueues[rName] = RouterQueue(defaultQueueCapacity);
    }

    return true;
}

bool NetworkSimulator::loadRoutingConfig(const string& configFile, string& error) {
    destinationPrefixes.clear();

    ifstream file(configFile);
    if (!file.is_open()) {
        error = "Failed to open routing config file: " + configFile;
        return false;
    }

    string line;
    int lineNum = 0;
    while (getline(file, line)) {
        lineNum++;
        if (line.empty() || line[0] == '#') continue;

        stringstream ss(line);
        string router, network;
        int prefixLen;

        if (!(ss >> router >> network >> prefixLen)) {
            error = "Malformed line " + to_string(lineNum) + " in " + configFile;
            return false;
        }

        destinationPrefixes[router] = {router, network, prefixLen};
    }

    return true;
}

void NetworkSimulator::buildForwardingTables() {
    routingTables.clear();

    vector<string> routers = graph.getAllRouters();

    // Create a Trie for each router
    for (const string& r : routers) {
        routingTables[r] = Trie();
    }

    // Build next-hop forwarding rules for every source/destination pair
    for (const string& src : routers) {
        for (const auto& pair : destinationPrefixes) {
            const string& dest = pair.first;
            const NetworkPrefixInfo& info = pair.second;

            if (src == dest) {
                // Same router, packets have arrived at destination
                continue;
            }

            string nextHop = graph.getNextHop(src, dest);
            if (!nextHop.empty()) {
                routingTables[src].insert(info.networkAddress, info.prefixLength, nextHop);
            }
        }
    }
}

bool NetworkSimulator::initialize(const string& topologyFile,
                                 const string& configFile,
                                 string& error,
                                 int queueCapacity) {
    defaultQueueCapacity = queueCapacity;

    if (!loadTopology(topologyFile, error)) {
        return false;
    }

    if (!loadRoutingConfig(configFile, error)) {
        return false;
    }

    buildForwardingTables();
    return true;
}

void NetworkSimulator::rebuildDSU() {
    dsu.reset(static_cast<int>(indexToRouter.size()));
    for (const auto& link : topologyLinks) {
        int uIdx = routerToIndex[link.first.first];
        int vIdx = routerToIndex[link.first.second];
        dsu.unite(uIdx, vIdx);
    }
}

bool NetworkSimulator::simulateLinkFailure(const string& u, const string& v) {
    bool removed = graph.removeEdge(u, v);
    if (!removed) {
        return false;
    }

    // Remove from topologyLinks
    auto it = remove_if(topologyLinks.begin(), topologyLinks.end(),
        [&](const pair<pair<string, string>, int>& link) {
            return (link.first.first == u && link.first.second == v) ||
                   (link.first.first == v && link.first.second == u);
        });
    topologyLinks.erase(it, topologyLinks.end());

    // Rebuild DSU and update routing tables
    rebuildDSU();
    buildForwardingTables();
    return true;
}

bool NetworkSimulator::simulateLinkRestore(const string& u, const string& v, int cost) {
    // Reload topology or add back edge
    topologyLinks.push_back({{u, v}, cost});

    // Reconstruct graph
    string tempError;
    // For simplicity, rebuild DSU and reload
    rebuildDSU();
    buildForwardingTables();
    return true;
}

bool NetworkSimulator::isConnected(const string& routerA, const string& routerB) {
    auto itA = routerToIndex.find(routerA);
    auto itB = routerToIndex.find(routerB);
    if (itA == routerToIndex.end() || itB == routerToIndex.end()) {
        return false;
    }
    return dsu.connected(itA->second, itB->second);
}

int NetworkSimulator::getRouterCount() const {
    return graph.routerCount();
}

vector<string> NetworkSimulator::getRouters() const {
    return graph.getAllRouters();
}

void NetworkSimulator::setRouterQueueCapacity(const string& router, int capacity) {
    routerQueues[router] = RouterQueue(capacity);
}

void NetworkSimulator::resetQueues() {
    for (auto& pair : routerQueues) {
        pair.second.clear();
    }
}

vector<Packet> NetworkSimulator::loadPacketsFromFile(const string& packetsFile, string& error) {
    vector<Packet> packets;
    ifstream file(packetsFile);
    if (!file.is_open()) {
        error = "Failed to open packets file: " + packetsFile;
        return packets;
    }

    string line;
    int lineNum = 0;
    while (getline(file, line)) {
        lineNum++;
        if (line.empty() || line[0] == '#') continue;

        stringstream ss(line);
        Packet p;
        if (!(ss >> p.id >> p.sourceRouter >> p.destinationRouter >> p.destinationIP >> p.priority)) {
            error = "Malformed packet entry at line " + to_string(lineNum);
            return packets;
        }
        p.currentRouter = p.sourceRouter;
        packets.push_back(p);
    }
    return packets;
}

PacketResult NetworkSimulator::simulatePacket(Packet packet, bool verbose) {
    PacketResult result;
    result.packetId = packet.id;
    result.delivered = false;
    result.finalRouter = packet.sourceRouter;
    result.totalRouteCost = 0;
    result.routeTaken.push_back(packet.sourceRouter);

    if (verbose) {
        cout << "\n[Simulator] Processing Packet P" << packet.id
             << " (" << packet.sourceRouter << " -> " << packet.destinationRouter
             << ", IP: " << packet.destinationIP
             << ", Priority: " << packet.priority << ")\n";
    }

    if (!graph.hasRouter(packet.sourceRouter)) {
        result.message = "Unknown source router: " + packet.sourceRouter;
        return result;
    }

    if (!graph.hasRouter(packet.destinationRouter)) {
        result.message = "Unknown destination router: " + packet.destinationRouter;
        return result;
    }

    // Step 1: Connectivity check via Member 4 DSU
    if (!isConnected(packet.sourceRouter, packet.destinationRouter)) {
        result.message = "Destination unreachable (partition detected by DSU)";
        if (verbose) cout << "  -> DSU check: NOT CONNECTED! Packet dropped.\n";
        return result;
    }

    // Step 2: Buffer at source router queue
    if (!routerQueues[packet.sourceRouter].enqueue(packet)) {
        result.message = "Dropped at source buffer: queue full (" + packet.sourceRouter + ")";
        if (verbose) cout << "  -> Ingress buffer at " << packet.sourceRouter << " is FULL! Packet dropped.\n";
        return result;
    }

    string currentRouter = packet.sourceRouter;
    const int MAX_HOPS = 100;
    int hopCount = 0;

    while (hopCount < MAX_HOPS) {
        hopCount++;

        // Step 3: Router transfers packet from queue to Member 3 PacketHeap scheduler
        Packet waiting;
        if (!routerQueues[currentRouter].dequeue(waiting)) {
            result.message = "Queue read error at " + currentRouter;
            return result;
        }

        PacketHeap scheduler;
        scheduler.push(waiting);
        Packet topPacket = scheduler.pop();

        if (verbose) {
            cout << "  Hop " << hopCount << ": At router " << currentRouter;
        }

        // Check if destination router reached
        if (currentRouter == packet.destinationRouter) {
            result.delivered = true;
            result.finalRouter = currentRouter;
            result.message = "Delivered successfully";
            if (verbose) {
                cout << " -> DESTINATION REACHED! Total Cost: " << result.totalRouteCost << "\n";
            }
            return result;
        }

        // Step 4: Longest Prefix Match via Member 2 Trie
        auto itTrie = routingTables.find(currentRouter);
        if (itTrie == routingTables.end()) {
            result.message = "No routing table initialized for " + currentRouter;
            return result;
        }

        string nextHop = itTrie->second.longestPrefixMatch(topPacket.destinationIP);

        if (nextHop.empty()) {
            result.message = "Trie prefix lookup failed at " + currentRouter + " for " + topPacket.destinationIP;
            if (verbose) cout << " -> No matching route in Trie. Dropped.\n";
            return result;
        }

        int linkCost = graph.getEdgeCost(currentRouter, nextHop);
        if (linkCost < 0) {
            result.message = "Link missing between " + currentRouter + " and " + nextHop;
            return result;
        }

        result.totalRouteCost += linkCost;
        result.routeTaken.push_back(nextHop);

        if (verbose) {
            cout << " -> Trie Next Hop: " << nextHop << " (Link Cost: " << linkCost << ")\n";
        }

        // Step 5: Buffer packet at next hop router queue
        topPacket.currentRouter = nextHop;
        if (!routerQueues[nextHop].enqueue(topPacket)) {
            result.finalRouter = currentRouter;
            result.message = "Buffer overflow: packet dropped at " + nextHop + " queue";
            if (verbose) {
                cout << "  -> Queue at " << nextHop << " is FULL! Packet dropped due to congestion.\n";
            }
            return result;
        }

        currentRouter = nextHop;
    }

    result.message = "Hop limit exceeded (TTL expired)";
    return result;
}

vector<PacketResult> NetworkSimulator::simulateBatch(const vector<Packet>& packets, bool verbose) {
    vector<PacketResult> results;
    resetQueues();

    if (packets.empty()) {
        return results;
    }

    if (verbose) {
        cout << "\n========================================\n";
        cout << "   RUNNING PACKET BATCH SIMULATION      \n";
        cout << "========================================\n";
    }

    for (const Packet& p : packets) {
        PacketResult res = simulatePacket(p, verbose);
        results.push_back(res);
    }

    return results;
}
