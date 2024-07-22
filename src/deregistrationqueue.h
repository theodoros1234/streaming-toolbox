#ifndef DEREGISTRATIONQUEUE_H
#define DEREGISTRATIONQUEUE_H

#include "deregistrationinterface.h"
#include <mutex>
#include <condition_variable>
#include <queue>

template <class T> class DeregistrationQueue : DeregistrationInterface <T> {
private:
    bool block_deletion = false, deleting = false;
    std::mutex lock;
    std::queue<std::pair<T*, std::condition_variable*> > queue;
    std::condition_variable queue_wait, delete_wait;
public:
    ~DeregistrationQueue();
    virtual void deregister(T *object);
    void blockDeletion();
    void allowDeletion();
    T* getRequest();
    void finishRequest();
};

#endif // DEREGISTRATIONQUEUE_H
