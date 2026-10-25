#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "common.h"
#include "Dijkstra/dijkstra.h"
#include "Member2_Trie/trie.h"
#include "Member3_PacketHeap/packet_heap.h"
#include "Member4_RouterQueue_DSU/dsu.h"
#include "Member4_RouterQueue_DSU/router_queue.h"

using namespace std;

namespace {

const int ROUTER_QUEUE_CAPACITY = 10;

bool loadRoutingConfig(
    const string& filename,
    const NetworkGraph& graph,
    unordered_map<string, RoutingEntry>& entries,
    Trie& destinationLookup,
    string& error) {
    ifstream file(filename);
    if (!file.is_open()) {
        error = "Unable to open routing configuration: " + filename;
        return false;
    }

    entries.clear();
    destinationLookup = Trie();
    string line;
    int lineNumber = 0;

    while (getline(file, line)) {
        ++lineNumber;
        if (line.empty()) {
            continue;
        }

        istringstream row(line);
        string router;
        RoutingEntry entry;
        string extra;

        if (!(row >> router >> entry.prefix >> entry.prefixLength) ||
            (row >> extra)) {
            error = "Malformed routing configuration at line " +
                    to_string(lineNumber) + ".";
            return false;
        }

        if (!graph.hasRouter(router)) {
            error = "Unknown router in routing configuration at line " +
                    to_string(lineNumber) + ": " + router;
            return false;
        }
        if (entry.prefixLength < 0 || entry.prefixLength > 32) {
            error = "Invalid prefix length at routing configuration line " +
                    to_string(lineNumber) + ".";
            return false;
        }
        if (entries.find(router) != entries.end()) {
            error = "Duplicate destination router in routing configuration: " +
                    router;
            return false;
        }

        entry.nextHop = router;
        try {
            destinationLookup.insert(
                entry.prefix, entry.prefixLength, router);
        } catch (const invalid_argument& exception) {
            error = "Invalid routing prefix at line " +
                    to_string(lineNumber) + ": " + exception.what();
            return false;
        }
        entries[router] = entry;
    }

    if (entries.empty()) {
        error = "Routing configuration contains no destinations.";
        return false;
    }

    return true;
}

bool buildForwardingTables(
    const NetworkGraph& graph,
    const unordered_map<string, RoutingEntry>& entries,
    DSU& connectivity,
    string& error) {
    routingTables.clear();
    connectivity.reset(graph.routerCount());

    for (int sourceId = 1; sourceId <= graph.routerCount(); ++sourceId) {
        const string source = "R" + to_string(sourceId);
        if (!graph.hasRouter(source)) {
            error = "Topology must use sequential router IDs R1 through Rn.";
            return false;
        }

        routingTables[source];
        for (int destinationId = 1;
             destinationId <= graph.routerCount();
             ++destinationId) {
            const string destination = "R" + to_string(destinationId);
            RouteResult route = graph.shortestPath(source, destination);
            if (!route.reachable) {
                continue;
            }

            connectivity.unite(sourceId - 1, destinationId - 1);

            auto config = entries.find(destination);
            if (config != entries.end() && route.path.size() > 1) {
                try {
                    routingTables[source].insert(
                        config->second.prefix,
                        config->second.prefixLength,
                        route.path[1]);
                } catch (const invalid_argument& exception) {
                    error = "Unable to build forwarding table for " +
                            source + ": " + exception.what();
                    return false;
                }
            }
        }
    }

    return true;
}

bool loadPackets(
    const string& filename,
    const NetworkGraph& graph,
    const unordered_map<string, RoutingEntry>& destinations,
    const Trie& destinationLookup,
    vector<Packet>& packets,
    string& error) {
    ifstream file(filename);
    if (!file.is_open()) {
        error = "Unable to open packet file: " + filename;
        return false;
    }

    packets.clear();
    unordered_map<int, bool> seenIds;
    string line;
    int lineNumber = 0;

    while (getline(file, line)) {
        ++lineNumber;
        if (line.empty()) {
            continue;
        }

        istringstream row(line);
        Packet packet;
        string extra;
        if (!(row >> packet.id >> packet.sourceRouter >>
              packet.destinationRouter >> packet.destinationIP >>
              packet.priority) ||
            (row >> extra)) {
            error = "Malformed packet at line " + to_string(lineNumber) +
                    " in " + filename + ".";
            return false;
        }

        if (seenIds.find(packet.id) != seenIds.end()) {
            error = "Duplicate packet ID " + to_string(packet.id) +
                    " in " + filename + ".";
            return false;
        }
        if (!graph.hasRouter(packet.sourceRouter) ||
            !graph.hasRouter(packet.destinationRouter)) {
            error = "Packet " + to_string(packet.id) +
                    " references an unknown router.";
            return false;
        }
        if (destinations.find(packet.destinationRouter) == destinations.end()) {
            error = "No routing configuration for destination router " +
                    packet.destinationRouter + ".";
            return false;
        }

        try {
            const string mappedRouter =
                destinationLookup.longestPrefixMatch(packet.destinationIP);
            if (mappedRouter.empty() ||
                mappedRouter != packet.destinationRouter) {
                error = "Packet " + to_string(packet.id) +
                        " destination IP does not map to " +
                        packet.destinationRouter + ".";
                return false;
            }
        } catch (const invalid_argument& exception) {
            error = "Invalid destination IP for packet " +
                    to_string(packet.id) + ": " + exception.what();
            return false;
        }

        packet.currentRouter = packet.sourceRouter;
        seenIds[packet.id] = true;
        packets.push_back(packet);
    }

    if (packets.empty()) {
        error = "No packets were found in " + filename + ".";
        return false;
    }

    return true;
}

bool simulatePackets(
    const NetworkGraph& graph,
    const vector<Packet>& packets,
    vector<PacketResult>& results) {
    const int routerCount = graph.routerCount();
    vector<RouterQueue> queues;
    queues.reserve(routerCount);
    for (int i = 0; i < routerCount; ++i) {
        queues.emplace_back(ROUTER_QUEUE_CAPACITY);
    }

    unordered_map<int, size_t> resultByPacketId;
    results.clear();
    results.reserve(packets.size());

    for (const Packet& packet : packets) {
        PacketResult result;
        result.packetId = packet.id;
        result.delivered = false;
        result.finalRouter = packet.sourceRouter;
        result.totalRouteCost = 0;
        result.routeTaken.push_back(packet.sourceRouter);
        result.message = "Waiting for processing";
        resultByPacketId[packet.id] = results.size();
        results.push_back(result);

        const int sourceId = stoi(packet.sourceRouter.substr(1)) - 1;
        if (!queues[sourceId].enqueue(packet)) {
            results.back().message =
                "Dropped: source router queue is full.";
        }
    }

    bool packetsRemain = true;
    while (packetsRemain) {
        packetsRemain = false;

        for (int routerId = 0; routerId < routerCount; ++routerId) {
            RouterQueue& queue = queues[routerId];
            if (queue.isEmpty()) {
                continue;
            }
            packetsRemain = true;

            PacketHeap scheduler;
            Packet packet;
            while (queue.dequeue(packet)) {
                scheduler.push(packet);
            }

            while (!scheduler.empty()) {
                packet = scheduler.pop();
                PacketResult& result =
                    results[resultByPacketId.at(packet.id)];

                if (packet.currentRouter == packet.destinationRouter) {
                    result.delivered = true;
                    result.finalRouter = packet.currentRouter;
                    result.message = "Delivered";
                    continue;
                }

                auto table = routingTables.find(packet.currentRouter);
                if (table == routingTables.end()) {
                    result.finalRouter = packet.currentRouter;
                    result.message =
                        "Dropped: no forwarding table for current router.";
                    continue;
                }

                string nextHop;
                try {
                    nextHop = table->second.longestPrefixMatch(
                        packet.destinationIP);
                } catch (const invalid_argument& exception) {
                    result.finalRouter = packet.currentRouter;
                    result.message =
                        "Dropped: invalid destination IP: " +
                        string(exception.what());
                    continue;
                }

                if (nextHop.empty() || !graph.hasRouter(nextHop)) {
                    result.finalRouter = packet.currentRouter;
                    result.message =
                        "Dropped: no route matches the destination IP.";
                    continue;
                }

                RouteResult hopRoute =
                    graph.shortestPath(packet.currentRouter, nextHop);
                if (!hopRoute.reachable || hopRoute.path.size() != 2) {
                    result.finalRouter = packet.currentRouter;
                    result.message =
                        "Dropped: forwarding table contains an invalid next hop.";
                    continue;
                }

                packet.currentRouter = nextHop;
                result.totalRouteCost += hopRoute.totalCost;
                result.routeTaken.push_back(nextHop);
                result.finalRouter = nextHop;

                const int nextRouterId = stoi(nextHop.substr(1)) - 1;
                if (!queues[nextRouterId].enqueue(packet)) {
                    result.message =
                        "Dropped: router queue is full at " + nextHop + ".";
                } else {
                    result.message = "In transit";
                }
            }
        }
    }

    return true;
}

void printPacketResults(const vector<PacketResult>& results) {
    cout << "\nPacket simulation results\n";
    cout << "------------------------\n";
    for (const PacketResult& result : results) {
        cout << "Packet " << result.packetId << ": "
             << (result.delivered ? "DELIVERED" : "NOT DELIVERED")
             << "\n  Route: ";
        for (size_t i = 0; i < result.routeTaken.size(); ++i) {
            if (i > 0) {
                cout << " -> ";
            }
            cout << result.routeTaken[i];
        }
        cout << "\n  Cost: " << result.totalRouteCost
             << "\n  Final router: " << result.finalRouter
             << "\n  Result: " << result.message << '\n';
    }
}

bool readInteger(const string& prompt, int& value) {
    while (true) {
        cout << prompt;
        if (cin >> value) {
            return true;
        }
        if (cin.eof()) {
            return false;
        }

        cout << "Please enter a whole number.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

void printRouterChoices(int routerCount) {
    cout << "Available routers: ";
    for (int i = 1; i <= routerCount; ++i) {
        if (i > 1) {
            cout << ", ";
        }
        cout << "R" << i;
    }
    cout << '\n';
}

void showConnectivity(DSU& connectivity, int routerCount) {
    cout << "Connected components: " << connectivity.getComponentCount()
         << '\n';
    for (int source = 1; source <= routerCount; ++source) {
        cout << "R" << source << " is connected to: ";
        bool first = true;
        for (int destination = 1; destination <= routerCount; ++destination) {
            if (connectivity.connected(source - 1, destination - 1)) {
                if (!first) {
                    cout << ", ";
                }
                cout << "R" << destination;
                first = false;
            }
        }
        cout << '\n';
    }
}

}  // namespace

int main() {
    const string topologyFile = "input.txt";
    const string configFile = "routing_config.txt";

    NetworkGraph graph;
    string error;
    if (!graph.loadFromFile(topologyFile, error)) {
        cerr << "Topology error: " << error << '\n';
        return 1;
    }

    unordered_map<string, RoutingEntry> destinations;
    Trie destinationLookup;
    if (!loadRoutingConfig(
            configFile, graph, destinations, destinationLookup, error)) {
        cerr << "Routing configuration error: " << error << '\n';
        return 1;
    }

    DSU connectivity(graph.routerCount());
    if (!buildForwardingTables(graph, destinations, connectivity, error)) {
        cerr << "Forwarding table error: " << error << '\n';
        return 1;
    }

    cout << "Network Packet Routing Simulator\n"
         << "Loaded " << graph.routerCount() << " routers and "
         << destinations.size() << " destination networks.\n";

    while (true) {
        cout << "\n1. Simulate packets from packets.txt\n"
             << "2. Send a packet interactively\n"
             << "3. Display network topology\n"
             << "4. Display router connectivity\n"
             << "5. Find a shortest route\n"
             << "6. Exit\n";

        int choice;
        if (!readInteger("Choose an option: ", choice)) {
            cout << '\n';
            break;
        }

        if (choice == 1) {
            vector<Packet> packets;
            if (!loadPackets(
                    "packets.txt", graph, destinations,
                    destinationLookup, packets, error)) {
                cerr << "Packet input error: " << error << '\n';
                continue;
            }

            vector<PacketResult> results;
            simulatePackets(graph, packets, results);
            printPacketResults(results);
        } else if (choice == 2) {
            int packetId;
            int priority;
            string source;
            string destinationIP;
            if (!readInteger("Packet ID: ", packetId) ||
                !readInteger("Priority (larger values run first): ", priority)) {
                cout << '\n';
                break;
            }

            printRouterChoices(graph.routerCount());
            cout << "Source router: ";
            if (!(cin >> source)) {
                break;
            }
            cout << "Destination IPv4 address: ";
            if (!(cin >> destinationIP)) {
                break;
            }

            if (!graph.hasRouter(source)) {
                cout << "Unknown source router: " << source << '\n';
                continue;
            }

            string destination;
            try {
                destination =
                    destinationLookup.longestPrefixMatch(destinationIP);
            } catch (const invalid_argument& exception) {
                cout << "Invalid destination IP: " << exception.what() << '\n';
                continue;
            }
            if (destination.empty()) {
                cout << "No configured network matches that destination IP.\n";
                continue;
            }

            Packet packet;
            packet.id = packetId;
            packet.sourceRouter = source;
            packet.destinationRouter = destination;
            packet.destinationIP = destinationIP;
            packet.currentRouter = source;
            packet.priority = priority;

            vector<PacketResult> results;
            simulatePackets(graph, vector<Packet>(1, packet), results);
            printPacketResults(results);
        } else if (choice == 3) {
            graph.displayGraph();
        } else if (choice == 4) {
            showConnectivity(connectivity, graph.routerCount());
        } else if (choice == 5) {
            string source;
            string destination;
            printRouterChoices(graph.routerCount());
            cout << "Source router: ";
            if (!(cin >> source)) {
                break;
            }
            cout << "Destination router: ";
            if (!(cin >> destination)) {
                break;
            }

            if (!graph.hasRouter(source) || !graph.hasRouter(destination)) {
                cout << "Please choose valid router IDs from the list.\n";
                continue;
            }

            RouteResult route = graph.shortestPath(source, destination);
            if (!route.reachable) {
                cout << "No route exists from " << source << " to "
                     << destination << ".\n";
                continue;
            }

            cout << "Shortest route (cost " << route.totalCost << "): ";
            for (size_t i = 0; i < route.path.size(); ++i) {
                if (i > 0) {
                    cout << " -> ";
                }
                cout << route.path[i];
            }
            cout << '\n';
        } else if (choice == 6) {
            cout << "Goodbye.\n";
            break;
        } else {
            cout << "Choose an option from 1 to 6.\n";
        }
    }

    return 0;
}
