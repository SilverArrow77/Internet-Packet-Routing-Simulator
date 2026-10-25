#include <fstream>
#include <algorithm>
#include <iostream>
#include <limits>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
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

struct TopologyLink {
    string source;
    string target;
    int cost;
};

struct DemoEvent {
    string type;
    string router;
    string from;
    string to;
    string action;
    string failedSource;
    string failedTarget;
    int packetId;
    int queueSize;
    int capacity;
    int cost;
    long long oldCost;
    long long newCost;
    vector<string> oldPath;
    vector<string> newPath;
};

struct StressDemo {
    Packet packet;
    RouteResult initialRoute;
    RouteResult changedRoute;
    string failedSource;
    string failedTarget;
    vector<DemoEvent> events;
};

bool loadTopologyLinks(
    const string& filename,
    vector<TopologyLink>& links,
    string& error) {
    ifstream file(filename);
    if (!file.is_open()) {
        error = "Unable to open topology file: " + filename;
        return false;
    }

    int routerCount;
    int linkCount;
    if (!(file >> routerCount >> linkCount)) {
        error = "Invalid topology header in " + filename + ".";
        return false;
    }

    links.clear();
    for (int i = 0; i < linkCount; ++i) {
        TopologyLink link;
        if (!(file >> link.source >> link.target >> link.cost)) {
            error = "Malformed topology link " + to_string(i + 1) + ".";
            return false;
        }
        links.push_back(link);
    }
    return true;
}

string jsonString(const string& value) {
    string escaped = "\"";
    for (unsigned char character : value) {
        switch (character) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (character < 0x20) {
                    const char hex[] = "0123456789abcdef";
                    escaped += "\\u00";
                    escaped += hex[(character >> 4) & 0xf];
                    escaped += hex[character & 0xf];
                } else {
                    escaped += static_cast<char>(character);
                }
        }
    }
    escaped += '"';
    return escaped;
}

void writeStringArray(ostream& output, const vector<string>& values) {
    output << '[';
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) output << ',';
        output << jsonString(values[i]);
    }
    output << ']';
}

long long pathCost(
    const vector<string>& path,
    const vector<TopologyLink>& links) {
    long long total = 0;
    for (size_t i = 1; i < path.size(); ++i) {
        bool found = false;
        for (const TopologyLink& link : links) {
            if ((link.source == path[i - 1] && link.target == path[i]) ||
                (link.source == path[i] && link.target == path[i - 1])) {
                total += link.cost;
                found = true;
                break;
            }
        }
        if (!found) return -1;
    }
    return total;
}

RouteResult shortestPathWithoutLink(
    const vector<TopologyLink>& links,
    int routerCount,
    const string& source,
    const string& destination,
    const string& blockedSource,
    const string& blockedTarget) {
    RouteResult result;
    result.reachable = false;
    result.totalCost = 0;

    vector<vector<pair<int, int> > > adjacency(routerCount);
    for (const TopologyLink& link : links) {
        if ((link.source == blockedSource && link.target == blockedTarget) ||
            (link.source == blockedTarget && link.target == blockedSource)) {
            continue;
        }
        int first = stoi(link.source.substr(1)) - 1;
        int second = stoi(link.target.substr(1)) - 1;
        adjacency[first].push_back(make_pair(second, link.cost));
        adjacency[second].push_back(make_pair(first, link.cost));
    }

    int sourceId = stoi(source.substr(1)) - 1;
    int destinationId = stoi(destination.substr(1)) - 1;
    const long long infinity = numeric_limits<long long>::max();
    vector<long long> distances(routerCount, infinity);
    vector<int> parents(routerCount, -1);
    vector<bool> visited(routerCount, false);
    distances[sourceId] = 0;

    for (int step = 0; step < routerCount; ++step) {
        int current = -1;
        for (int i = 0; i < routerCount; ++i) {
            if (!visited[i] &&
                (current == -1 || distances[i] < distances[current])) {
                current = i;
            }
        }
        if (current == -1 || distances[current] == infinity) break;
        visited[current] = true;
        if (current == destinationId) break;

        for (const pair<int, int>& edge : adjacency[current]) {
            long long candidate = distances[current] + edge.second;
            if (candidate < distances[edge.first]) {
                distances[edge.first] = candidate;
                parents[edge.first] = current;
            }
        }
    }

    if (distances[destinationId] == infinity) return result;
    vector<string> reversedPath;
    for (int current = destinationId; current != -1; current = parents[current]) {
        reversedPath.push_back("R" + to_string(current + 1));
        if (current == sourceId) break;
    }
    if (reversedPath.back() != source) return result;
    reverse(reversedPath.begin(), reversedPath.end());
    result.reachable = true;
    result.totalCost = distances[destinationId];
    result.path = reversedPath;
    return result;
}

bool createVisualDemos(
    const NetworkGraph& graph,
    const vector<TopologyLink>& links,
    const unordered_map<string, RoutingEntry>& destinations,
    vector<StressDemo>& demos,
    string& error) {
    struct Candidate {
        string source;
        string destination;
        RoutingEntry config;
        RouteResult route;
        vector<RouteResult> alternatives;
        vector<pair<string, string> > failedLinks;
    };

    vector<pair<string, RoutingEntry> > orderedDestinations(
        destinations.begin(), destinations.end());
    sort(
        orderedDestinations.begin(),
        orderedDestinations.end(),
        [](const pair<string, RoutingEntry>& left,
           const pair<string, RoutingEntry>& right) {
            return left.first < right.first;
        });

    vector<Candidate> candidates;
    for (int sourceId = 1; sourceId <= graph.routerCount(); ++sourceId) {
        const string source = "R" + to_string(sourceId);
        for (const auto& destination : orderedDestinations) {
            RouteResult route =
                graph.shortestPath(source, destination.first);
            if (!route.reachable || route.path.size() < 4) {
                continue;
            }

            Candidate candidate;
            candidate.source = source;
            candidate.destination = destination.first;
            candidate.config = destination.second;
            candidate.route = route;
            for (size_t i = 1; i + 1 < route.path.size(); ++i) {
                RouteResult alternative = shortestPathWithoutLink(
                    links,
                    graph.routerCount(),
                    route.path[1],
                    destination.first,
                    route.path[i],
                    route.path[i + 1]);
                vector<string> originalSuffix(
                    route.path.begin() + 1, route.path.end());
                if (alternative.reachable &&
                    alternative.path != originalSuffix) {
                    candidate.alternatives.push_back(alternative);
                    candidate.failedLinks.push_back(make_pair(
                        route.path[i], route.path[i + 1]));
                }
            }
            if (!candidate.alternatives.empty()) {
                candidates.push_back(candidate);
            }
        }
    }

    if (candidates.size() < 3) {
        error = "Could not find three routes with alternate paths for the stress demos.";
        return false;
    }

    demos.clear();
    for (int demoIndex = 0; demoIndex < 3; ++demoIndex) {
        size_t candidateIndex =
            static_cast<size_t>(demoIndex) * (candidates.size() - 1) / 2;
        const Candidate& candidate = candidates[candidateIndex];
        const size_t chosen =
            static_cast<size_t>(rand()) % candidate.alternatives.size();
        StressDemo demo;
        demo.packet.id = 9001 + demoIndex * 100;
        demo.packet.sourceRouter = candidate.source;
        demo.packet.destinationRouter = candidate.destination;
        demo.packet.destinationIP = candidate.config.prefix;
        demo.packet.currentRouter = candidate.source;
        demo.packet.priority = 9 + demoIndex;
        demo.initialRoute = candidate.route;
        demo.changedRoute = candidate.alternatives[chosen];
        demo.failedSource = candidate.failedLinks[chosen].first;
        demo.failedTarget = candidate.failedLinks[chosen].second;

        RouterQueue burst(ROUTER_QUEUE_CAPACITY);
        for (int i = 0; i < ROUTER_QUEUE_CAPACITY + 2; ++i) {
            Packet queued = demo.packet;
            queued.id = demo.packet.id + i;
            queued.priority = ROUTER_QUEUE_CAPACITY + 2 - i;
            const bool accepted = burst.enqueue(queued);

            DemoEvent event;
            event.type = "queue";
            event.router = demo.packet.sourceRouter;
            event.packetId = queued.id;
            event.action = accepted ? "enqueued" : "full";
            event.queueSize = burst.size();
            event.capacity = burst.getCapacity();
            demo.events.push_back(event);
        }
        Packet scheduledPacket;
        if (burst.dequeue(scheduledPacket)) {
            DemoEvent dequeue;
            dequeue.type = "queue";
            dequeue.router = demo.packet.sourceRouter;
            dequeue.packetId = scheduledPacket.id;
            dequeue.action = "dequeued";
            dequeue.queueSize = burst.size();
            dequeue.capacity = burst.getCapacity();
            demo.events.push_back(dequeue);
        }

        DemoEvent firstMove;
        firstMove.type = "move";
        firstMove.packetId = demo.packet.id;
        firstMove.from = candidate.route.path[0];
        firstMove.to = candidate.route.path[1];
        firstMove.cost = static_cast<int>(
            pathCost(vector<string>{firstMove.from, firstMove.to}, links));
        demo.events.push_back(firstMove);

        DemoEvent reroute;
        reroute.type = "reroute";
        reroute.packetId = demo.packet.id;
        reroute.router = candidate.route.path[1];
        reroute.failedSource = demo.failedSource;
        reroute.failedTarget = demo.failedTarget;
        reroute.oldPath.assign(
            candidate.route.path.begin() + 1, candidate.route.path.end());
        reroute.newPath = demo.changedRoute.path;
        reroute.oldCost = pathCost(reroute.oldPath, links);
        reroute.newCost = demo.changedRoute.totalCost;
        demo.events.push_back(reroute);

        for (size_t i = 1; i < demo.changedRoute.path.size(); ++i) {
            DemoEvent move;
            move.type = "move";
            move.packetId = demo.packet.id;
            move.from = demo.changedRoute.path[i - 1];
            move.to = demo.changedRoute.path[i];
            move.cost = static_cast<int>(
                pathCost(vector<string>{move.from, move.to}, links));
            demo.events.push_back(move);
        }

        DemoEvent delivered;
        delivered.type = "delivered";
        delivered.packetId = demo.packet.id;
        delivered.router = demo.packet.destinationRouter;
        demo.events.push_back(delivered);
        demos.push_back(demo);
    }
    return true;
}

void writeDemoJson(
    ostream& output,
    const StressDemo& demo,
    const vector<TopologyLink>& links) {
    vector<string> finalRoute;
    if (!demo.initialRoute.path.empty() && !demo.changedRoute.path.empty()) {
        finalRoute.assign(
            demo.initialRoute.path.begin(),
            demo.initialRoute.path.begin() + 1);
        finalRoute.insert(
            finalRoute.end(),
            demo.changedRoute.path.begin(),
            demo.changedRoute.path.end());
    }

    output << "{\"packet\":{\"id\":" << demo.packet.id
           << ",\"source\":" << jsonString(demo.packet.sourceRouter)
           << ",\"destination\":"
           << jsonString(demo.packet.destinationRouter)
           << ",\"ip\":" << jsonString(demo.packet.destinationIP)
           << ",\"priority\":" << demo.packet.priority
           << "},\"initialRoute\":";
    writeStringArray(output, demo.initialRoute.path);
    output << ",\"initialCost\":" << demo.initialRoute.totalCost
           << ",\"finalRoute\":";
    writeStringArray(output, finalRoute);
    output << ",\"finalCost\":" << pathCost(finalRoute, links)
           << ",\"queueCapacity\":" << ROUTER_QUEUE_CAPACITY
           << ",\"queueSize\":" << ROUTER_QUEUE_CAPACITY - 1
           << ",\"droppedPackets\":2,\"failedLink\":{\"source\":"
           << jsonString(demo.failedSource)
           << ",\"target\":" << jsonString(demo.failedTarget)
           << "},\"events\":[";
    for (size_t i = 0; i < demo.events.size(); ++i) {
        const DemoEvent& event = demo.events[i];
        if (i > 0) output << ',';
        output << "{\"type\":" << jsonString(event.type)
               << ",\"packetId\":" << event.packetId;
        if (event.type == "queue") {
            output << ",\"router\":" << jsonString(event.router)
                   << ",\"action\":" << jsonString(event.action)
                   << ",\"size\":" << event.queueSize
                   << ",\"capacity\":" << event.capacity;
        } else if (event.type == "move") {
            output << ",\"from\":" << jsonString(event.from)
                   << ",\"to\":" << jsonString(event.to)
                   << ",\"cost\":" << event.cost;
        } else if (event.type == "reroute") {
            output << ",\"router\":" << jsonString(event.router)
                   << ",\"failedSource\":" << jsonString(event.failedSource)
                   << ",\"failedTarget\":" << jsonString(event.failedTarget)
                   << ",\"oldPath\":";
            writeStringArray(output, event.oldPath);
            output << ",\"newPath\":";
            writeStringArray(output, event.newPath);
            output << ",\"oldCost\":" << event.oldCost
                   << ",\"newCost\":" << event.newCost;
        } else if (event.type == "delivered") {
            output << ",\"router\":" << jsonString(event.router);
        }
        output << '}';
    }
    output << "]}";
}

bool writeSimulationJson(
    const string& filename,
    const NetworkGraph& graph,
    const vector<TopologyLink>& links,
    const vector<Packet>& packets,
    const vector<PacketResult>& results,
    const vector<StressDemo>& demos,
    string& error) {
    ofstream output(filename.c_str());
    if (!output.is_open()) {
        error = "Unable to write simulation JSON file: " + filename;
        return false;
    }

    output << "{\n  \"version\": 1,\n  \"routers\": [";
    for (int i = 1; i <= graph.routerCount(); ++i) {
        if (i > 1) output << ',';
        output << jsonString("R" + to_string(i));
    }
    output << "],\n  \"links\": [";
    for (size_t i = 0; i < links.size(); ++i) {
        if (i > 0) output << ',';
        output << "{\"source\":" << jsonString(links[i].source)
               << ",\"target\":" << jsonString(links[i].target)
               << ",\"weight\":" << links[i].cost << '}';
    }

    output << "],\n  \"packets\": [";
    for (size_t i = 0; i < results.size(); ++i) {
        if (i > 0) output << ',';
        output << "{\"id\":" << results[i].packetId;
        if (i < packets.size()) {
            output << ",\"source\":" << jsonString(packets[i].sourceRouter)
                   << ",\"destination\":"
                   << jsonString(packets[i].destinationRouter)
                   << ",\"ip\":" << jsonString(packets[i].destinationIP)
                   << ",\"priority\":" << packets[i].priority;
        }
        output << ",\"route\":";
        writeStringArray(output, results[i].routeTaken);
        output << ",\"cost\":" << results[i].totalRouteCost
               << ",\"delivered\":"
               << (results[i].delivered ? "true" : "false")
               << ",\"message\":" << jsonString(results[i].message) << '}';
    }

    output << "],\n  \"demos\": [";
    for (size_t i = 0; i < demos.size(); ++i) {
        if (i > 0) output << ',';
        writeDemoJson(output, demos[i], links);
    }
    output << "]\n}\n";
    if (!output) {
        error = "Failed while writing simulation JSON file: " + filename;
        return false;
    }
    return true;
}

bool saveSimulationForFrontend(
    const NetworkGraph& graph,
    const vector<TopologyLink>& links,
    const unordered_map<string, RoutingEntry>& destinations,
    const vector<Packet>& packets,
    const vector<PacketResult>& results,
    string& error) {
    vector<StressDemo> demos;
    if (!createVisualDemos(graph, links, destinations, demos, error)) {
        return false;
    }

    if (!writeSimulationJson(
            "simulation.json", graph, links, packets, results,
            demos, error)) {
        return false;
    }
    return true;
}

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

bool queueInteractivePacket(
    NetworkGraph& graph,
    vector<RouterQueue>& queues,
    const Trie& destinationLookup,
    unordered_map<int, PacketResult>& results,
    const int packetId,
    const int priority,
    const string& source,
    const string& destinationIP,
    string& error) {
    if (results.find(packetId) != results.end()) {
        error = "Packet ID " + to_string(packetId) +
                " has already been used in this session.";
        return false;
    }
    if (!graph.hasRouter(source)) {
        error = "Unknown source router: " + source;
        return false;
    }

    string destination;
    try {
        destination = destinationLookup.longestPrefixMatch(destinationIP);
    } catch (const invalid_argument& exception) {
        error = "Invalid destination IP: " + string(exception.what());
        return false;
    }
    if (destination.empty()) {
        error = "No configured network matches destination IP " +
                destinationIP + ".";
        return false;
    }

    Packet packet;
    packet.id = packetId;
    packet.sourceRouter = source;
    packet.destinationRouter = destination;
    packet.destinationIP = destinationIP;
    packet.currentRouter = source;
    packet.priority = priority;

    const int routerIndex = stoi(source.substr(1)) - 1;
    if (!queues[routerIndex].enqueue(packet)) {
        error = "Router " + source + "'s queue is full (capacity " +
                to_string(queues[routerIndex].getCapacity()) + ").";
        return false;
    }

    PacketResult result;
    result.packetId = packet.id;
    result.delivered = false;
    result.finalRouter = source;
    result.totalRouteCost = 0;
    result.routeTaken.push_back(source);
    result.message = "Queued at " + source;
    results[packet.id] = result;
    cout << "Queued packet " << packet.id << " at " << source
         << " for " << destination << " (priority " << priority << ", "
         << queues[routerIndex].size() << "/"
         << queues[routerIndex].getCapacity() << ").\n";
    return true;
}

bool executeRouterPacket(
    NetworkGraph& graph,
    vector<RouterQueue>& queues,
    unordered_map<int, PacketResult>& results,
    const string& router,
    string& error) {
    if (!graph.hasRouter(router)) {
        error = "Unknown router: " + router;
        return false;
    }

    const int routerIndex = stoi(router.substr(1)) - 1;
    RouterQueue& queue = queues[routerIndex];
    if (queue.isEmpty()) {
        error = "Router " + router + " has no queued packets.";
        return false;
    }

    PacketHeap scheduler;
    Packet packet;
    while (queue.dequeue(packet)) {
        scheduler.push(packet);
    }
    packet = scheduler.pop();
    vector<Packet> waitingPackets;
    while (!scheduler.empty()) {
        waitingPackets.push_back(scheduler.pop());
    }
    for (const Packet& waiting : waitingPackets) {
        if (!queue.enqueue(waiting)) {
            error = "Unable to restore waiting packets to router " + router + ".";
            return false;
        }
    }

    auto resultEntry = results.find(packet.id);
    if (resultEntry == results.end()) {
        error = "No result record exists for packet " +
                to_string(packet.id) + ".";
        return false;
    }
    PacketResult& result = resultEntry->second;

    cout << "Router " << router << " selected packet " << packet.id
         << " (priority " << packet.priority << ").\n";
    if (packet.currentRouter == packet.destinationRouter) {
        result.delivered = true;
        result.finalRouter = router;
        result.message = "Delivered";
        cout << "Packet " << packet.id << " delivered at " << router << ".\n";
        return true;
    }

    auto table = routingTables.find(router);
    if (table == routingTables.end()) {
        result.finalRouter = router;
        result.message = "Dropped: no forwarding table for current router.";
        cout << result.message << '\n';
        return true;
    }

    string nextHop;
    try {
        nextHop = table->second.longestPrefixMatch(packet.destinationIP);
    } catch (const invalid_argument& exception) {
        error = "Could not forward packet: " + string(exception.what());
        return false;
    }
    if (nextHop.empty() || !graph.hasRouter(nextHop)) {
        result.finalRouter = router;
        result.message = "Dropped: no route matches the destination IP.";
        cout << result.message << '\n';
        return true;
    }

    RouteResult hop = graph.shortestPath(router, nextHop);
    if (!hop.reachable || hop.path.size() != 2) {
        result.finalRouter = router;
        result.message = "Dropped: forwarding entry is no longer reachable.";
        cout << result.message << '\n';
        return true;
    }

    packet.currentRouter = nextHop;
    result.routeTaken.push_back(nextHop);
    result.totalRouteCost += hop.totalCost;
    result.finalRouter = nextHop;
    const int nextRouterIndex = stoi(nextHop.substr(1)) - 1;
    if (!queues[nextRouterIndex].enqueue(packet)) {
        result.message = "Dropped: router queue is full at " + nextHop + ".";
        cout << result.message << '\n';
        return true;
    }

    result.message = "In transit to " + nextHop;
    cout << "Forwarded packet " << packet.id << " from " << router
         << " to " << nextHop << " (link cost " << hop.totalCost << ").\n"
         << "Packet is now queued at " << nextHop << " ("
         << queues[nextRouterIndex].size() << "/"
         << queues[nextRouterIndex].getCapacity() << ").\n";
    return true;
}

bool breakInteractiveLink(
    NetworkGraph& graph,
    vector<TopologyLink>& links,
    const unordered_map<string, RoutingEntry>& destinations,
    DSU& connectivity,
    const string& routerA,
    const string& routerB,
    string& error) {
    if (!graph.hasRouter(routerA) || !graph.hasRouter(routerB) ||
        routerA == routerB) {
        error = "Choose two different valid router IDs.";
        return false;
    }

    size_t linkIndex = links.size();
    for (size_t i = 0; i < links.size(); ++i) {
        if ((links[i].source == routerA && links[i].target == routerB) ||
            (links[i].source == routerB && links[i].target == routerA)) {
            linkIndex = i;
            break;
        }
    }
    if (linkIndex == links.size()) {
        error = "There is no direct link between " + routerA +
                " and " + routerB + ".";
        return false;
    }

    const int cost = links[linkIndex].cost;
    if (!graph.removeLink(routerA, routerB)) {
        error = "Failed to remove link " + routerA + " - " + routerB + ".";
        return false;
    }
    links.erase(links.begin() + linkIndex);

    if (!buildForwardingTables(graph, destinations, connectivity, error)) {
        return false;
    }

    cout << "Link " << routerA << " - " << routerB << " (cost " << cost
         << ") is now down.\n"
         << "Forwarding tables and connectivity have been recalculated.\n";
    showConnectivity(connectivity, graph.routerCount());
    return true;
}

}  // namespace

int main() {
    srand(static_cast<unsigned int>(time(NULL) ^ clock()));
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

    vector<TopologyLink> topologyLinks;
    if (!loadTopologyLinks(topologyFile, topologyLinks, error)) {
        cerr << "Topology error: " << error << '\n';
        return 1;
    }

    DSU connectivity(graph.routerCount());
    if (!buildForwardingTables(graph, destinations, connectivity, error)) {
        cerr << "Forwarding table error: " << error << '\n';
        return 1;
    }

    vector<RouterQueue> routerQueues;
    routerQueues.reserve(graph.routerCount());
    for (int i = 0; i < graph.routerCount(); ++i) {
        routerQueues.emplace_back(ROUTER_QUEUE_CAPACITY);
    }
    unordered_map<int, PacketResult> interactiveResults;

    cout << "Network Packet Routing Simulator\n"
         << "Loaded " << graph.routerCount() << " routers and "
         << destinations.size() << " destination networks.\n";

    while (true) {
        cout << "\n1. Simulate packets from packets.txt\n"
             << "2. Send a packet interactively\n"
             << "3. Display network topology\n"
             << "4. Display router connectivity\n"
             << "5. Find a shortest route\n"
             << "6. Queue a packet at a router\n"
             << "7. Execute the highest-priority packet at a router\n"
             << "8. Break a network link\n"
             << "9. Exit\n";

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
            if (saveSimulationForFrontend(
                    graph, topologyLinks, destinations, packets, results, error)) {
                cout << "Frontend replay saved to simulation.json\n";
            } else {
                cerr << "Could not save frontend replay: " << error << '\n';
            }
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
            if (saveSimulationForFrontend(
                    graph, topologyLinks, destinations, vector<Packet>(1, packet),
                    results, error)) {
                cout << "Frontend replay saved to simulation.json\n";
            } else {
                cerr << "Could not save frontend replay: " << error << '\n';
            }
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
            cout << "Router to queue packet at: ";
            if (!(cin >> source)) {
                break;
            }
            cout << "Destination IPv4 address: ";
            if (!(cin >> destinationIP)) {
                break;
            }

            if (!queueInteractivePacket(
                    graph, routerQueues, destinationLookup,
                    interactiveResults, packetId, priority,
                    source, destinationIP, error)) {
                cout << "Could not queue packet: " << error << '\n';
            }
        } else if (choice == 7) {
            string router;
            printRouterChoices(graph.routerCount());
            cout << "Router to execute: ";
            if (!(cin >> router)) {
                break;
            }
            if (!executeRouterPacket(
                    graph, routerQueues, interactiveResults, router, error)) {
                cout << "Could not execute packet: " << error << '\n';
            }
        } else if (choice == 8) {
            string routerA;
            string routerB;
            printRouterChoices(graph.routerCount());
            cout << "First router on the link: ";
            if (!(cin >> routerA)) {
                break;
            }
            cout << "Second router on the link: ";
            if (!(cin >> routerB)) {
                break;
            }
            if (!breakInteractiveLink(
                    graph, topologyLinks, destinations, connectivity,
                    routerA, routerB, error)) {
                cout << "Could not break link: " << error << '\n';
            }
        } else if (choice == 9) {
            cout << "Goodbye.\n";
            break;
        } else {
            cout << "Choose an option from 1 to 9.\n";
        }
    }

    return 0;
}
