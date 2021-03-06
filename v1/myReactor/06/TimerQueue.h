#ifndef _TIMERQUEUE_H_
#define _TIMERQUEUE_H_

#include <vector>
#include <set>

#include "notCopyable.h"
#include "Timestamp.h"
#include "Callbacks.h"
#include "Channel.h"
#include <sys/timerfd.h>

class EventLoop;
class Timer;
class TimerId;

class TimerQueue: public notCopyable{
public:
    TimerQueue(EventLoop* loop);
    ~TimerQueue();

    // schedules the callback to be run at given time
    TimerId addTimer(const TimerCallback& cb, Timestamp when, double interval);

    // void cancel(TimerId timerId);

private:
    // binary search tree
    // other options: linear list or priority queue based on binary heap(LinYaCool::Websever)
    typedef std::pair<Timestamp, Timer*> Entry;
    typedef std::set<Entry> TimerList;

    void addTimerInLoop(Timer* timer);
    // handle the readable timefd event
    void handleRead();

    // get the expired timer
    std::vector<Entry> getExpired(Timestamp now);
    void reset(const std::vector<Entry>& expired, Timestamp now);

    bool insert(Timer* timer);

    EventLoop* loop_;
    const int timerfd_;
    Channel timerfdChannel_;

    TimerList timers_;

};


#endif
