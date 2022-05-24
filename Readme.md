# SyncCell
This repo is intending to provide some concurrency containers and sync utilities that used in a C++ project.

Generally, all containers are designed to be lock-free, but they may be not always lock-free because of the compiler, platform, etc. For example:
* A type using the `std::atomic<i128>` may not be lock-free, if the CPU does not support an atomic operation on the size of double word.
* Even if the CPU supports, the compiler may still use a lock: static link to the `libatomic.a` library of `MinGW64` on Windows, the compiler will always use a lock because it does not know the runtime CPU info.

A lock-free type can also block the call: instead of block the thread and waiting a wakeup, they do a spin operation / while-CAS operation and that may spend a long time. So a lock-free type is not suitable for all the concurrency cases and sometimes using a "heavily" lock may have a better performance.
> Usually, high concurrent accesses will lead much more CAS failure, and in this case a lock like `Mutex` can get the same read/write performance but have a lower CPU usage.

The containers can be classified into four classes by the producer/consumer channel access type:
- **SPSC**: Single-Producer, Single-Consumer. Both the producer channel and consumer channel can only be accessed by single thread.
- **SPMC**: Single-Producer, Multiple-Consumer. Only one thread can access the producer channel, while the consumer channel can be accessed by multiple threads.
- **MPSC**: Multiple-Producer, Single-Consumer. Multiple threads can access the producer channel, only one thread can access the consumer channel.
- **MPMC**: Multiple-Producer, Multiple-Consumer. Both the producer channel and consumer channel can be accessed concurrently.

Each container in the repo has their special channel access convention, violating the convention is an undefined behavior, and will usually cause a logic or runtime error.

Unfortunately, there is no mechanism to prevent an object from being accessed by multiple threads in the C++ (In Rust we can. See [Rust Send and Sync](https://doc.rust-lang.org/nomicon/send-and-sync.html)). So it is the user's responsibility to guard that the access is satisfied.

Last but not the least, most container types may release the resources(memory) in their destructor, and usually the destructor is not thread safe so the user must do a synchronization between the read/write thread and the object construct/destruct thread to avoid the potential race condition.

## Containers
* Queue: see [queue](./queue).
  * [`sc::mpmc::LinkedListQueue`](./queue/mpmc_list_queue.hpp)
  * [`sc::mpmc::LinkedListQueueV2`](./queue/mpmc_list_queue_v2.hpp)
  * [`sc::mpmc::ArrayListQueue`](./queue/mpmc_array_queue.hpp)
  * [`sc::mpsc::LinkedListQueue`](./queue/mpsc_list_queue.hpp)
  * [`sc::BlockingQueue`](./queue/blocking_queue.hpp)

## Utilities
* `Backoff`: An utility to perform exponential backoff in spin loops. Ported from [crossbeam-util/Backoff](https://github.com/crossbeam-rs/crossbeam/blob/master/crossbeam-utils/src/backoff.rs).
* `CachePadded`: Pads and aligns a value to the length of a cache line. Inspired by **crossbeam-util/CachePadded**, and the API design is similar to the `std::optional`.
  > There is no rust auto-deref mechanism in C++, so the `operator->` and `operator*` are overload to simplify the access and make it behaves like a smart pointer.

# Build
The main source is header-only and users only need to add the repo's root directory path to the compiler's `include` list such as `-I` in GCC and `/I` in MSVC. If you use the CMake, just use `include_directories(${RepoPath})` to include the header path for all targets in the current directory or use `target_include_directories` to set header path for special target.

Some tests in the project need to build to run. All tests are under the `test/` folder and built with CMake.

```shell
cd path/to/repo
mkdir build & cd build

# build in a GNU-like env.
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j

# build using a generator (VS2022 target).
cmake -G "Visual Studio 17 2022" ..  # need a newer cmake version
# open the .sln file in the Visual Studio.

# let cmake select the compile toolchain and build.
cmake ..
cmake --build . --config Release
```
