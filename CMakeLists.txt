find_package(Threads REQUIRED)
set(TpThreadPool_SRC ThreadPool.cpp ThreadPool.h ThreadPoolTask.h)
add_library(TpThreadPool STATIC ${TpThreadPool_SRC})
target_link_libraries(TpThreadPool PRIVATE Threads::Threads)
