#include "queue.h"

using namespace strtb;
using namespace strtb::chat;

queue::queue() {}

queue::~queue() {
    {
        // Mark for deletion and wake up thread that's waiting on this queue
        std::lock_guard<std::mutex> guard(this->lock);
        this->deleting = true;
        this->wait.notify_one();
    }
    {
        // Delay object destruction until we're told it's safe to do it
        std::unique_lock<std::mutex> guard(this->deletion_lock);
        while (!this->deletion_allowed)
            this->deletion_wait.wait(guard);
    }
}

bool queue::empty() {
    std::lock_guard<std::mutex> guard(this->lock);
    return this->q.empty();
}

int queue::size() {
    std::lock_guard<std::mutex> guard(this->lock);
    return this->q.size();
}

void queue::push(message &message) {
    std::lock_guard<std::mutex> guard(this->lock);
    // Push message into queue
    q.push(message);
    // Notify threads waiting for messages
    wait.notify_one();
}

void queue::push(std::vector<message> &messages) {
    std::lock_guard<std::mutex> guard(this->lock);
    // Push messages into queue
    for (auto msg : messages)
        q.push(msg);
    // Notify threads waiting for messages
    wait.notify_one();
}

std::vector<message> queue::pull() {
    std::unique_lock<std::mutex> guard(this->lock);
    std::vector<message> messages;
    // Abort if the queue is being deleted
    if (deleting)
        return messages;
    // Wait for new messages to come in, or for the queue to be deleted
    if (this->q.empty())
        this->wait.wait(guard);
    // Abort if the interruption was due to the queue being deleted
    if (deleting)
        return messages;
    // Otherwise, grab new messages and return them
    while (!this->q.empty()) {
        messages.push_back(this->q.front());
        this->q.pop();
    }
    return messages;
}

std::vector<message> queue::pull_instantly() {
    std::lock_guard<std::mutex> guard(this->lock);
    std::vector<message> messages;
    // Abort if the queue is being deleted
    if (deleting)
        return messages;
    // Grab any new messages (possibly none), and return them
    while (!this->q.empty()) {
        messages.push_back(this->q.front());
        this->q.pop();
    }
    return messages;
}

void queue::block_deletion() {
    std::lock_guard<std::mutex> guard(this->deletion_lock);
    deletion_allowed = false;
}

void queue::allow_deletion() {
    std::lock_guard<std::mutex> guard(this->deletion_lock);
    deletion_allowed = true;
    // Wake up destructor
    deletion_wait.notify_one();
}
