#include "router_queue.h"

RouterQueue::RouterQueue(int capacity)
    : buffer(capacity > 0 ? capacity : 0),
      head(0),
      tail(0),
      currentSize(0),
      capacity(capacity > 0 ? capacity : 0) {}

bool RouterQueue::enqueue(const Packet& packet) {
    if (isFull()) {
        return false;
    }

    buffer[tail] = packet;
    tail = (tail + 1) % capacity;
    currentSize++;
    return true;
}

bool RouterQueue::dequeue(Packet& packet) {
    if (isEmpty()) {
        return false;
    }

    packet = buffer[head];
    head = (head + 1) % capacity;
    currentSize--;
    return true;
}

bool RouterQueue::isEmpty() const {
    return currentSize == 0;
}

bool RouterQueue::isFull() const {
    return currentSize >= capacity;
}

int RouterQueue::size() const {
    return currentSize;
}

int RouterQueue::getCapacity() const {
    return capacity;
}

void RouterQueue::clear() {
    head = 0;
    tail = 0;
    currentSize = 0;
}
