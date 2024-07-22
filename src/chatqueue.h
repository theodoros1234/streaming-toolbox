#ifndef CHATQUEUE_H
#define CHATQUEUE_H

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "chatmessage.h"

class ChatQueue {
private:
    std::queue<ChatMessage> queue;
    std::mutex lock;
    std::condition_variable wait;
    bool allow_deletion = true;
    bool deleting = false;
    std::mutex deletion_lock;
    std::condition_variable deletion_wait;
public:
    ChatQueue();
    ~ChatQueue();
    bool isEmpty();
    int size();
    void push(ChatMessage &message);
    void push(std::vector<ChatMessage> &messages);
    std::vector<ChatMessage> pull();
    std::vector<ChatMessage> pullInstantly();
    void blockDeletion();
    void allowDeletion();
};

#endif // CHATQUEUE_H
