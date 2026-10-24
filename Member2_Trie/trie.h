#ifndef MEMBER2_TRIE_H
#define MEMBER2_TRIE_H

#include <string>
#include <unordered_map>

#include "../common.h"

using namespace std;

class Trie {
public:
    void insert(
        const string& prefix,
        int prefixLength,
        const string& nextHop
    );

    string longestPrefixMatch(
        const string& destinationIP
    ) const;
};

extern unordered_map<string, Trie> routingTables;

#endif
