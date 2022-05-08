# Concurrent Queue
Some `ConcurrentQueue` types are provided in this directory:

| Class Name | Channel Type | Is Bounded | Header File | Description |
| --- | --- | --- | --- | --- |
| [`sc::mpmc::LinkedListQueue`](./mpmc_list_queue.hpp) | MPMC | Unbounded | `queue/mpmc_list_queue.hpp` | Implemented using the single linked-list. |
| [`sc::mpmc::ArrayListQueue`](./mpmc_array_queue.hpp) | MPMC | Unbounded | `queue/mpmc_array_queue.hpp` | Implemented using array + single linked-list. |
| [`sc::mpmc::LinkedListQueueV2`](./mpmc_list_queue_v2.hpp) | MPMC | Unbounded | `queue/mpmc_list_queue_v2.hpp` | Implemented using single linked-list, but memory is managed by `std::atomic<std::shared_ptr>`. |

> The `sc::mpmc::ArrayListQueue` is ported from [the `Injector` of **crossbeam** project](https://github.com/crossbeam-rs/crossbeam/blob/master/crossbeam-deque/src/deque.rs) which is written in Rust.

From the following performance test result, some optimizations can be done:
* [ ] `sc::mpmc::LinkedListQueue` head pointer's tag can fold into the pointer itself.
* [ ] `LinkedListQueue` with **MPSC** type.
* [ ] Add a template param to `sc::mpmc::ArrayListQueu` to control the size of the inner cache pool?

# Simple Performance Test
A simple read/write test is done with each queue type, and as a comparison, a similar test is also write for the Rust version (`crossbeam-deque/Injector`) and Java version (`java.util.concurrent.ConcurrentLinkedQueue`).

The Rust test code is in [test/res/crossbeam_deque_test.rs](../test/res/crossbeam_deque_test.rs). Create a new binary package using the `Cargo`, edit the `Cargo.toml` and add the `crossbeam` dependence:
```toml
[dependencies]
crossbeam = "0.8"
```
Then copy the test file to the `src` directory, edit the `main.rs` like this:
```rust
extern crate crossbeam;

mod crossbeam_deque_test;
use crate::crossbeam_deque_test::concurrent_enqueue_dequeue;

fn main() {
    concurrent_enqueue_dequeue();
}
```

The Java test code is in [test/res/Test.java](../test/res/Test.java). Create a simple maven project and put the test file in the `src/main/java` directory, then add the `org.apache.commons.commons-lang3` dependence in the `pom.xml` file:
```xml
<dependency>
    <groupId>org.apache.commons</groupId>
    <artifactId>commons-lang3</artifactId>
    <version>3.12.0</version>
</dependency>
```

**For the MPMC queue, in all tests, unless otherwise specified, the producer thread's count is 4 and the consumer thread's count is 2.**

*Each produce thread will loop by 10,000,000 times.*

---

In addition, the single-threaded version was also tested for comparison. Test code is in [test/single_produce_test.cpp](../test/single_produce_test.cpp). Only do a put operation in the test.

---

**All C++ test executables are built with MSVC(VS2022) in Windows 11.**

Rust toolchain: stable-x86_64-pc-windows-gnu (1.60).

Java version:
```text
# java -version
openjdk version "17.0.1" 2021-10-19 LTS
OpenJDK Runtime Environment Zulu17.30+15-CA (build 17.0.1+12-LTS)
OpenJDK 64-Bit Server VM Zulu17.30+15-CA (build 17.0.1+12-LTS, mixed mode, sharing)
```

## Rust Version
One test result is like:
```text
[Produce] Thread [ThreadId(5)] finished. total time: 1576939800ns
[Produce] Thread [ThreadId(2)] finished. total time: 1590594900ns
[Produce] Thread [ThreadId(3)] finished. total time: 1609360500ns
[Produce] Thread [ThreadId(4)] finished. total time: 1637673100ns
[Consume] Thread [ThreadId(7)] finished. total time: 3127333400ns
[Consume] Thread [ThreadId(6)] finished. total time: 3127333600ns
Thread [ThreadId(6)] result size: 19837269
Thread [ThreadId(7)] result size: 20162731
```

The average put time of the producer thread is `~1600ms`, and the read time is `~3127ms`.

## Java Version
> There is a little difference between the Java test code and others: The Java code does not save and collect the read result.

One test result is like:
```text
put: thread=24,time=00:00:06.380
put: thread=23,time=00:00:06.392
put: thread=26,time=00:00:06.406
put: thread=25,time=00:00:06.425
read: thread=28,time=6648641300
read: thread=27,time=6648631700
put: thread=31,time=00:00:05.945
put: thread=30,time=00:00:05.950
put: thread=29,time=00:00:05.968
put: thread=32,time=00:00:05.968
read: thread=33,time=6496438400
read: thread=34,time=6496146100
......
put: thread=62,time=00:00:04.488
put: thread=60,time=00:00:04.544
put: thread=59,time=00:00:04.547
put: thread=61,time=00:00:04.549
read: thread=64,time=4929431100
read: thread=63,time=4928866700
put: thread=68,time=00:00:04.600
put: thread=65,time=00:00:04.606
put: thread=67,time=00:00:04.608
put: thread=66,time=00:00:04.626
read: thread=69,time=4958323200
read: thread=70,time=4958083600
```

> The test jar is run with `-server` option.

We can see that the average put time of the producer thread is `~6400ms` on the cold launch and the read time is `~6648ms`, after running for some time, the average cost time is gradually stable (because of the JIT and other JVM optimizations), the average put time is `~4500ms` and the read time is `~4900ms`.

## Single-threaded Version
One test result:
```text
[Single] Thread [15060] finished. total time: 494750800ns
```

The simple single-threaded implement runs very fast, the average time of put operation is `~490ms`.

## mpmc::LinkedListQueue
> The execution time of the consumer thread is very unstable. The time fluctuation is more than 1000ms.

One test result:
```text
[Produce] Thread [30024] finished. total time: 1507336000ns
[Produce] Thread [28828] finished. total time: 1508152000ns
[Produce] Thread [27780] finished. total time: 1539461300ns
[Produce] Thread [26924] finished. total time: 1544745200ns
[Consume] Consumer Thread [7124] finished. total time: 4945263400ns
[Consume] Consumer Thread [21496] finished. total time: 4945282900ns
```

The average execution time of the producer thread is `~1500ms`.

## mpmc::ArrayListQueue
One test result:
```text
[Produce] Thread [26732] finished. total time: 1396067100ns
[Produce] Thread [33080] finished. total time: 1403089400ns
[Produce] Thread [29016] finished. total time: 1431001400ns
[Produce] Thread [23024] finished. total time: 1444958900ns
[Consume] Consumer Thread [9080] finished. total time: 2983311000ns
[Consume] Consumer Thread [6108] finished. total time: 2983334600ns
```

The average put time of the producer thread is `~1400ms`, and the read time is `~3000ms`.

The average put/read time is slightly shorter than [the rust version](#rust-version), I think the possible reasons are:
* The Rust toolchain is gnu version but not the msvc. This may lead a difference performance.
* A simple memory cache pool is used to reduce frequent memory allocation and release operations in the C++ implementation code.

## mpmc::LinkedListQueueV2
> **Note**: Sometime the test process will run in crash because of the stack overflow problem. debug ing...

One test result:
```text
[Produce] Thread [8480] finished. total time: 2117627700ns
[Produce] Thread [28472] finished. total time: 2122792900ns
[Produce] Thread [6964] finished. total time: 2139098900ns
[Produce] Thread [30580] finished. total time: 2142942200ns
[Consume] Consumer Thread [31352] finished. total time: 13247829200ns
[Consume] Consumer Thread [31512] finished. total time: 13247834800ns
```
