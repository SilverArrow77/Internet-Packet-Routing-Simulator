#include <iostream>
#include <cassert>
#include <string>
#include <vector>

#include "../common.h"
#include "../integration/network_simulator.h"

using namespace std;

void testTopologyAndConfigInit() {
    cout << "Test 1: Topology and Config Initialization... ";
    NetworkSimulator sim;
    string error;

    bool ok = sim.initialize("input.txt", "routing_config.txt", error);
    assert(ok && "Simulator initialization failed");
    assert(sim.getRouterCount() == 25 && "Expected 25 routers");
    assert(sim.isConnected("R1", "R16") && "R1 and R16 should be connected");
    assert(sim.isConnected("R1", "R25") && "R1 and R25 should be connected");
    cout << "PASSED\n";
}

void testPacketDelivery() {
    cout << "Test 2: End-to-End Packet Delivery (README Contract)... ";
    NetworkSimulator sim;
    string error;
    sim.initialize("input.txt", "routing_config.txt", error);

    // Packet 101: R1 to R16 via 10.0.16.25
    Packet p101{101, "R1", "R16", "10.0.16.25", "R1", 3};
    PacketResult res = sim.simulatePacket(p101, false);

    assert(res.delivered && "Packet 101 must be delivered");
    assert(res.finalRouter == "R16" && "Final router must be R16");
    assert(res.totalRouteCost == 16 && "Expected route cost 16 for R1 -> R16");

    // Must follow shortest path: R1 -> R3 -> R4 -> R8 -> ... -> R16
    assert(res.routeTaken.front() == "R1");
    assert(res.routeTaken.back() == "R16");
    assert(res.routeTaken[1] == "R3" && "Next hop from R1 must be R3");
    cout << "PASSED\n";
}

void testBatchWorkload() {
    cout << "Test 3: Batch Workload from packets.txt... ";
    NetworkSimulator sim;
    string error;
    sim.initialize("input.txt", "routing_config.txt", error);

    vector<Packet> packets = sim.loadPacketsFromFile("packets.txt", error);
    assert(packets.size() == 5 && "Expected 5 packets loaded");

    vector<PacketResult> results = sim.simulateBatch(packets, false);
    assert(results.size() == 5 && "Expected 5 results");

    for (const auto& r : results) {
        assert(r.delivered && "All 5 sample packets should be delivered in default topology");
    }
    cout << "PASSED\n";
}

void testPriorityScheduling() {
    cout << "Test 4: Priority Scheduling Order and Tie-Breaking... ";
    PacketHeap heap;

    Packet lowPrio{101, "R1", "R2", "10.0.2.1", "R1", 1};
    Packet midPrio{102, "R1", "R2", "10.0.2.1", "R1", 5};
    Packet highPrio{103, "R1", "R2", "10.0.2.1", "R1", 9};
    Packet tieBreakA{50, "R1", "R2", "10.0.2.1", "R1", 5}; // Same prio as midPrio, smaller ID

    heap.push(lowPrio);
    heap.push(midPrio);
    heap.push(highPrio);
    heap.push(tieBreakA);

    assert(heap.pop().id == 103); // Highest prio (9)
    assert(heap.pop().id == 50);  // Tie-break: ID 50 before 102
    assert(heap.pop().id == 102); // Prio 5
    assert(heap.pop().id == 101); // Lowest prio (1)
    assert(heap.empty());
    cout << "PASSED\n";
}

void testRouterQueueOverflowDrop() {
    cout << "Test 5: Router Queue Buffer Overflow Packet Drop... ";
    NetworkSimulator sim;
    string error;
    sim.initialize("input.txt", "routing_config.txt", error);

    // Restrict R1's queue capacity to 1
    sim.setRouterQueueCapacity("R1", 1);

    Packet p1{1, "R1", "R16", "10.0.16.25", "R1", 1};
    Packet p2{2, "R1", "R16", "10.0.16.25", "R1", 1};

    // Pre-fill queue
    assert(sim.getRouterQueue("R1").enqueue(p1));
    assert(sim.getRouterQueue("R1").isFull());

    // Second packet attempting to enter R1 must be dropped
    PacketResult dropRes = sim.simulatePacket(p2, false);
    assert(!dropRes.delivered);
    assert(dropRes.message.find("queue full") != string::npos);
    cout << "PASSED\n";
}

void testDynamicLinkFailureAndRerouting() {
    cout << "Test 6: Dynamic Link Failure and Route Recomputation... ";
    NetworkSimulator sim;
    string error;
    sim.initialize("input.txt", "routing_config.txt", error);

    Packet p{201, "R1", "R16", "10.0.16.25", "R1", 3};
    PacketResult before = sim.simulatePacket(p, false);
    assert(before.delivered);

    // Fail the link R8 - R16
    bool cut = sim.simulateLinkFailure("R8", "R16");
    assert(cut && "Link R8-R16 should be removed");

    // Packet should still reach R16 via alternate route (e.g., through R12)
    PacketResult after = sim.simulatePacket(p, false);
    assert(after.delivered && "Packet should find alternate route");
    assert(after.finalRouter == "R16");

    // Verify R8-R16 direct hop is NOT in path
    for (size_t i = 0; i + 1 < after.routeTaken.size(); ++i) {
        bool isCutLink = (after.routeTaken[i] == "R8" && after.routeTaken[i+1] == "R16");
        assert(!isCutLink && "Failed link must not appear in new path");
    }
    cout << "PASSED\n";
}

void testDSUPartitionDetection() {
    cout << "Test 7: Network Partition Detection via DSU... ";
    NetworkSimulator sim;
    string error;
    sim.initialize("input.txt", "routing_config.txt", error);

    // Isolate R25 by cutting all incident links (R1-R25 and R24-R25)
    sim.simulateLinkFailure("R1", "R25");
    sim.simulateLinkFailure("R24", "R25");

    assert(!sim.isConnected("R1", "R25") && "R25 must be disconnected from R1");

    Packet p{301, "R1", "R25", "10.0.25.99", "R1", 1};
    PacketResult res = sim.simulatePacket(p, false);

    assert(!res.delivered);
    assert(res.message.find("partition") != string::npos || res.message.find("unreachable") != string::npos);
    cout << "PASSED\n";
}

void testTrieLongestPrefixMatch() {
    cout << "Test 8: Trie Longest Prefix Match Subnet Specificity... ";
    Trie trie;
    trie.insert("10.0.0.0", 8, "R_BROAD");
    trie.insert("10.0.16.0", 24, "R_SPECIFIC");

    // IP matching both /8 and /24 must choose the longer prefix (/24)
    string match1 = trie.longestPrefixMatch("10.0.16.55");
    assert(match1 == "R_SPECIFIC");

    // IP matching only /8 must choose /8
    string match2 = trie.longestPrefixMatch("10.0.99.55");
    assert(match2 == "R_BROAD");

    // Unmatched IP returns empty string
    string match3 = trie.longestPrefixMatch("192.168.1.1");
    assert(match3.empty());
    cout << "PASSED\n";
}

int main() {
    cout << "\n==================================================\n";
    cout << "      RUNNING INTEGRATION TEST FRAMEWORK         \n";
    cout << "==================================================\n\n";

    testTopologyAndConfigInit();
    testPacketDelivery();
    testBatchWorkload();
    testPriorityScheduling();
    testRouterQueueOverflowDrop();
    testDynamicLinkFailureAndRerouting();
    testDSUPartitionDetection();
    testTrieLongestPrefixMatch();

    cout << "\n==================================================\n";
    cout << "   ALL 8 INTEGRATION TESTS PASSED SUCCESSFULLY!   \n";
    cout << "==================================================\n\n";

    return 0;
}
