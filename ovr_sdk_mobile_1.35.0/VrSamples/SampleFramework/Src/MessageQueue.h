/************************************************************************************

Filename    :   MessageQueue.h
Content     :   Thread communication by string commands
Created     :   October 15, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/
#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace OVRFW {

// This is a multiple-producer, single-consumer message queue.

class ovrMessageQueue {
   public:
    ovrMessageQueue(int maxMessages);
    ~ovrMessageQueue();

    // Shut down the message queue once messages are no longer polled
    // to avoid overflowing the queue on message spam.
    void Shutdown();

    // Thread safe, callable by any thread.
    // The msg text is copied off before return, the caller can free
    // the buffer.
    // The app will abort() with a dump of all messages if the message
    // buffer overflows.
    void PostString(const char* msg);
    // Builds a printf string and sends it as a message.
    void PostPrintf(const char* fmt, ...);
    // If there are at least requiredSpace slots available in the queue, builds a printf
    // string and sends it as a message.
    bool PostPrintfIfSpaceAvailable(int requiredSpace, const char* fmt, ...);

    // Same as above but these return false if the queue is full instead of an abort.
    bool TryPostString(const char* msg);
    bool TryPostPrintf(const char* fmt, ...);

    // Same as above but these wait until the message has been processed.
    // NOTE: these cannot be used by multiple producers simultaneously.
    void SendString(const char* msg);
    void SendPrintf(const char* fmt, ...);

    // Returns the number slots available for new messages.
    int SpaceAvailable() const {
        return maxMessages - (tail - head);
    }

    // The other methods are NOT thread safe, and should only be
    // called by the thread that owns the ovrMessageQueue.

    // Returns NULL if there are no more messages, otherwise returns
    // a string that the caller is now responsible for freeing.
    const char* GetNextMessage();

    // Returns immediately if there is already a message in the queue.
    void SleepUntilMessage();

    // Explicitly notify that a message has been processed.
    void NotifyMessageProcessed();

    // Dumps all unread messages
    void ClearMessages();

   private:
    // If set true, print all message sends and gets to the log
    static bool debug;

    bool shutdown;
    int maxMessages;

    struct message_t {
        const char* string;
        bool synced;
    };

    // All messages will be allocated with strdup, and returned to
    // the caller on GetNextMessage().
    message_t* messages;

    // PostMessage() fills in messages[tail%maxMessages], then increments tail
    // If tail > head, GetNextMessage() will fetch messages[head%maxMessages],
    // then increment head.
    volatile int head;
    volatile int tail;
    std::atomic<bool> synced;
    std::mutex message_mutex;
    std::condition_variable posted;
    std::condition_variable processed;

    bool PostMessage(const char* msg, bool sync, bool abortIfFull);
};

} // namespace OVRFW
