# Integration Test Suite

This directory contains the integration testing framework for the Internet Packet Routing Simulator.

## Running Tests

From the project root:
```bash
make test
```
Or directly:
```bash
g++ -std=c++17 -Wall -Wextra -I. tests/test_integration.cpp \
  integration/network_simulator.cpp Dijkstra/dijkstra.cpp Member2_Trie/trie.cpp \
  Member3_PacketHeap/packet_heap.cpp Member4_RouterQueue_DSU/router_queue.cpp \
  Member4_RouterQueue_DSU/dsu.cpp -o test_integration && ./test_integration
```

## Implemented Test Cases

1. **Topology & Config Initialization**: Verifies all 25 routers and DSU connectivity are set up properly.
2. **End-to-End Packet Delivery**: Tests Packet 101 ($R1 \to R16$) reproducing the exact blueprint contract (Route: $R1 \to R3 \to R4 \to R8 \to \dots \to R16$, Cost: 16).
3. **Batch Workload**: Verifies that all packets from `packets.txt` are parsed and delivered.
4. **Priority Scheduling & Tie-Breaking**: Validates that higher priority packets dequeue first and equal priorities break ties by lowest packet ID.
5. **Router Queue Overflow Drop**: Validates that exceeding router buffer capacity drops packets and logs clear overflow messages.
6. **Dynamic Link Failure & Rerouting**: Simulates cutting link $R8-R16$, recomputing shortest paths, rebuilding forwarding tables, and delivering packets across alternate surviving routes.
7. **Network Partition Detection via DSU**: Completely isolates router $R25$, ensuring DSU detects disconnection immediately.
8. **Trie Longest Prefix Match**: Tests hierarchical subnet matching (e.g. `/24` preference over `/16` and `/8`).
