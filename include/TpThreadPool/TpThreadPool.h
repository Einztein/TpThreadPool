// TpThreadPool.h
// Created by Enze Jin on 2022/9/1.
//

// ReSharper disable CppUseTypeTraitAlias
#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <functional>

namespace Tp
{
class TpTaskBase
{
public:
    virtual void invoke() {}
    virtual void terminate() {}
    virtual ~TpTaskBase() = default;
};
class TpDisposalBase : public TpTaskBase
{};

class TpThreadPool
{
public:
    TpThreadPool()                    = delete;
    TpThreadPool(TpThreadPool&)       = delete;
    TpThreadPool(const TpThreadPool&) = delete;
    TpThreadPool(TpThreadPool&& tp2) noexcept: mem(tp2.mem), q_cnt_chg_cb(tp2.q_cnt_chg_cb)
    {
        tp2.mem          = nullptr;
        tp2.q_cnt_chg_cb = nullptr;
    }

    // Constructor
    //  with specified threads count
    //  or using auto to ignore th_cnt
    explicit TpThreadPool(size_t th_cnt, bool use_auto = false);

    size_t getThreadCount() const;
    size_t getTaskCount() const;

    // Set Task Queue Count Change Callback
    //  give a void(size_t) to enable
    //  or no argus to disable
    template<typename FUN, typename... PARS>
    void setTaskQueCountChangeCallback(FUN&& func, PARS&&... pars)
    {
        q_cnt_chg_cb = new std::function<void(size_t)>(std::bind(std::forward<FUN>(func), std::forward<PARS>(pars)..., std::placeholders::_1));
    }
    void setTaskQueCountChangeCallback();

    size_t changeThreadCount(size_t count, bool force_kill_randomly) const;

    void lockTaskQue() const;

    // Modify Task Queue: Insert
    //  to insert a task into the task queue
    //  must lock the task queue before modify
    void modifyTaskQueInsert(TpTaskBase* task) const;

    // Modify Task Queue: Retrieve
    //  to retrieve a task from the task queue, if has
    //  must lock the task queue before modify
    bool modifyTaskQueRetrieve(const TpTaskBase* task) const;

    // Unlock Task Queue
    //  must call from previous locking thread
    void unlockTaskQue() const;

    // Beckon Threads
    //  will notify all threads preparing to collect tasks
    //  must call before unlock the task queue
    void beckonThreads() const;

    ~TpThreadPool();

    // opaque inner struct
    struct TpThreadPoolPrivates;

private:
    TpThreadPoolPrivates*        mem;
    std::function<void(size_t)>* q_cnt_chg_cb{nullptr};
};
} // namespace Tp
#endif // THREADPOOL_H
