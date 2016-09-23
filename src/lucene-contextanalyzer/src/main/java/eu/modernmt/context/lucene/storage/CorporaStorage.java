package eu.modernmt.context.lucene.storage;

import eu.modernmt.context.ContextAnalyzerException;
import eu.modernmt.context.lucene.ContextAnalyzerIndex;
import eu.modernmt.updating.Update;
import eu.modernmt.updating.UpdatesListener;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.Map;
import java.util.concurrent.*;

import static java.nio.file.StandardCopyOption.ATOMIC_MOVE;
import static java.nio.file.StandardCopyOption.REPLACE_EXISTING;

/**
 * Created by davide on 22/09/16.
 */
public class CorporaStorage implements UpdatesListener {

    private static final Update POISON_PILL = new Update(0, 0L, 0, false, false);

    private final Logger logger = LogManager.getLogger(CorporaStorage.class);

    private final File indexPath;
    private final File swapIndexPath;
    private final Options options;
    private final BackgroundTask backgroundTask;
    private final ExecutorService analysisExecutor;

    private final ContextAnalyzerIndex contextAnalyzer;
    private final CorporaIndex index;
    private HashSet<CorpusBucket> pendingUpdatesBuckets = new HashSet<>();

    public CorporaStorage(File path, Options options, ContextAnalyzerIndex contextAnalyzer) throws IOException {
        this.analysisExecutor = Executors.newFixedThreadPool(options.analysisThreads);

        this.options = options;
        this.contextAnalyzer = contextAnalyzer;

        FileUtils.forceMkdir(path);

        this.indexPath = new File(path, "index");
        this.swapIndexPath = new File(path, "~index");

        if (indexPath.exists())
            this.index = CorporaIndex.load(options.analysisOptions, indexPath, path);
        else
            this.index = new CorporaIndex(options.analysisOptions, path);

        try {
            this.analyzeIfNeeded(this.index.getBuckets());
        } catch (ContextAnalyzerException e) {
            throw new IOException(e);
        }

        this.backgroundTask = new BackgroundTask(options.queueSize);
        this.backgroundTask.start();
    }

    @Override
    public void updateReceived(Update update) throws InterruptedException, IOException {
        backgroundTask.add(update);
    }

    @Override
    public Map<Integer, Long> getLatestSequentialNumbers() {
        return index.getStreams();
    }

    private void flushToDisk(boolean skipAnalysis) throws IOException {
        if (pendingUpdatesBuckets.isEmpty())
            return;

        logger.info("Flushing index to disk. Pending updates: " + pendingUpdatesBuckets.size());

        for (CorpusBucket bucket : pendingUpdatesBuckets) {
            bucket.flush();
        }

        if (!skipAnalysis) {
            try {
                analyzeIfNeeded(pendingUpdatesBuckets);
            } catch (ContextAnalyzerException e) {
                throw new IOException(e);
            }
        }

        pendingUpdatesBuckets.clear();

        this.index.store(this.swapIndexPath);
        Files.move(this.swapIndexPath.toPath(), this.indexPath.toPath(), REPLACE_EXISTING, ATOMIC_MOVE);
        FileUtils.deleteQuietly(this.swapIndexPath);
    }

    private void analyzeIfNeeded(Collection<CorpusBucket> buckets) throws ContextAnalyzerException {
        ArrayList<Future<Void>> pendingAnalysis = new ArrayList<>(buckets.size());

        buckets.stream()
                .filter(CorpusBucket::shouldAnalyze)
                .forEach(bucket -> {
                    AnalysisTask task = new AnalysisTask(contextAnalyzer, bucket);
                    pendingAnalysis.add(analysisExecutor.submit(task));
                });

        if (pendingAnalysis.isEmpty())
            return;

        for (Future<Void> analysis : pendingAnalysis) {
            try {
                analysis.get();
            } catch (InterruptedException e) {
                throw new ContextAnalyzerException("Analysis has been interrupted", e);
            } catch (ExecutionException e) {
                Throwable cause = e.getCause();

                if (cause instanceof RuntimeException)
                    throw (RuntimeException) cause;
                else if (cause instanceof ContextAnalyzerException)
                    throw (ContextAnalyzerException) cause;
                else
                    throw new Error("Unexpected exception", cause);
            }
        }

        this.contextAnalyzer.flush();
        this.contextAnalyzer.invalidateCache();
    }

    public void shutdown() {
        backgroundTask.shutdown();
    }

    public void awaitTermination(TimeUnit unit, long timeout) throws InterruptedException {
        unit.timedJoin(backgroundTask, timeout);
    }

    private class BackgroundTask extends Thread {

        private final BlockingQueue<Update> queue;
        private boolean shuttingDown = false;
        private IOException error = null;

        private long lastWriteDate = 0;

        public BackgroundTask(int queueSize) {
            this.queue = new ArrayBlockingQueue<>(queueSize);
        }

        public void add(Update update) throws InterruptedException, IOException {
            if (error != null)
                throw error;

            if (!shuttingDown)
                queue.put(update);
        }

        public void shutdown() {
            if (!shuttingDown) {
                shuttingDown = true;
                queue.clear();

                while (!queue.add(POISON_PILL)) ;
            }
        }

        private Update getUpdate(long timeout) {
            if (timeout < 1)
                return null;

            try {
                return queue.poll(timeout, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                return POISON_PILL;
            }
        }

        private void doRun() throws IOException {
            while (true) {
                long availableTime = options.writeBehindDelay - (System.currentTimeMillis() - lastWriteDate);

                Update update = getUpdate(availableTime);

                if (update == null) {
                    // timeout
                    flushToDisk(false);
                    lastWriteDate = System.currentTimeMillis();
                } else if (update == POISON_PILL) {
                    break;
                } else if (index.registerUpdate(update.streamId, update.sentenceId)) {
                    CorpusBucket bucket = index.getBucket(update.domain);

                    if (!bucket.isOpen())
                        bucket.open();

                    String line = update.sourceSentence.toString();
                    bucket.append(line);

                    pendingUpdatesBuckets.add(bucket);
                }
            }
        }

        @Override
        public void run() {
            try {
                doRun();
                flushToDisk(true);
            } catch (IOException e) {
                error = e;
            } finally {
                IOUtils.closeQuietly(index);
            }
        }

    }
}