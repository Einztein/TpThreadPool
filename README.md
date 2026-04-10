# TpThreadPool

A lightweight C++ thread pool library built on top of the C++ standard threading facilities.

`TpThreadPool` provides a low-level but flexible task scheduling model with:

- configurable worker thread count
- runtime thread count adjustment
- typed tasks with return values
- fire-and-forget disposable tasks
- task queue size change callback support

> Namespace: `Tp`

---

## Features

- Supports tasks with return values via `Tp::TpTask<T>`
- Supports self-destroying one-shot tasks via `Tp::TpDisposalTask`
- Manual task queue control for custom scheduling workflows
- Dynamic worker count adjustment at runtime

---

## Project Structure

```text
TpThreadPool/
├─ CMakeLists.txt
├─ include/
│  └─ TpThreadPool/
│     ├─ TpThreadPool.h
│     └─ TpThreadPoolTask.h
└─ src/
   └─ TpThreadPool.cpp
```

---

## Requirements

- CMake
- A C++17 Compatible compiler
- Thread library support

Although the provided `CMakeLists.txt` does not explicitly set the C++ standard, the code uses C++17 features such as `if constexpr`, so C++17 or later is recommended.

---

## Build

This library is currently best used through `add_subdirectory`.

### CMake

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(TpThreadPool)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE TpThreadPool::TpThreadPool)
```

The library target exposed by this project is:

```cmake
TpThreadPool::TpThreadPool
```

---

## Quick Start

### Include headers

```cpp
#include <TpThreadPool/TpThreadPool.h>
#include <TpThreadPool/TpThreadPoolTask.h>
```

### Create a thread pool

```cpp
Tp::TpThreadPool pool(4);
```

This creates a pool with 4 worker threads.

---

## Core Design

This library exposes a relatively low-level API.

Instead of a built-in high-level `submit()` interface returning `std::future`, tasks are manually inserted into the internal queue by the user:

1. lock the task queue
2. insert a task
3. notify worker threads
4. unlock the task queue

A typical submission flow looks like this:

```cpp
pool.lockTaskQue();
pool.modifyTaskQueInsert(task);
pool.beckonThreads();
pool.unlockTaskQue();
```

Because of this design, it is recommended to wrap submission logic in your own helper function.

---

## Task Types

### `Tp::TpTask<RT>`

A typed task that stores a callable and optionally returns a result.

- supports any return type `RT`, including `void`
- `fetchResult()` blocks until the task finishes
- lifetime is managed by the caller

Example:

```cpp
auto* task = new Tp::TpTask<int>([] {
    return 42;
});
```

For `void` tasks:

```cpp
auto* task = new Tp::TpTask<void>([] {
    // do work
});
```

---

### `Tp::TpDisposalTask`

A fire-and-forget task type.

- created through `createInstance(...)`
- automatically deletes itself after execution
- intended for tasks where no result is needed

Example:

```cpp
auto* task = Tp::TpDisposalTask::createInstance([] {
    // background work
});
```

---

## Usage

## 1. Submit a task with a return value

```cpp
#include <iostream>
#include <TpThreadPool/TpThreadPool.h>
#include <TpThreadPool/TpThreadPoolTask.h>

static void submit(Tp::TpThreadPool& pool, Tp::TpTaskBase* task)
{
    pool.lockTaskQue();
    pool.modifyTaskQueInsert(task);
    pool.beckonThreads();
    pool.unlockTaskQue();
}

int main()
{
    Tp::TpThreadPool pool(4);

    auto* task = new Tp::TpTask<int>([] {
        return 1 + 2;
    });

    submit(pool, task);

    int result = task->fetchResult();
    std::cout << "result = " << result << std::endl;

    delete task;
    return 0;
}
```

---

## 2. Submit a fire-and-forget task

```cpp
#include <iostream>
#include <TpThreadPool/TpThreadPool.h>
#include <TpThreadPool/TpThreadPoolTask.h>

static void submit(Tp::TpThreadPool& pool, Tp::TpTaskBase* task)
{
    pool.lockTaskQue();
    pool.modifyTaskQueInsert(task);
    pool.beckonThreads();
    pool.unlockTaskQue();
}

int main()
{
    Tp::TpThreadPool pool(2);

    submit(pool, Tp::TpDisposalTask::createInstance([] {
        std::cout << "hello from worker thread" << std::endl;
    }));

    return 0;
}
```

---

## 3. Wait for a `void` task

```cpp
auto* task = new Tp::TpTask<void>([] {
    // do something
});

submit(pool, task);
task->fetchResult();  // waits until completion
delete task;
```

---

## Queue Size Change Callback

The thread pool can invoke a callback whenever the queue size changes.

### Set callback

```cpp
pool.setTaskQueCountChangeCallback([](size_t count) {
    std::cout << "queue size: " << count << std::endl;
});
```

### Clear callback

```cpp
pool.setTaskQueCountChangeCallback();
```

---

## Resize the Thread Pool

The worker count can be changed at runtime.

### Increase thread count

```cpp
pool.changeThreadCount(8, false);
```

### Decrease thread count

```cpp
pool.changeThreadCount(2, false);
```

### Meaning of `force_kill_randomly`

```cpp
pool.changeThreadCount(new_count, force_kill_randomly);
```

- `false`  
  Only idle workers are removed when shrinking.

- `true`  
  If there are not enough idle workers, the pool will continue removing workers more aggressively.

The function returns the actual number of remaining threads.

---

## Auto Thread Count Mode

Constructor:

```cpp
Tp::TpThreadPool(size_t th_cnt, bool use_auto = false);
```

When `use_auto` is `true`, the actual thread count is computed as:

```cpp
std::thread::hardware_concurrency() - th_cnt
```

Example:

```cpp
Tp::TpThreadPool pool(1, true);
```

This means:

- reserve 1 hardware thread
- use the rest for the pool

If the computed count becomes `0`, the implementation falls back to `1`.

---

## Task Queue Control

The public API exposes queue operations directly.

### Lock the queue

```cpp
pool.lockTaskQue();
```

### Insert a task

```cpp
pool.modifyTaskQueInsert(task);
```

### Retrieve a task from the queue

```cpp
bool removed = pool.modifyTaskQueRetrieve(task);
```

This removes a task from the waiting queue if it has not started yet.

### Notify workers

```cpp
pool.beckonThreads();
```

### Unlock the queue

```cpp
pool.unlockTaskQue();
```

---

## API Overview

## `Tp::TpThreadPool`

### Construction

```cpp
explicit TpThreadPool(size_t th_cnt, bool use_auto = false);
```

### Deleted operations

```cpp
TpThreadPool() = delete;
TpThreadPool(TpThreadPool&) = delete;
TpThreadPool(const TpThreadPool&) = delete;
```

### Move construction

```cpp
TpThreadPool(TpThreadPool&&) noexcept;
```

### Query

```cpp
size_t getThreadCount() const;
size_t getTaskCount() const;
```

### Queue callback

```cpp
template<typename FUN, typename... PARS>
void setTaskQueCountChangeCallback(FUN&& func, PARS&&... pars);

void setTaskQueCountChangeCallback();
```

### Thread count management

```cpp
size_t changeThreadCount(size_t count, bool force_kill_randomly) const;
```

### Queue management

```cpp
void lockTaskQue() const;
void modifyTaskQueInsert(TpTaskBase* task) const;
bool modifyTaskQueRetrieve(const TpTaskBase* task) const;
void beckonThreads() const;
void unlockTaskQue() const;
```

---

## `Tp::TpTask<RT>`

### Construction

```cpp
template<typename FUN, typename... PARS>
explicit TpTask(FUN&& infunc, PARS&&... paras);
```

### Optional termination callback

```cpp
template<typename FUN, typename... PARS>
void setTerminator(FUN&& infunc, PARS&&... paras);
```

### Wait for completion and fetch result

```cpp
auto fetchResult();
```

For non-`void` tasks, this returns a reference to the stored result.  
For `void` tasks, this blocks until the task finishes and returns nothing.

---

## `Tp::TpDisposalTask`

### Factory creation

```cpp
template<typename FUN, typename... PARS>
static TpDisposalTask* createInstance(FUN&& infunc, PARS&&... paras);
```

This type is intentionally created on the heap and destroys itself after execution.

---

## Minimal Example

```cpp
#include <iostream>
#include <TpThreadPool/TpThreadPool.h>
#include <TpThreadPool/TpThreadPoolTask.h>

static void submit(Tp::TpThreadPool& pool, Tp::TpTaskBase* task)
{
    pool.lockTaskQue();
    pool.modifyTaskQueInsert(task);
    pool.beckonThreads();
    pool.unlockTaskQue();
}

int main()
{
    Tp::TpThreadPool pool(4);

    auto* task = new Tp::TpTask<int>([] {
        return 2024;
    });

    submit(pool, task);

    std::cout << task->fetchResult() << std::endl;
    delete task;

    submit(pool, Tp::TpDisposalTask::createInstance([] {
        std::cout << "background task finished" << std::endl;
    }));

    return 0;
}
```

---

## Memory Management Notes

### `TpTask<T>` must be manually deleted

The pool executes `TpTask<T>`, but does not own it.

```cpp
auto* task = new Tp::TpTask<int>([] { return 7; });
submit(pool, task);

int value = task->fetchResult();
delete task;
```

---

### `TpDisposalTask` deletes itself after execution

You should not manually delete it after successful execution.

However, if you remove it from the queue before it starts, you are responsible for destroying it yourself.

---

## Important Notes

- Always follow the queue operation order:

  ```cpp
  lockTaskQue() -> modifyTaskQueInsert() -> beckonThreads() -> unlockTaskQue()
  ```

- `fetchResult()` blocks until the task completes.

- `TpTask<RT>::fetchResult()` returns a reference for non-`void` tasks, so it is generally safest to copy the returned value unless you control the task lifetime carefully.

- This library intentionally exposes lower-level queue controls instead of a future-based high-level API.

- The current CMake configuration builds a static library and exposes the include directory, but does not currently include install rules.

---

## Current Characteristics

Based on the current implementation, users should be aware of the following:

- no `std::future` / `std::promise` integration
- no built-in `submit()` helper
- no task priority system
- no batch submission API
- no install/export packaging setup in CMake
- some task lifetimes are managed manually by the caller

---

## Suggested Wrapper

In real projects, it is recommended to define a helper like this:

```cpp
inline void submit(Tp::TpThreadPool& pool, Tp::TpTaskBase* task)
{
    pool.lockTaskQue();
    pool.modifyTaskQueInsert(task);
    pool.beckonThreads();
    pool.unlockTaskQue();
}
```

This keeps call sites simpler and reduces misuse.

---

## Status

This project provides a compact and usable thread pool core with explicit queue control and typed task support. It is a good fit if you want:

- a small dependency-free thread pool
- direct access to queue behavior
- simple task execution without introducing futures or larger frameworks
