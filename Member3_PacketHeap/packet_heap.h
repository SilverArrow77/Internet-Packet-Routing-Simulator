#ifndef MEMBER3_PACKET_HEAP_H
#define MEMBER3_PACKET_HEAP_H

#include <vector>

#include "../common.h"

using namespace std;

class PacketHeap {
public:
    void push(const Packet& packet);
    Packet pop();
    const Packet& top() const;
    bool empty() const;
    int size() const;
};

#endif
