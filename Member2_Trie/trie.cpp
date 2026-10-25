
#include "trie.h"

#include <sstream>
#include <stdexcept>
#include <cctype>

unordered_map<string, Trie> routingTables;

Trie::Trie() : nodes(1) {
    // Start with one empty root node.
}

// Convert dotted-decimal IPv4 into a 32-character binary string.
static string ipToBinary(const string& ip) {
    if (ip.empty() || ip.back() == '.') {
        throw invalid_argument("Invalid IPv4 address: " + ip);
    }

    stringstream stream(ip);
    string part;
    string binary;
    int octetCount = 0;

    while (getline(stream, part, '.')) {
        if (part.empty() || part.size() > 3) {
            throw invalid_argument("Invalid IPv4 address: " + ip);
        }

        for (unsigned char c : part) {
            if (!isdigit(c)) {
                throw invalid_argument("Invalid IPv4 address: " + ip);
            }
        }

        int octet;

        try {
            octet = stoi(part);
        } catch (...) {
            throw invalid_argument("Invalid IPv4 address: " + ip);
        }

        if (octet < 0 || octet > 255) {
            throw invalid_argument("Invalid IPv4 address: " + ip);
        }

        for (int bit = 7; bit >= 0; --bit) {
            binary += ((octet >> bit) & 1) ? '1' : '0';
        }

        ++octetCount;
    }

    if (octetCount != 4) {
        throw invalid_argument("Invalid IPv4 address: " + ip);
    }

    return binary;
}

void Trie::insert(
    const string& prefix,
    int prefixLength,
    const string& nextHop
) {
    if (prefixLength < 0 || prefixLength > 32) {
        throw invalid_argument(
            "Prefix length must be between 0 and 32"
        );
    }

    if (nextHop.empty()) {
        throw invalid_argument("Next hop cannot be empty");
    }

    string binary = ipToBinary(prefix);
    int current = 0;

    // Insert only the number of bits specified by the prefix length.
    for (int i = 0; i < prefixLength; ++i) {
        int bit = binary[i] - '0';
        int next = nodes[current].child[bit];

        if (next == -1) {
            next = static_cast<int>(nodes.size());

            nodes[current].child[bit] = next;
            nodes.emplace_back();
        }

        current = next;
    }

    // Mark this node as a complete route.
    nodes[current].nextHop = nextHop;
    nodes[current].isEnd = true;
}

string Trie::longestPrefixMatch(
    const string& destinationIP
) const {
    string binary = ipToBinary(destinationIP);

    int current = 0;
    string bestMatch;

    // The root stores a /0 default route, if one exists.
    if (nodes[current].isEnd) {
        bestMatch = nodes[current].nextHop;
    }

    // Follow destination bits and retain the deepest matching route.
    for (char c : binary) {
        int bit = c - '0';
        int next = nodes[current].child[bit];

        if (next == -1) {
            break;
        }

        current = next;

        if (nodes[current].isEnd) {
            bestMatch = nodes[current].nextHop;
        }
    }

    // Empty string means no route matched.
    return bestMatch;
}
