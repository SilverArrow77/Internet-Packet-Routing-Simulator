#include "packet_heap.h"

void PacketHeap::push(const Packet& packet) {
    (void)packet;
}

Packet PacketHeap::pop() {
    static Packet emptyPacket = {0, "", "", "", "", 0};
    return emptyPacket;
}

const Packet& PacketHeap::top() const {
    static Packet emptyPacket = {0, "", "", "", "", 0};
    return emptyPacket;
}

bool PacketHeap::empty() const {
    return true;
}

int PacketHeap::size() const {
    return 0;
}
