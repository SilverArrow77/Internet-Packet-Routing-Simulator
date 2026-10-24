# Member 2 — Trie + Forwarding Tables

This member owns the IPv4 prefix lookup logic. The trie is responsible only for longest-prefix matching and should not compute routes or link costs.

Responsibilities:
- Parse and store IPv4 prefixes.
- Insert routing entries in the form `prefix + prefixLength + nextHop`.
- Walk the trie according to the destination IP bits.
- Return the next hop for the longest matching prefix.
- Support multiple routers each with their own trie table.

Required integration contract:
- `void insert(const string& prefix, int prefixLength, const string& nextHop);`
- `string longestPrefixMatch(const string& destinationIP) const;`

Important:
- Keep trie logic focused on IP prefix matching only.
- `main.cpp` builds forwarding tables from the topology and configuration files.
- The trie does not determine shortest paths or packet priorities.
