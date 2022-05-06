# Concurrent Queue
Some `ConcurrentQueue` types are provided in this directory:

| Class Name | Channel Type | Is Bounded | Header File | Description |
| --- | --- | --- | --- | --- |
| [`sc::mpmc::LinkedListQueue`](./mpmc_list_queue.hpp) | MPMC | Unbounded | `queue/mpmc_list_queue.hpp` | Implemented using the single linked-list. |
| [`sc::mpmc::ArrayListQueue`](./mpmc_array_queue.hpp) | MPMC | Unbounded | `queue/mpmc_array_queue.hpp` | Implemented using array + single linked-list. |
| [`sc::mpmc::LinkedListQueueV2`](./mpmc_list_queue_v2.hpp) | MPMC | Unbounded | `queue/mpmc_list_queue_v2.hpp` | Implemented using single linked-list, but memory is managed by `std::atomic<std::shared_ptr>`. |



