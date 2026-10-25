#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>

using namespace std;

struct Packet {
    int id;
    string sourceRouter;
    string destinationRouter;
    string destinationIP;
    string currentRouter;
    int priority;
};

struct RouteResult {
    bool reachable;
    long long totalCost;
    vector<string> path;
};

struct RoutingEntry {
    string prefix;
    int prefixLength;
    string nextHop;
};

struct PacketResult {
    int packetId;
    bool delivered;
    string finalRouter;
    long long totalRouteCost;
    vector<string> routeTaken;
    string message;
};

#endif
