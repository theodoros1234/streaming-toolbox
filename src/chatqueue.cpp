#include "chatqueue.h"

ChatQueue::ChatQueue() {}

ChatQueue::~ChatQueue() {
    {
        // Mark for deletion and wake up thread that's waiting on this queue
        std::lock_guard<std::mutex> guard(this->lock);
        this->deleting = true;
        this->wait.notify_one();
    }
    {
        // Delay object destruction until we're told it's safe to do it
        std::unique_lock<std::mutex> guard(this->deletion_lock);
        while (!this->allow_deletion)
            this->deletion_wait.wait(guard);
    }
}

bool ChatQueue::isEmpty() {
    std::lock_guard<std::mutex> guard(this->lock);
    return this->queue.empty();
}

int ChatQueue::size() {
    std::lock_guard<std::mutex> guard(this->lock);
    return this->queue.size();
}

void ChatQueue::push(ChatMessage &message) {
    std::lock_guard<std::mutex> guard(this->lock);
    // Push message into queue
    queue.push(message);
    // Notify threads waiting for messages
    wait.notify_one();
}

void ChatQueue::push(std::vector<ChatMessage> &messages) {
    std::lock_guard<std::mutex> guard(this->lock);
    // Push messages into queue
    for (auto msg:messages)
        queue.push(msg);
    // Notify threads waiting for messages
    wait.notify_one();
}

std::vector<ChatMessage> ChatQueue::pull() {
    std::unique_lock<std::mutex> guard(this->lock);
    std::vector<ChatMessage> messages;
    // Abort if the queue is being deleted
    if (deleting)
        return messages;
    // Wait for new messages to come in, or for the queue to be deleted
    if (this->queue.empty())
        this->wait.wait(guard);
    // Abort if the interruption was due to the queue being deleted
    if (deleting)
        return messages;
    // Otherwise, grab new messages and return them
    while (!this->queue.empty()) {
        messages.push_back(this->queue.front());
        this->queue.pop();
    }
    return messages;
}

std::vector<ChatMessage> ChatQueue::pullInstantly() {
    std::lock_guard<std::mutex> guard(this->lock);
    std::vector<ChatMessage> messages;
    // Abort if the queue is being deleted
    if (deleting)
        return messages;
    // Grab any new messages (possibly none), and return them
    while (!this->queue.empty()) {
        messages.push_back(this->queue.front());
        this->queue.pop();
    }
    return messages;
}

void ChatQueue::blockDeletion() {
    std::lock_guard<std::mutex> guard(this->deletion_lock);
    allow_deletion = false;
}

void ChatQueue::allowDeletion() {
    std::lock_guard<std::mutex> guard(this->deletion_lock);
    allow_deletion = true;
    // Wake up destructor
    deletion_wait.notify_one();
}
