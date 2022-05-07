use std::sync::{Barrier, Arc};
use std::sync::atomic::{AtomicU64, Ordering};
use std::thread::{self, ThreadId};
use std::time::Instant;

use crossbeam::deque::Injector;
use crossbeam::deque::Steal::Success;

const LOOP_COUNT: i64 = 10_000_000;

pub fn concurrent_enqueue_dequeue() {
    #[derive(Copy, Clone, Debug)]
    struct Task {
        tid: ThreadId,
        consume_tid: ThreadId,
        task_id: i64,
        in_time: Instant,
        out_time: i64
    }

    let barrier = Arc::new(Barrier::new(6));
    let injector = Arc::new(Injector::new());
    let mut produce_threads = Vec::with_capacity(4);
    for _ in 0..4 {
        let c = Arc::clone(&barrier);
        let injector = Arc::clone(&injector);
        produce_threads.push(thread::spawn(move || {
            let tid = thread::current().id();
            println!("[Produce] Thread [{:?}] waiting...", tid);
            c.wait();

            let tick = Instant::now();

            for i in 0..LOOP_COUNT {
                injector.push(Task {
                    tid,
                    consume_tid: tid,
                    task_id: i,
                    in_time: Instant::now(),
                    out_time: 0
                });
            }

            let elapsed = tick.elapsed();
            println!("[Produce] Thread [{:?}] finished. total time: {:?}ns", tid, elapsed.as_nanos());
        }));
    }

    let mut consume_threads = Vec::with_capacity(2);
    let counter = Arc::new(AtomicU64::new(0));
    for _ in 0..2 {
        let c = Arc::clone(&barrier);
        let counter = Arc::clone(&counter);
        let injector = Arc::clone(&injector);
        consume_threads.push(thread::spawn(move || {
            let tid = thread::current().id();
            println!("[Consume] Thread [{:?}] waiting...", tid);
            c.wait();

            let mut result = Vec::with_capacity(TOTAL as usize / 4 * 3);
            let tick = Instant::now();

            const TOTAL: u64 = 4 * LOOP_COUNT as u64;
            while counter.load(Ordering::Acquire) != TOTAL {
                let task = injector.steal();
                if let Success(mut t) = task {
                    counter.fetch_add(1, Ordering::AcqRel);
                    t.consume_tid = tid;
                    t.out_time = t.in_time.elapsed().as_nanos() as i64;
                    result.push(t);
                }
            }

            let elapsed = tick.elapsed();
            println!("[Consume] Thread [{:?}] finished. total time: {:?}ns", tid, elapsed.as_nanos());

            result
        }));
    }

    for t in produce_threads {
        t.join().unwrap();
    }
    for t in consume_threads {
        let tid = t.thread().id();
        let r = t.join().unwrap();
        println!("Thread [{:?}] result size: {}", tid, r.len());
    }
}
