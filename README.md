Network Packet Routing Simulator — Project Blueprint

This blueprint defines the shared data structures, module interfaces, function contracts, and runtime flow for all four team members. The goal is to let everyone implement their module independently and then integrate them without incompatible function signatures or duplicated responsibilities.

1. Final architecture

The simulator models a network of routers, routes packets through the network, schedules packets by priority, and handles router buffers and connectivity analysis.

Input and initialization

input.txt defines routers, links, and link costs. Packet input defines the traffic to simulate.

Member 1 — Graph + Dijkstra

Calculates shortest paths and next hops between routers.

Integration layer — Build forwarding tables

Maps destination networks to routers and inserts Dijkstra-derived next hops into the appropriate Tries.

Member 2 — Trie

Looks up the destination IP at the current router and returns the next hop.

Members 3 and 4 — Scheduling and buffering

Member 3 selects packets by priority. Member 4 manages router queues and connectivity analysis.

Output: delivered packets, routes, costs, and failure reports

The main program coordinates these components. Each member owns their implementation files and exposes only the agreed public functions.

2. Shared files and project structure

Use one shared directory:

input.txt: network topology, shared by the graph module and connectivity analysis.

packets.txt: packet workload, read by the main program.

routing_config.txt: destination IP networks and their corresponding destination routers.

common.h: shared structures used by multiple modules.

main.cpp: initializes components and coordinates the simulation.


3. Shared data structures — common.h

All members must agree on these structures first.

What each field means

Structure	Important fields	Purpose

Packet	id, sourceRouter, destinationRouter, destinationIP, currentRouter, priority	Identifies the packet, its destination, its current location, and scheduling priority
RouteResult	reachable, totalCost, path	Result returned by Dijkstra
RoutingEntry	prefix, prefixLength, nextHop	One forwarding rule stored in a router's Trie
PacketResult	delivered, routeTaken, totalRouteCost, message	Final outcome of simulating a packet


The destinationRouter field is used by the simulator to know which router represents the intended destination. The destinationIP is used for Trie lookup. The routing configuration must map the IP prefix to the destination router.

For example, 10.0.16.0/24 could represent the network behind R16. This is a project-defined mapping, not information already present in your topology file.

4. Member 1 — Graph and Dijkstra

Files: dijkstra.h, dijkstra.cpp

Your module is the source of shortest-path information. It must not depend on the Trie, packet heap, or router queue.

Public interface

Your existing methods can remain. Add getNextHop() as a convenience method.

getNextHop() should return the second router in the shortest path, or an empty string if the destination is unreachable or the source and destination are the same.

Function contract

Function	Input	Expected output

loadFromFile()	"input.txt", error string	true on success; false with an error message otherwise
shortestPath()	"R1", "R16"	RouteResult containing the path and total cost
getNextHop()	"R1", "R16"	"R3" for the current topology
hasRouter()	"R16"	true
routerCount()	None	25 for the supplied topology


Who calls your functions?

main.cpp calls loadFromFile() once during initialization.

The forwarding-table initialization code calls shortestPath() for each source/destination pair it needs to configure.

Other modules may call hasRouter() if they need to validate a router ID.


Example

Expected result:

That path is used to construct forwarding entries. Member 2 does not call Dijkstra internally; the integration code obtains routes from your module and inserts the corresponding entries into the Tries.

5. Member 2 — Trie and forwarding table

Files: trie.h, trie.cpp

Member 2 owns the prefix-matching algorithm. The Trie should store forwarding entries, not calculate network costs.

Public interface

Each router gets its own Trie. The integration layer can maintain them in a map:

For example, routingTables["R1"] represents R1's forwarding table.

Function contract

Function	Input	Expected output

insert()	"10.0.16.0", 24, "R3"	Stores the prefix with next hop R3
longestPrefixMatch()	"10.0.16.25"	"R3" if that is the best matching entry
longestPrefixMatch()	An unmatched IP	Empty string


The implementation must parse IPv4 addresses into bits, follow the Trie branches, and remember the most recent matching prefix. That last matching node is the longest-prefix match.

Who calls the Trie?

The initialization code inserts routing entries into each router's Trie.

The main packet-forwarding loop calls longestPrefixMatch() whenever a packet needs forwarding from its current router.


How are the entries generated?

For every source router S and destination router D:

1. Look up D's configured destination IP prefix.


2. Call graph.shortestPath(S, D).


3. If the route is reachable and has more than one router, take path[1] as the next hop.


4. Insert D's prefix and that next hop into routingTables[S].



Example:

For this example, R1's Trie stores the prefix 10.0.16.0/24 with next hop R3.

The same prefix is inserted into R3's Trie with next hop R4, R4's with R8, and so on.

Important: the prefix-to-destination-router mapping must be supplied by routing_config.txt or another shared configuration. It cannot be inferred from input.txt alone.

6. Member 3 — Binary heap and packet scheduler

Files: packet_heap.h, packet_heap.cpp

Member 3 implements a max-priority heap of packets. A larger priority number means a packet is processed sooner. Ties are resolved by lower packet ID.

Public interface

Function contract

Function	Input	Expected output

push()	A Packet	Inserts it into the heap
top()	None	Returns the highest-priority packet without removing it
pop()	None	Returns and removes the highest-priority packet
empty()	None	true if no packets remain
size()	None	Number of packets in the heap


pop() and top() must only be called when the heap is not empty.

Who calls the heap?

The main program transfers packets waiting in a router's buffer into that router's scheduler heap. It then pops the highest-priority packet and forwards it. A packet should be held in only one active queue or heap at a time.

This makes Member 3 responsible for which waiting packet is processed first, not which network route it takes.

7. Member 4 — Router queues and DSU

Files: router_queue.h, router_queue.cpp, dsu.h, dsu.cpp

Member 4 has two independent responsibilities: buffering packets at routers and analyzing network connectivity.

Router queue interface

Function	Input	Expected output

enqueue()	Packet	true if added, false if full
dequeue()	Output Packet&	true if removed, false if empty
isEmpty()	None	Whether queue is empty
isFull()	None	Whether queue has reached capacity
size()	None	Current number of queued packets


Maintain one queue per router, for example:

Since RouterQueue needs a capacity when constructed, the team should initialize each router's queue explicitly rather than relying on default construction.

DSU interface

DSU uses integer IDs, so the integration layer must consistently map router names to integers. For example, R1 maps to 0, R2 to 1, and so on.

DSU is initialized using the network links. It can answer whether two routers are connected. After a link failure, rebuild it from the surviving links if arbitrary deletions are supported.


---

8. Main program — who calls whom?

Only main.cpp contains the program's main() function. It owns the integration logic and calls the modules through their public interfaces.

Here is the exact responsibility matrix:

Operation	Caller	Function called	Result used by

Load topology	main.cpp	graph.loadFromFile()	Member 1 / integration
Calculate route	Integration layer	graph.shortestPath()	Forwarding-table builder
Build forwarding table	Integration layer	trie.insert()	Member 2's Tries
Create packet	main.cpp	—	Source router queue
Buffer packet	main.cpp	routerQueues[id].enqueue()	Member 4
Schedule packet	main.cpp	heap.push(), heap.pop()	Member 3
Look up next hop	main.cpp	trie.longestPrefixMatch()	Forwarding logic
Check connectivity	main.cpp	dsu.connected()	Failure reporting


The forwarding-table builder is integration code in main.cpp or a separate helper module. It is not an additional responsibility that should be hidden inside Member 2's Trie implementation.

9. Complete runtime example

Suppose the program receives packet P101:

The destination IP belongs to the configured destination network 10.0.16.0/24.

1

Initialization

main.cpp loads input.txt. Your Dijkstra module calculates the shortest paths. The integration layer builds forwarding entries in each router's Trie.


---

2

Packet enters R1

Member 4's circular queue stores P101. The main program transfers it to the scheduling heap when R1 is ready to process packets.


---

3

Select and forward

Member 3's heap selects P101. The main program queries R1's Trie with 10.0.16.25. The Trie returns R3, so the packet is placed in R3's queue.


---

4

Repeat at each router

The packet is processed at R3, R4, R8, and R12. Each router's Trie returns the next hop configured from the shortest route.


---

5

Delivery

At R16, the main program recognizes that the destination router has been reached and marks P101 as delivered. It records the route and total cost.

The expected route for P101 is:

R1 → R3 → R4 → R8 → R12 → R16

Total cost: 16.

This is a deterministic example using the current topology. The priority heap determines the processing order of competing packets; it does not change Dijkstra's route.

10. Input file formats

input.txt — already implemented by you

The first line gives the number of routers and links. Each subsequent line contains a bidirectional link and its cost.

routing_config.txt — destination IP mapping

Use one line per destination network:

This format means destinationRouter networkAddress prefixLength. The configuration must define all destination networks used by the simulator. Each router's forwarding table is generated from the shortest paths to these destinations.

packets.txt — sample packet workload

Format:

The main program reads each line and creates a Packet. Member 3's heap uses the priority, and Member 2's Trie uses the destination IP.

11. Required test cases

Before integration, each member should verify their module independently.

Member	Minimum tests

Member 1	Known shortest path, unreachable destination, unknown router, malformed topology
Member 2	Exact prefix match, overlapping prefixes, longest-prefix selection, no matching prefix
Member 3	Correct priority order, equal-priority tie-breaking, empty heap
Member 4	Queue full/empty cases, circular wraparound, DSU connectivity, connectivity after a simulated failure


Then the integrated program should test:

A packet delivered successfully.

A destination unreachable from its source.

A packet dropped because a router queue is full.

Multiple packets with different priorities.

A link failure followed by route recomputation and forwarding-table updates.


12. Implementation order for your team

Follow this order to minimize integration problems.

1. You: finalize dijkstra.h, dijkstra.cpp, and getNextHop().


2. All members: agree on common.h and commit it to the shared repository.


3. Member 2: implement the Trie and test longest-prefix matching independently.


4. Member 3: implement the heap using the shared Packet structure.


5. Member 4: implement the circular queue and DSU independently.


6. Integration: build the forwarding-table initialization logic, then implement the main packet-processing loop.


7. Testing: run the complete simulation with input.txt, routing_config.txt, and packets.txt.



One architectural caveat: a Trie is only useful here if the team actually uses destination IP prefixes for forwarding. If your assignment only requires routing between named routers, Dijkstra already supplies the next hop, and the Trie would otherwise duplicate that job. Confirm that IP-prefix forwarding is a project requirement before investing in the full integration.