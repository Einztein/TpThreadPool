find_package(Threads REQUIRED)

set(TpThreadPool_SRC src/ThreadPool.cpp)

add_library(TpThreadPool STATIC ${TpThreadPool_SRC})
add_library(TpThreadPool::TpThreadPool ALIAS TpThreadPool)

target_link_libraries(TpThreadPool PRIVATE Threads::Threads)

target_include_directories(TpThreadPool PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

