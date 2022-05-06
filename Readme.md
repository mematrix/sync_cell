# SyncCell
This repo is intending to provide some concurrency containers and sync utilities that used in a C++ project.

Generally, all containers are designed to be lock-free, but they may be not always lock-free because of the compiler, platform, etc. For example:
* A type used a `std::atomic<i128>` may be not lock-free, if the CPU does not support an atomic operation on the double word.
* Even if the CPU supports, the compiler may still use a lock: static link the `libatomic.a` library of `MinGW64` on Windows, the compiler will always use a lock because it does not know the runtime CPU info.

A lock-free type can also block the call: instead of block the thread and waiting a wakeup, they do a spin operation / while-CAS operation and that may spend a long time. So a lock-free type is not suitable for all the concurrency cases and sometimes using a "heavily" lock may have a better performance.
> Usually, high concurrent access will lead much more CAS failure, and in this case a lock like `Mutex` can get the same read/write performance but have a lower CPU usage.

So the containers can be classified to four classes by the produce/consume channel access type:
- **SPSC**: Single-Producer, Single-Consumer. Both the produce channel and consume channel can only be accessed by single thread.
- **SPMC**: Single-Producer, Multiple-Consumer. Only ane thread can access the produce channel, while the consume channel can be accessed by multiple threads.
- **MPSC**: Multiple-Producer, Single-Consumer. Multiple threads can access the produce channel, only one thread can access the consume channel.
- **MPMC**: Multiple-Producer, Multiple-Consumer. Both the produce channel and consume channel can be accessed concurrently.

## Containers
* Queue: see [queue](./queue/Readme.md).

## Utilities
* `Backoff`:
* `CachePadded`:

# Build
