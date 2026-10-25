#ifndef MEMBER3_PACKET_HEAP_H
#define MEMBER3_PACKET_HEAP_H

#include <vector>

#if __has_include("../common.h")
#include "../common.h"
#elif __has_include("common.h")
#include "common.h"
#endif

using namespace std;

class PacketHeap {
private:
    vector<Packet> heap;

    bool higherPriority(const Packet& a, const Packet& b) const;
    void siftUp(int index);
    void siftDown(int index);

public:
    void push(const Packet& packet);
    Packet pop();
    const Packet& top() const;
    bool empty() const;
    int size() const;
};

#endif
