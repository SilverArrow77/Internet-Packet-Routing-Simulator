#include "router_queue.h"

RouterQueue::RouterQueue(int capacity) {
    (void)capacity;
}

bool RouterQueue::enqueue(const Packet& packet) {
    (void)packet;
    return false;
}

bool RouterQueue::dequeue(Packet& packet) {
    (void)packet;
    return false;
}

bool RouterQueue::isEmpty() const {
    return true;
}

bool RouterQueue::isFull() const {
    return false;
}

int RouterQueue::size() const {
    return 0;
}
