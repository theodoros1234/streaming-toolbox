#include "deregistrationqueue.h"

template<> DeregistrationQueue<class T>::~DeregistrationQueue() {
    // NOTE: The queue should ONLY be destroyed AFTER all registrants have deregistered
    //       The destructor assumes that this condition is met.
    std::unique_lock<std::mutex> guard(this->lock);
    // Let the parent waiting thread know that we're destroying the queue
    this->deleting = true;
    this->queue_wait.notify_one();
    // Wait until it's safe to destroy the queue
    while (block_deletion)
        this->delete_wait.wait(guard);
}

template<> void DeregistrationQueue<class T>::deregister(T* object) {
    std::unique_lock<std::mutex> guard(this->lock);
    std::condition_variable request_wait;
    // Send our deregistration request
    this->queue.push(std::pair<T*, std::condition_variable*>(object, &request_wait));
    // Wait for our request to be processed
    request_wait.wait(guard);
}

template<> void DeregistrationQueue<class T>::blockDeletion() {
    std::lock_guard<std::mutex> guard(this->lock);
    this->block_deletion = true;
}

template<> void DeregistrationQueue<class T>::allowDeletion() {
    std::lock_guard<std::mutex> guard(this->lock);
    this->block_deletion = false;
    this->delete_wait.notify_one();
}

template<> T* DeregistrationQueue<class T>::getRequest() {
    std::unique_lock<std::mutex> guard(this->lock);
    // Wait for requests to be made
    while (this->queue.empty()) {
        // Abort if queue is empty and being deleted
        if (deleting)
            return nullptr;
        // Otherwise, wait for request or queue deletion
        this->queue_wait.wait(guard);
    }
    // Return most recent request
    return this->queue.front().first;
}

template<> void DeregistrationQueue<class T>::finishRequest() {
    std::lock_guard<std::mutex> guard(this->lock);
    // Tell last requester that they've been deregistered by waking them up
    this->queue.front().second->notify_one();
    // Remove their request from the queue
    this->queue.pop();
}
