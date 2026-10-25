#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

#include "common.h"
#include "integration/network_simulator.h"

using namespace std;

void printSection(const string& title) {
    cout << "\n======================================================================\n";
    cout << "  " << title << "\n";
    cout << "======================================================================\n";
}

int main() {
    cout << "######################################################################\n";
    cout << "#                                                                    #\n";
    cout << "#        INTERNET PACKET ROUTING SIMULATOR - INTERACTIVE DEMO        #\n";
    cout << "#                                                                    #\n";
    cout << "######################################################################\n";

    NetworkSimulator sim;
    string error;

    if (!sim.initialize("input.txt", "routing_config.txt", error)) {
        cerr << "Failed to initialize simulator: " << error << "\n";
        return 1;
    }

    // -------------------------------------------------------------------------
    // SCENARIO 1: Standard Priority & Shortest Path Delivery
    // -------------------------------------------------------------------------
    printSection("SCENARIO 1: Standard End-to-End Delivery (R1 to R16)");
    cout << "Description: Simulating Packet P101 addressed to 10.0.16.25 (behind R16).\n";
    cout << "Shows the cooperation between Dijkstra (path), Trie (next hop), and Queue/Heap.\n\n";

    Packet p101{101, "R1", "R16", "10.0.16.25", "R1", 3};
    PacketResult r1 = sim.simulatePacket(p101, true);

    cout << "\nResult: " << (r1.delivered ? "DELIVERED" : "FAILED")
         << " | Total Cost: " << r1.totalRouteCost
         << " | Status: " << r1.message << "\n";

    // -------------------------------------------------------------------------
    // SCENARIO 2: Priority Scheduling Under Congestion (Member 3 Max-Heap)
    // -------------------------------------------------------------------------
    printSection("SCENARIO 2: Priority Scheduling Under Contention");
    cout << "Description: Two packets arrive at Router R1 at the same time:\n";
    cout << "  - Packet A: Bulk Background Download (Priority 1, ID 201)\n";
    cout << "  - Packet B: Emergency VoIP Audio Call (Priority 9, ID 202)\n\n";

    // Demonstrate the Member 3 Heap inside the router queue context
    Packet pLow{201, "R1", "R16", "10.0.16.50", "R1", 1};
    Packet pHigh{202, "R1", "R16", "10.0.16.99", "R1", 9};

    RouterQueue q(5);
    q.enqueue(pLow);
    q.enqueue(pHigh);
    cout << "Ingress Router Queue received both packets in arrival order: [P201 (Prio 1), P202 (Prio 9)]\n";

    PacketHeap scheduler;
    Packet dequeued;
    while (q.dequeue(dequeued)) {
        scheduler.push(dequeued);
    }
    cout << "Transferred all queued packets into Member 3's PacketHeap scheduler...\n";

    Packet firstServed = scheduler.pop();
    Packet secondServed = scheduler.pop();

    cout << "-> FIRST packet dispatched by Heap: P" << firstServed.id
         << " (Priority: " << firstServed.priority << ") [VIP packet jumped ahead!]\n";
    cout << "-> SECOND packet dispatched by Heap: P" << secondServed.id
         << " (Priority: " << secondServed.priority << ") [Served after VIP]\n";

    // -------------------------------------------------------------------------
    // SCENARIO 3: Bounded Buffer Overflow & Packet Drop (Member 4 Queue)
    // -------------------------------------------------------------------------
    printSection("SCENARIO 3: Router Queue Capacity Limit & Dropping");
    cout << "Description: Router R1 has a hardware buffer capacity of only 2 packets.\n";
    cout << "We attempt to push 3 packets into R1.\n\n";

    sim.setRouterQueueCapacity("R1", 2);

    Packet p1{301, "R1", "R16", "10.0.16.1", "R1", 3};
    Packet p2{302, "R1", "R16", "10.0.16.2", "R1", 3};
    Packet p3{303, "R1", "R16", "10.0.16.3", "R1", 3};

    sim.getRouterQueue("R1").enqueue(p1);
    cout << "Enqueue Packet 301 into R1: SUCCESS (Queue size: " << sim.getRouterQueue("R1").size() << "/2)\n";

    sim.getRouterQueue("R1").enqueue(p2);
    cout << "Enqueue Packet 302 into R1: SUCCESS (Queue size: " << sim.getRouterQueue("R1").size() << "/2 - FULL!)\n";

    PacketResult dropResult = sim.simulatePacket(p3, false);
    cout << "Enqueue Packet 303 into R1: "
         << (dropResult.delivered ? "DELIVERED" : "REJECTED/DROPPED")
         << "\nMessage: " << dropResult.message << "\n";

    // Reset queue capacity back to standard
    sim.setRouterQueueCapacity("R1", 10);
    sim.resetQueues();

    // -------------------------------------------------------------------------
    // SCENARIO 4: Dynamic Link Failure & Automated Rerouting
    // -------------------------------------------------------------------------
    printSection("SCENARIO 4: Dynamic Link Failure & Route Adaptation");
    cout << "Description: Simulating a physical cable cut on link (R8 <-> R16).\n";

    cout << "\n[Before Cut] Route for P401 (R1 -> R16):\n";
    Packet p401{401, "R1", "R16", "10.0.16.25", "R1", 3};
    PacketResult beforeCut = sim.simulatePacket(p401, false);
    cout << "  Path: ";
    for (size_t i = 0; i < beforeCut.routeTaken.size(); ++i) {
        if (i > 0) cout << " -> ";
        cout << beforeCut.routeTaken[i];
    }
    cout << " (Cost: " << beforeCut.totalRouteCost << ")\n";

    cout << "\n>>> SIMULATING CABLE FAILURE: Cutting link R8 <-> R16 <<<\n";
    sim.simulateLinkFailure("R8", "R16");
    cout << "  - Link removed from graph.\n";
    cout << "  - DSU updated.\n";
    cout << "  - Dijkstra recomputed shortest paths.\n";
    cout << "  - Forwarding Tries re-populated with new next hops.\n";

    cout << "\n[After Cut] Route for P402 (R1 -> R16):\n";
    Packet p402{402, "R1", "R16", "10.0.16.25", "R1", 3};
    PacketResult afterCut = sim.simulatePacket(p402, false);
    cout << "  Path: ";
    for (size_t i = 0; i < afterCut.routeTaken.size(); ++i) {
        if (i > 0) cout << " -> ";
        cout << afterCut.routeTaken[i];
    }
    cout << " (Cost: " << afterCut.totalRouteCost << ")\n";
    cout << "  Result: " << afterCut.message << "\n";

    // -------------------------------------------------------------------------
    // SCENARIO 5: Network Partition / Unreachable Destination (DSU Check)
    // -------------------------------------------------------------------------
    printSection("SCENARIO 5: Network Partition & DSU Early Detection");
    cout << "Description: Isolating Router R25 by cutting all its incident links.\n";
    cout << "DSU detects network disconnection immediately without wasted forwarding.\n\n";

    sim.simulateLinkFailure("R1", "R25");
    sim.simulateLinkFailure("R24", "R25");

    bool connected = sim.isConnected("R1", "R25");
    cout << "DSU Connectivity Check: isConnected(R1, R25) = " << (connected ? "TRUE" : "FALSE") << "\n";

    Packet p501{501, "R1", "R25", "10.0.25.99", "R1", 1};
    PacketResult partResult = sim.simulatePacket(p501, false);

    cout << "Packet P501 to isolated router R25: "
         << (partResult.delivered ? "DELIVERED" : "DROPPED")
         << "\nMessage: " << partResult.message << "\n";

    cout << "\n######################################################################\n";
    cout << "#                       DEMO COMPLETE                                #\n";
    cout << "######################################################################\n";

    return 0;
}
