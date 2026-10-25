#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

#include "common.h"
#include "integration/network_simulator.h"

using namespace std;

void printBanner() {
    cout << "=================================================================\n";
    cout << "         INTERNET PACKET ROUTING SIMULATOR (INTEGRATED)          \n";
    cout << "=================================================================\n";
    cout << " Modules:\n";
    cout << "   [Member 1] Graph Topology + Dijkstra Shortest Path\n";
    cout << "   [Member 2] Binary Trie + Longest Prefix Match Forwarding\n";
    cout << "   [Member 3] Max-Heap Priority Packet Scheduling\n";
    cout << "   [Member 4] Bounded Circular Router Queue + DSU Connectivity\n";
    cout << "=================================================================\n\n";
}

void printSummaryTable(const vector<PacketResult>& results) {
    cout << "\n===================================================================================================\n";
    cout << "                                    SIMULATION RESULTS SUMMARY                                     \n";
    cout << "===================================================================================================\n";
    cout << left << setw(10) << "Packet ID"
         << setw(12) << "Delivered"
         << setw(12) << "Total Cost"
         << setw(35) << "Route Traversed"
         << "Message\n";
    cout << "---------------------------------------------------------------------------------------------------\n";

    int deliveredCount = 0;
    int droppedCount = 0;

    for (const auto& res : results) {
        string pathStr;
        for (size_t i = 0; i < res.routeTaken.size(); ++i) {
            if (i > 0) pathStr += " -> ";
            pathStr += res.routeTaken[i];
        }

        cout << left << setw(10) << ("P" + to_string(res.packetId))
             << setw(12) << (res.delivered ? "YES" : "NO")
             << setw(12) << (res.delivered ? to_string(res.totalRouteCost) : "-")
             << setw(35) << (pathStr.empty() ? "-" : pathStr)
             << res.message << "\n";

        if (res.delivered) deliveredCount++;
        else droppedCount++;
    }

    cout << "===================================================================================================\n";
    cout << " Total Packets: " << results.size()
         << " | Delivered: " << deliveredCount
         << " | Dropped/Failed: " << droppedCount << "\n";
    cout << "===================================================================================================\n";
}

int main(int argc, char* argv[]) {
    string topoFile   = (argc > 1) ? argv[1] : "input.txt";
    string configFile = (argc > 2) ? argv[2] : "routing_config.txt";
    string packetFile = (argc > 3) ? argv[3] : "packets.txt";

    printBanner();

    NetworkSimulator sim;
    string error;

    cout << "[Step 1] Loading network topology from: " << topoFile << "\n";
    if (!sim.loadTopology(topoFile, error)) {
        cerr << "Error: " << error << "\n";
        return 1;
    }
    cout << "         -> Success! Loaded " << sim.getRouterCount() << " routers.\n\n";

    cout << "[Step 2] Loading routing configuration from: " << configFile << "\n";
    if (!sim.loadRoutingConfig(configFile, error)) {
        cerr << "Error: " << error << "\n";
        return 1;
    }
    cout << "         -> Success! Loaded destination subnet configurations.\n\n";

    cout << "[Step 3] Building forwarding tables in Tries using Dijkstra routes...\n";
    sim.buildForwardingTables();
    cout << "         -> Success! Trie forwarding tables generated for all routers.\n\n";

    cout << "[Step 4] Loading packet workload from: " << packetFile << "\n";
    vector<Packet> packets = sim.loadPacketsFromFile(packetFile, error);
    if (!error.empty()) {
        cerr << "Error: " << error << "\n";
        return 1;
    }
    cout << "         -> Success! Loaded " << packets.size() << " packets.\n\n";

    cout << "[Step 5] Simulating packet forwarding...\n";
    vector<PacketResult> results = sim.simulateBatch(packets, true);

    printSummaryTable(results);

    return 0;
}
