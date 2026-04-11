// TpThreadPool.cpp
// Created by Enze Jin on 2022/9/4.
//
#include <thread>
#include <atomic>
#include <mutex>
#include <list>
#include <condition_variable>
#include "TpThreadPool/TpThreadPool.h"

namespace Tp
{
class TpThread;

struct TpThreadPool::TpThreadPoolPrivates
{
    TpThreadPool&          mother;
    std::list<TpThread*>   ths{};
    std::mutex             mt_ths{};
    std::list<TpTaskBase*> task_que{};
    std::mutex             mt_task_que{};
    int                    tid_counter{0};

    explicit TpThreadPoolPrivates(TpThreadPool& tpool): mother(tpool) {}

    TpTaskBase* fetchTaskByThread()
    {
        std::lock_guard<std::mutex> lck(mt_task_que);
        if (task_que.empty()) return nullptr;
        auto* ret = task_que.front();
        task_que.pop_front();
        if (mother.q_cnt_chg_cb) mother.q_cnt_chg_cb->operator()(task_que.size());
        return ret;
    }
};

class TpThread
{
public:
    explicit TpThread(const size_t t_id, TpThreadPool::TpThreadPoolPrivates& rel_poolmem): tid(t_id), related_poolmem(rel_poolmem), th(&TpThread::runner, this)
    {
        th.detach();
    }

    void runner()
    {
        while (true)
        {
            TpTaskBase*                  task_working = nullptr;
            std::unique_lock<std::mutex> lck_on(mt_on_duty);
            cv_on_duty.wait(lck_on, [this, &task_working]() {
                if (!pending_terminate)
                {
                    task_working = related_poolmem.fetchTaskByThread();
                    return task_working != nullptr;
                }
                return true;
            });
            lck_on.unlock();
            if (pending_terminate) break;
            current_task_is_disposal = (dynamic_cast<TpDisposalBase*>(task_working) != nullptr);
            task_base_atom           = task_working;
            task_working->invoke();
            // if throw, program abort
            // FIXME-1: between this, task_working may be deleted
            task_base_atom           = nullptr;
            current_task_is_disposal = false;
            // FIXED-1: by adding a flag current_task_is_disposal, 260410
        }
        std::unique_lock<std::mutex> lck_off(mt_off_duty);
        is_off_duty = true;
        cv_off_duty.notify_one();
    }

    void destory() const { delete this; }

    size_t                              tid;
    TpThreadPool::TpThreadPoolPrivates& related_poolmem;
    std::mutex                          mt_on_duty, mt_off_duty;
    std::condition_variable             cv_on_duty, cv_off_duty;
    std::atomic_bool                    pending_terminate{false}, is_off_duty{false};
    std::atomic<TpTaskBase*>            task_base_atom{nullptr};
    std::atomic_bool                    current_task_is_disposal{false};
    std::thread                         th;

private:
    // TpThread cannot build on stack
    ~TpThread()
    {
        pending_terminate = true;
        TpTaskBase* task  = task_base_atom.load();
        if (task && !current_task_is_disposal.load())
        {
            task->terminate();
        }
        cv_on_duty.notify_one();
        std::unique_lock<std::mutex> lck(mt_off_duty);
        cv_off_duty.wait(lck, [this]() {
            return is_off_duty.load();
        });
    }
};

TpThreadPool::TpThreadPool(size_t th_cnt, const bool use_auto)
{
    mem = new TpThreadPoolPrivates(*this);
    if (use_auto)
    {
        int hardware_thread_count = (int)std::thread::hardware_concurrency();
        if (hardware_thread_count <= 0) th_cnt = 0;
        else th_cnt = hardware_thread_count;
    }
    if (th_cnt == 0) throw std::runtime_error("Will create ZERO threads");
    std::lock_guard<std::mutex> lck(mem->mt_ths);
    mem->tid_counter = 0;
    for (size_t i = 0; i < th_cnt; i++)
    {
        mem->ths.emplace_back(new TpThread(mem->tid_counter++, *this->mem));
    }
}

size_t TpThreadPool::getThreadCount() const
{
    std::lock_guard<std::mutex> lck(mem->mt_ths);
    return mem->ths.size();
}
size_t TpThreadPool::getTaskCount() const
{
    std::lock_guard<std::mutex> lck(mem->mt_task_que);
    return mem->task_que.size();
}

void TpThreadPool::setTaskQueCountChangeCallback()
{
    delete q_cnt_chg_cb;
    q_cnt_chg_cb = nullptr;
}

size_t TpThreadPool::changeThreadCount(const size_t count, const bool force_kill_randomly) const
{
    if (count == 0) throw std::runtime_error("tp_threadpool_change_to_zero");
    std::lock_guard<std::mutex> lck(mem->mt_ths);
    if (count > mem->ths.size())
    {
        for (size_t i = mem->ths.size(); i < count; i++)
        {
            mem->ths.emplace_back(new TpThread(mem->tid_counter++, *this->mem));
        }
    }
    else if (count < mem->ths.size())
    {
        size_t to_kill = mem->ths.size() - count;
        for (auto it = mem->ths.begin(); it != mem->ths.end();)
        {
            if (to_kill == 0) break;
            if (!(*it)->task_base_atom.load())
            {
                (*it)->destory();
                it = mem->ths.erase(it);
                to_kill--;
            }
            else ++it;
        }
        if (to_kill && force_kill_randomly)
        { // to softly force kill
            for (auto it = mem->ths.begin(); it != mem->ths.end();)
            {
                if (to_kill == 0) break;
                (*it)->destory();
                it = mem->ths.erase(it);
                to_kill--;
            }
        }
    }
    return mem->ths.size();
}

void TpThreadPool::lockTaskQue() const
{
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable: 26111)
#endif
    // fix on 20240223
    // lock acquiring upside down
    for (auto* th : mem->ths) th->mt_on_duty.lock();
    mem->mt_task_que.lock();
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
}

void TpThreadPool::modifyTaskQueInsert(TpTaskBase* task) const
{
    mem->task_que.emplace_back(task);
    if (q_cnt_chg_cb) q_cnt_chg_cb->operator()(mem->task_que.size());
}

bool TpThreadPool::modifyTaskQueRetrieve(const TpTaskBase* task) const
{
    for (auto it = mem->task_que.begin(); it != mem->task_que.end(); ++it)
    {
        if (task == *it)
        {
            mem->task_que.erase(it);
            if (q_cnt_chg_cb) q_cnt_chg_cb->operator()(mem->task_que.size());
            return true;
        }
    }
    return false;
}

void TpThreadPool::unlockTaskQue() const
{
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable: 26110)
#endif
    // fix on 20240223
    // lock acquiring upside down
    mem->mt_task_que.unlock();
    for (auto* th : mem->ths) th->mt_on_duty.unlock();
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
}

void TpThreadPool::beckonThreads() const
{
    for (auto* th : mem->ths) th->cv_on_duty.notify_one();
}

TpThreadPool::~TpThreadPool()
{
    if (!mem) return;
    {
        std::lock_guard<std::mutex> lck(mem->mt_ths);
        for (const auto* th : mem->ths) th->destory();
    }
    delete q_cnt_chg_cb;
    delete mem;
}
} // namespace Tp