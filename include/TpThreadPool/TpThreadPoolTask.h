//
// Created by Deveinz on 2022/9/15.
//

#ifndef TPQTEST_THREADPOOLTASK_H
#define TPQTEST_THREADPOOLTASK_H
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "TpThreadPool.h"

namespace Tp
{
class TpDisposalTask final : public TpDisposalBase
{
public:
    // 20240907, forbidden create on stack
    template<typename FUN, typename... PARS>
    static TpDisposalTask* createInstance(FUN&& infunc, PARS&&... paras)
    {
        return new TpDisposalTask(infunc, paras...);
    }

    // Invoke
    //  by thread pool's thread
    //  don't invoke directly
    void invoke() override
    {
        func();
        delete this;
        // 20240907, do not suicide, you still have resp.
    }

private:
    std::function<void()> func;

    // 20240907, forbidden create on stack
    template<typename FUN, typename... PARS>
    explicit TpDisposalTask(FUN&& infunc, PARS&&... paras)
        : func(std::bind(std::forward<FUN>(infunc), std::forward<PARS>(paras)...))
    {}
};

template<typename RT>
class TpTask final : public TpTaskBase
{
public:
    std::atomic_bool has_done{false};

    template<typename FUN, typename... PARS>
    explicit TpTask(FUN&& infunc, PARS&&... paras)
        : func(std::bind(std::forward<FUN>(infunc), std::forward<PARS>(paras)...))
    {}

    // Set Terminator
    //  must be void()
    template<typename FUN, typename... PARS>
    void setTerminator(FUN&& infunc, PARS&&... paras)
    {
        func_terminator    = std::bind(std::forward<FUN>(infunc), std::forward<PARS>(paras)...);
        has_termerator_set = true;
    }

    // Invoke
    //  by thread pool's thread
    //  don't invoke directly
    void invoke() override
    {
        has_done = false;
        if constexpr (!std::is_void_v<RT>)
        {
            delete (RT*)result;
            result = new RT(func());
        }
        else func();
        std::unique_lock<std::mutex> lck(mt_cv);
        has_done = true;
        cv_run_finished.notify_one();
    }

    void terminate() override
    {
        if (has_termerator_set) func_terminator();
    }

    // Fetch Result
    //  will block the calling thread until the task result return
    std::conditional_t<std::is_void_v<RT>, void, std::add_lvalue_reference_t<RT>>
        fetchResult()
    {
        std::unique_lock<std::mutex> lck(mt_cv);
        cv_run_finished.wait(lck, [this]() {
            return has_done.load();
        });
        if constexpr (!std::is_void_v<RT>) return *(RT*)result;
        else return;
    }
    ~TpTask() override
    {
        if constexpr (!std::is_void_v<RT>) delete (RT*)result;
    }

private:
    std::condition_variable cv_run_finished;
    std::mutex              mt_cv;
    std::atomic_bool        has_termerator_set{false};
    void*                   result{nullptr};
    std::function<RT()>     func;
    std::function<void()>   func_terminator;
};
} // namespace Tp
#endif // TPQTEST_THREADPOOLTASK_H
