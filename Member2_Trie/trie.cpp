#include "trie.h"

unordered_map<string, Trie> routingTables;

void Trie::insert(
    const string& prefix,
    int prefixLength,
    const string& nextHop
) {
    (void)prefix;
    (void)prefixLength;
    (void)nextHop;
}

string Trie::longestPrefixMatch(
    const string& destinationIP
) const {
    (void)destinationIP;
    return "";
}
