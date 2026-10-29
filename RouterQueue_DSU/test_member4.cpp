#include <iostream>
#include <cassert>
#include "router_queue.h"
#include "dsu.h"

using namespace std;

void testRouterQueue() {
    cout << "--- Testing RouterQueue ---\n";

    // Test 1: Empty queue behavior
    RouterQueue q(3);
    assert(q.isEmpty());
    assert(!q.isFull());
    assert(q.size() == 0);
    assert(q.getCapacity() == 3);

    Packet dummyOut;
    assert(!q.dequeue(dummyOut)); // Should fail when empty
    cout << "PASS: Empty queue checks\n";

    // Test 2: Enqueue up to capacity
    Packet p1{101, "R1", "R16", "10.0.16.25", "R1", 3};
    Packet p2{102, "R2", "R12", "10.0.12.10", "R2", 5};
    Packet p3{103, "R1", "R8",  "10.0.8.15",  "R1", 2};
    Packet p4{104, "R5", "R20", "10.0.20.7",  "R5", 4};

    assert(q.enqueue(p1));
    assert(q.size() == 1);
    assert(!q.isEmpty());
    assert(!q.isFull());

    assert(q.enqueue(p2));
    assert(q.size() == 2);

    assert(q.enqueue(p3));
    assert(q.size() == 3);
    assert(q.isFull());
    cout << "PASS: Enqueue to capacity\n";

    // Test 3: Overflow rejection
    assert(!q.enqueue(p4)); // Capacity reached, must reject
    assert(q.size() == 3);
    cout << "PASS: Queue full rejection\n";

    // Test 4: FIFO ordering & dequeue
    Packet out;
    assert(q.dequeue(out));
    assert(out.id == 101);
    assert(q.size() == 2);
    assert(!q.isFull());

    assert(q.dequeue(out));
    assert(out.id == 102);
    assert(q.size() == 1);
    cout << "PASS: FIFO dequeue order\n";

    // Test 5: Circular buffer wraparound
    // We dequeued 2 items, so indices 0 and 1 are freed.
    // Adding 2 new items should wrap around to index 0.
    assert(q.enqueue(p4));
    Packet p5{105, "R7", "R25", "10.0.25.99", "R7", 1};
    assert(q.enqueue(p5));
    assert(q.isFull());
    assert(q.size() == 3);

    // Remaining sequence should be: p3 (103), p4 (104), p5 (105)
    assert(q.dequeue(out));
    assert(out.id == 103);
    assert(q.dequeue(out));
    assert(out.id == 104);
    assert(q.dequeue(out));
    assert(out.id == 105);

    assert(q.isEmpty());
    assert(q.size() == 0);
    cout << "PASS: Circular wraparound verification\n";
}

void testDSU() {
    cout << "\n--- Testing DSU ---\n";

    // 5 routers (0 to 4 representing R1 to R5)
    DSU dsu(5);
    assert(dsu.getComponentCount() == 5);

    for (int i = 0; i < 5; ++i) {
        assert(dsu.find(i) == i);
        assert(dsu.connected(i, i));
    }

    assert(!dsu.connected(0, 1));
    assert(!dsu.connected(0, 4));
    cout << "PASS: Initial disjoint state\n";

    // Connect 0-1 and 2-3
    dsu.unite(0, 1);
    assert(dsu.connected(0, 1));
    assert(dsu.getComponentCount() == 4);

    dsu.unite(2, 3);
    assert(dsu.connected(2, 3));
    assert(!dsu.connected(0, 2));
    assert(dsu.getComponentCount() == 3);
    cout << "PASS: Direct connections\n";

    // Transitive connection: unite 1-2 => 0, 1, 2, 3 should all be connected
    dsu.unite(1, 2);
    assert(dsu.connected(0, 3));
    assert(dsu.connected(1, 3));
    assert(!dsu.connected(0, 4)); // Router 4 is still isolated
    assert(dsu.getComponentCount() == 2);
    cout << "PASS: Transitive connectivity\n";

    // Out of bounds check
    assert(dsu.find(-1) == -1);
    assert(dsu.find(10) == -1);
    assert(!dsu.connected(-1, 2));
    assert(!dsu.connected(1, 10));
    cout << "PASS: Bounds checking\n";

    // Simulated link failure:
    // If link (1, 2) fails, we reset DSU and rebuild with surviving links (0-1, 2-3)
    dsu.reset(5);
    assert(dsu.getComponentCount() == 5);
    dsu.unite(0, 1);
    dsu.unite(2, 3);
    assert(dsu.connected(0, 1));
    assert(dsu.connected(2, 3));
    assert(!dsu.connected(0, 3)); // Now disconnected because (1, 2) failed
    cout << "PASS: Failure rebuild scenario\n";
}

int main() {
    cout << "Running Member 4 Unit Tests...\n\n";
    testRouterQueue();
    testDSU();
    cout << "\nALL MEMBER 4 TESTS PASSED SUCCESSFULLY!\n";
    return 0;
}
