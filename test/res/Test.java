import org.apache.commons.lang3.time.StopWatch;

import java.io.IOException;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.stream.IntStream;

public class Test {
    public static void main(String[] args) throws IOException, InterruptedException {
        while (true) {
            var t = new Test();
            t.run();
            Thread.sleep(10000);
        }
//        System.in.read();
    }

    public record Task(long tid, long consumeId, long taskId, long inTime, long outTime) {

    }

    private final int count = 10_000_000;
    private final int totalCount = 40_000_000;
    private final AtomicInteger total = new AtomicInteger(0);

    public void run() {
        IntStream.of(0, 1, 2, 3).forEach(id -> new Thread(this::put).start());
        IntStream.of(0, 1).forEach(id -> new Thread(this::read).start());
    }

    private void put() {
        var threadId = Thread.currentThread().getId();
        var watch = StopWatch.createStarted();
        for (var i = 0; i < count; i++) {
            queue.add(new Task(threadId, 0, i, System.nanoTime(), 0));
        }
        watch.stop();
        System.out.println("put: thread=%s,time=%s".formatted(threadId, watch.formatTime()));
    }

    private void read() {
        var threadId = Thread.currentThread().getId();
        var watch = StopWatch.createStarted();
        while (total.get() < totalCount) {
            var value = queue.poll();
            if (null != value) {
                if (total.addAndGet(1) == totalCount) {
                    break;
                }
            }
        }
        watch.stop();
        System.out.println("read: thread=%s,time=%s".formatted(threadId, watch.getNanoTime()));
    }

    private final ConcurrentLinkedQueue<Task> queue = new ConcurrentLinkedQueue<>();
}
