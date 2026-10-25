# Member 4 — Router Queue + DSU

This member owns the bounded circular router queues and the connectivity logic used for failure checks.

Responsibilities:
- Maintain one queue per router with a fixed capacity.
- Enqueue and dequeue packets with circular buffer wraparound logic.
- Track queue emptiness, fullness, and size.
- Model router connectivity with DSU on integer router IDs.
- Rebuild connectivity state after link failures when required.

Required integration contract:
- `RouterQueue(int capacity)`
- `bool enqueue(const Packet& packet);`
- `bool dequeue(Packet& packet);`
- `bool isEmpty() const;`
- `bool isFull() const;`
- `int size() const;`
- `DSU(int numberOfRouters);`
- `int find(int routerId);`
- `void unite(int routerA, int routerB);`
- `bool connected(int routerA, int routerB);`

Important:
- One queue per router should be explicitly constructed with a capacity.
- The DSU uses integer IDs and should be rebuilt after link failures.
- The main program coordinates packet movement and failure reporting, not the queue implementation itself.
