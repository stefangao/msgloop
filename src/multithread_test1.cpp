#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <list>

#include <sys/timeb.h>

typedef std::function<void(void*)> llCallbackFunc;

typedef struct _Msg
{
    llCallbackFunc callback;
    void* data;
} MsgInfo;

typedef struct _TimerMsg
{
    llCallbackFunc callback;
    int interval;
    std::chrono::system_clock::time_point timepoint;
    bool once;
    int id;
} TimerInfo;

static std::mutex gMainMtx;
static std::condition_variable gMainCv;

static std::mutex gTimerMtx;
static std::condition_variable gTimerCv;

static bool gFinished = false;

static std::queue<MsgInfo> gMsgQueue;
static std::list<TimerInfo> gTimerList;

void postMsg(const MsgInfo& msg)
{
    std::unique_lock < std::mutex > lck(gMainMtx);
    gMsgQueue.push(msg);
    gMainCv.notify_one();
}

void postCallback(std::function<void(void*)> func)
{
    MsgInfo msg;
    msg.data = nullptr;
    msg.callback = func;
    postMsg(msg);
}

void setTimer(int duration, bool once, std::function<void(void*)> func)
{
    TimerInfo timer;
    timer.interval = duration;
    timer.timepoint = std::chrono::system_clock::now()
            + std::chrono::milliseconds(duration);
    timer.once = once;
    timer.id = 0;
    timer.callback = func;

    std::unique_lock < std::mutex > lck(gTimerMtx);
    gTimerList.push_back(timer);
    gTimerCv.notify_one();
}

void do_timer_thread()
{
    std::unique_lock < std::mutex > lck(gTimerMtx);
    while (!gFinished)
    {
        gTimerList.sort([](const TimerInfo& timer1, const TimerInfo& timer2)->bool
                {
                    return timer1.timepoint < timer2.timepoint;
                });

        int interval = 0;
        TimerInfo timerInfo;
        auto iter = gTimerList.begin();
        if (iter != gTimerList.end())
        {
            timerInfo = *iter;
            interval = timerInfo.interval;
        }

        //std::cout << "timer wait begin\n";
        if (interval > 0)
        {
            std::cv_status cvsts = gTimerCv.wait_until(lck, timerInfo.timepoint);
            if (cvsts == std::cv_status::timeout)
            {
                MsgInfo msg;
                msg.data = nullptr;
                msg.callback = timerInfo.callback;
                postMsg(msg);
                if (!timerInfo.once)
                {
                    (*iter).timepoint = std::chrono::system_clock::now()
                            + std::chrono::milliseconds(interval);
                }
                else
                {
                    gTimerList.erase(iter);
                }
            }
        }
        else
        {
            gTimerCv.wait(lck);
        }
        //std::cout << "timer wait end\n";
    }

    std::cout << "timer thread exit\n";
}

void exitApp()
{
    gFinished = true;
    gTimerCv.notify_one();
}

int main()
{
    std::cout << "main start\n";

    std::thread timerThread;
    timerThread = std::thread(do_timer_thread);

    auto timeStart = std::chrono::system_clock::now();

    postCallback([timeStart](void* p)
    {
        setTimer(2020, false, [timeStart](void* p)
                {
                    auto timeEnd = std::chrono::system_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart);
                    std::cout << "---time out--(2)--" << duration.count() << "\n";
                });

        setTimer(1500, false, [timeStart](void* p)
                {
                    auto timeEnd = std::chrono::system_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart);
                    std::cout << "---time out--(1)--" << duration.count() << "\n";
                });

        setTimer(9000, true, [timeStart](void* p)
                {
                    auto timeEnd = std::chrono::system_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart);
                    std::cout << "---time out--(3)--" << duration.count() << "\n";
                    exitApp();
                });
    });

    std::unique_lock < std::mutex > lck(gMainMtx);
    while (!gFinished)
    {
        //std::cout << "wait begin\n";
        gMainCv.wait(lck, []
        {   return gMsgQueue.size() > 0;});
        //std::cout << "wait end\n";

        while (gMsgQueue.size() > 0)
        {
            MsgInfo& msg = gMsgQueue.front();
            msg.callback(msg.data);
            gMsgQueue.pop();
        }
    }
    timerThread.join();

    std::cout << "main exit\n";

    return 0;
}
