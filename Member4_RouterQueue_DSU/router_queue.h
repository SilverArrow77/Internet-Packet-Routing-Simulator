#ifndef MEMBER4_ROUTER_QUEUE_H
#define MEMBER4_ROUTER_QUEUE_H

#include <vector>
#include "../common.h"

using namespace std;

class RouterQueue {
private:
    vector<Packet> buffer;
    int head;
    int tail;
    int currentSize;
    int capacity;

public:
    explicit RouterQueue(int capacity = 10);

    bool enqueue(const Packet& packet);
    bool dequeue(Packet& packet);

    bool isEmpty() const;
    bool isFull() const;
    int size() const;
    int getCapacity() const;
    void clear();
};

#endif
