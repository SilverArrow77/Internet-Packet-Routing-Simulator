#ifndef MEMBER4_ROUTER_QUEUE_H
#define MEMBER4_ROUTER_QUEUE_H

#include "../common.h"

class RouterQueue {
public:
    explicit RouterQueue(int capacity);

    bool enqueue(const Packet& packet);
    bool dequeue(Packet& packet);

    bool isEmpty() const;
    bool isFull() const;
    int size() const;
};

#endif
