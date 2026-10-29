# Internet Packet Routing Simulator

A C++ network simulator that routes IPv4 packets across a weighted router
topology. It combines shortest-path routing, IP-prefix forwarding, bounded
router queues, priority-based packet scheduling, and connectivity analysis.
An optional browser interface replays simulation results on an interactive
network graph.

## Features

- **Weighted network topology:** Load routers and bidirectional links from a
  text file.
- **Shortest paths:** Use Dijkstra's algorithm to find routes and calculate
  their total cost.
- **IPv4 forwarding:** Build per-router forwarding tables and select next hops
  with longest-prefix matching.
- **Packet queues and scheduling:** Buffer up to 10 packets per router and
  process higher-priority packets first; ties go to the lower packet ID.
- **Connectivity and link failures:** Inspect connected components, take a
  direct link offline, and rebuild routes for the updated network.
- **Browser replay:** Explore packet paths, queue events, and reroute stress
  demonstrations in the frontend.

## Requirements

- A C++ compiler with C++11 support (for example, `g++`)
- Python 3, if you want to serve the browser frontend locally
- An internet connection for the frontend's Cytoscape.js and web-font CDN
  assets

## Build and run

Run these commands from the repository root so the simulator can find its
input files:

```sh
g++ -std=c++11 main.cpp Dijkstra/dijkstra.cpp Member2_Trie/trie.cpp Member3_PacketHeap/packet_heap.cpp Member4_RouterQueue_DSU/router_queue.cpp Member4_RouterQueue_DSU/dsu.cpp -o simulator
```

Start the interactive simulator:

```sh
./simulator
```

On Windows, run the generated executable in PowerShell:

```powershell
.\simulator.exe
```

The program loads `input.txt` and `routing_config.txt` at startup. Choose a
menu option to run a batch, enter a packet, inspect the network, or manage
packets and links interactively.

### Menu options

| Option | Action |
| --- | --- |
| 1 | Load `packets.txt`, simulate the packet workload, print results, and save `simulation.json` for browser replay. |
| 2 | Send one packet immediately using a source router and destination IPv4 address; save its result for browser replay. |
| 3 | Display the network topology and link costs. |
| 4 | Display router connectivity components. |
| 5 | Find and display a shortest route between two routers. |
| 6 | Queue a packet at a router. The destination router is resolved from its destination IP. |
| 7 | Execute the highest-priority queued packet at a router and forward it one hop. |
| 8 | Break a direct link between two routers, then rebuild forwarding tables and connectivity. |
| 9 | Exit. |

For options 6 and 7, a forwarded packet waits in the next router's queue until
that router is executed. Each router queue has a capacity of 10 packets.

## Browser visualization

Options 1 and 2 write `simulation.json` in the repository root. To view the
saved results, start a local static server from that directory:

```sh
python -m http.server 8000
```

Open [http://localhost:8000/frontend/](http://localhost:8000/frontend/) in a
browser. Choose a packet result or one of the three generated stress demos,
then select **Play simulation**. The demos illustrate queue overflow and
rerouting after a link failure. The browser animates the data recorded by the
C++ program; it does not calculate routes itself.

If the page cannot load `simulation.json`, use **Upload simulation.json** in
the page header to select it manually. The visualization uses Cytoscape.js and
web fonts from public CDNs.

## Input files

All input files are plain text. Keep them in the repository root, and run the
simulator from that directory.

### `input.txt` — network topology

The first line contains the router count and link count. Each remaining line
defines an undirected link and its positive cost:

```text
25 40
R1 R2 4
R1 R3 2
R1 R4 7
```

Router IDs in the link list must be among `R1` through `R<router-count>`.
Routers without links are still part of the network.

### `routing_config.txt` — destination networks

Each line maps a router to an IPv4 network and prefix length:

```text
R1 10.0.1.0 24
R2 10.0.2.0 24
R3 10.0.3.0 24
```

The simulator uses these entries to map destination IP addresses to routers and
to build longest-prefix forwarding tables.

### `packets.txt` — batch workload

Each line contains a packet ID, source router, destination router, destination
IPv4 address, and priority:

```text
101 R1 R16 10.0.16.25 3
102 R2 R12 10.0.12.10 5
```

The destination IP must match a network configured for the specified
destination router. Packet IDs in a workload must be unique.

## How routing works

1. The graph module loads the weighted topology and calculates shortest paths
   using Dijkstra's algorithm.
2. The simulator combines those paths with `routing_config.txt` to build a
   forwarding trie for each router.
3. At each hop, the current router's trie finds the most specific prefix that
   matches the packet's destination IP and returns its next hop.
4. The packet enters the next router's bounded queue. During simulation, a
   max-priority heap chooses which waiting packet is processed first.
5. Delivery, route cost, final router, and any drop reason are reported for
   each packet.

The priority affects processing order, not the route selected. Removing a
link updates the graph; forwarding tables and connectivity are then rebuilt
from the surviving links.

## Project structure

```text
.
├── main.cpp                         # Program entry point and integration
├── common.h                         # Shared packet and route data structures
├── input.txt                        # Sample weighted topology
├── packets.txt                      # Sample packet workload
├── routing_config.txt               # Sample router-to-network mapping
├── simulation.json                  # Browser replay data (generated by the app)
├── Dijkstra/
│   ├── dijkstra.h
│   ├── dijkstra.cpp                 # Graph and shortest-path algorithm
│   └── test_dijkstra.cpp            # Standalone graph tests
├── Member2_Trie/
│   ├── trie.h
│   └── trie.cpp                     # IPv4 longest-prefix lookup
├── Member3_PacketHeap/
│   ├── packet_heap.h
│   └── packet_heap.cpp              # Packet priority scheduler
├── Member4_RouterQueue_DSU/
│   ├── router_queue.h
│   ├── router_queue.cpp              # Bounded circular router queue
│   ├── dsu.h
│   ├── dsu.cpp                       # Disjoint-set connectivity tracking
│   └── test_member4.cpp             # Router queue and DSU tests
└── frontend/
    ├── index.html
    ├── app.js
    └── styles.css                    # Browser-based simulation replay
```

## Tests

The repository includes a standalone Dijkstra test program. Build and run it
from the repository root:

```sh
g++ -std=c++11 Dijkstra/test_dijkstra.cpp Dijkstra/dijkstra.cpp -o dijkstra_tests
./dijkstra_tests input.txt
```

On Windows, run `.\dijkstra_tests.exe input.txt`. The test checks topology
loading, a known shortest-path cost, unreachable and unknown routers, and
malformed topology input. A separate Member 4 test source is also included.
There is currently no unified test runner.
