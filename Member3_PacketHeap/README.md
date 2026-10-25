# Member 3 — Binary Max-Heap + Packet Scheduler

This member owns the packet scheduler priority queue. It decides which waiting packet gets processed next when multiple packets compete for a router.

Responsibilities:
- Push packets into a max-heap ordered by priority.
- Return the highest-priority packet without removing it.
- Remove and return the highest-priority packet.
- Expose heap state via size and emptiness checks.
- Resolve ties by lower packet ID.

Required integration contract:
- `void push(const Packet& packet);`
- `Packet pop();`
- `const Packet& top() const;`
- `bool empty() const;`
- `int size() const;`

Important:
- Larger numeric priority means earlier service.
- The heap controls scheduling order, not routing path selection.
- `main.cpp` transfers packets from a router queue into a scheduler heap before processing.
