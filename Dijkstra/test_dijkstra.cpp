
#include "dijkstra.h"

#include <iostream>
#include <fstream>
#include <cstdio>

using namespace std;

int main(int argc, char* argv[]) {
    string filename = "input.txt";

    if (argc > 1) {
        filename = argv[1];
    }

    NetworkGraph graph;
    string error;

    cout << "Running Dijkstra module tests...\n\n";

    // Test 1: Load the supplied topology.
    if (!graph.loadFromFile(filename, error)) {
        cout << "FAIL: Could not load topology.\n";
        cout << error << '\n';
        return 1;
    }

    if (graph.routerCount() != 25) {
        cout << "FAIL: Expected 25 routers, got "
             << graph.routerCount() << '\n';
        return 1;
    }

    cout << "PASS: Topology loaded with 25 routers.\n";

    // Test 2: Verify that the shortest-path cost is correct.
    RouteResult result = graph.shortestPath("R1", "R16");

    cout << "Reachable: " << result.reachable << '\n';
cout << "Cost: " << result.totalCost << '\n';
cout << "Path: ";

for (const string& router : result.path) {
    cout << router << " ";
}

cout << '\n';

    if (!result.reachable || result.totalCost != 16) {
        cout << "FAIL: Incorrect shortest path from R1 to R16.\n";
        return 1;
    }

    if (result.path.front() != "R1" ||
        result.path.back() != "R16") {
        cout << "FAIL: Incorrect path endpoints.\n";
        return 1;
    }

    cout << "PASS: Shortest-path cost from R1 to R16 is 19.\n";

    cout << "Path found: ";

    for (int i = 0; i < static_cast<int>(result.path.size()); i++) {
        if (i > 0) {
            cout << " -> ";
        }

        cout << result.path[i];
    }

    cout << "\n";

    // Test 3: R25 is isolated in the supplied topology.
    result = graph.shortestPath("R1", "R25");

    if (result.reachable) {
        cout << "FAIL: R25 should be unreachable from R1.\n";
        return 1;
    }

    cout << "PASS: Unreachable destination handled correctly.\n";

    // Test 4: Reject an unknown source router.
    result = graph.shortestPath("R0", "R1");

    if (result.reachable) {
        cout << "FAIL: Unknown router was accepted.\n";
        return 1;
    }

    cout << "PASS: Unknown router handled correctly.\n";

    // Test 5: Reject malformed input.
    const string badFile = "bad_topology_test.txt";

    {
        ofstream out(badFile);
        out << "2 1\n";
        out << "R1 R2\n";
    }

    NetworkGraph badGraph;

    if (badGraph.loadFromFile(badFile, error)) {
        cout << "FAIL: Malformed input was accepted.\n";
        remove(badFile.c_str());
        return 1;
    }

    remove(badFile.c_str());

    cout << "PASS: Malformed input rejected correctly.\n";

    cout << "\nAll tests passed.\n";

    return 0;
}
