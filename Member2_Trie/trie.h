#ifndef MEMBER2_TRIE_H
#define MEMBER2_TRIE_H

#include <string>
#include <unordered_map>
#include <vector>

#include "../common.h"

using namespace std;

class Trie {
private:
    struct TrieNode {
        int child[2];
        string nextHop;
        bool isEnd;

        TrieNode() : child{-1, -1}, isEnd(false) {}
    };

    // Index 0 is the root node.
    // -1 means that a child does not exist.
    vector<TrieNode> nodes;

public:
    Trie();

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
