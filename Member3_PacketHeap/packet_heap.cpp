#include "packet_heap.h"
#include <stdexcept>
#include <utility>

bool PacketHeap::higherPriority(const Packet& a, const Packet& b) const {
    if (a.priority != b.priority) {
        return a.priority > b.priority;
    }

    return a.id < b.id;
}

void PacketHeap::siftUp(int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;

        if (!higherPriority(heap[index], heap[parent])) {
            break;
        }

        swap(heap[index], heap[parent]);
        index = parent;
    }
}

void PacketHeap::siftDown(int index) {
    int n = static_cast<int>(heap.size());

    while (true) {
        int highest = index;
        int left = 2 * index + 1;
        int right = 2 * index + 2;

        if (left < n && higherPriority(heap[left], heap[highest])) {
            highest = left;
        }

        if (right < n && higherPriority(heap[right], heap[highest])) {
            highest = right;
        }

        if (highest == index) {
            break;
        }

        swap(heap[index], heap[highest]);
        index = highest;
    }
}

void PacketHeap::push(const Packet& packet) {
    heap.push_back(packet);
    siftUp(static_cast<int>(heap.size()) - 1);
}

Packet PacketHeap::pop() {
    if (heap.empty()) {
        throw out_of_range("Cannot pop from an empty packet heap");
    }

    Packet result = heap.front();

    if (heap.size() == 1) {
        heap.pop_back();
        return result;
    }

    heap.front() = heap.back();
    heap.pop_back();

    siftDown(0);

    return result;
}

const Packet& PacketHeap::top() const {
    if (heap.empty()) {
        throw out_of_range("Cannot access top of an empty packet heap");
    }

    return heap.front();
}

bool PacketHeap::empty() const {
    return heap.empty();
}

int PacketHeap::size() const {
    return static_cast<int>(heap.size());
}
