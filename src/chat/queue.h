#ifndef CHAT_QUEUE_H
#define CHAT_QUEUE_H

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "message.h"

namespace chat {

class queue {
private:
    std::queue<message> q;
    std::mutex lock;
    std::condition_variable wait;
    bool deletion_allowed = true;
    bool deleting = false;
    std::mutex deletion_lock;
    std::condition_variable deletion_wait;
public:
    queue();
    ~queue();
    bool empty();
    int size();
    void push(message &message);
    void push(std::vector<message> &messages);
    std::vector<message> pull();
    std::vector<message> pull_instantly();
    void block_deletion();
    void allow_deletion();
};

}

#endif // CHAT_QUEUE_H
