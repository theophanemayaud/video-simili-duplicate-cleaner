#include "videoprocessingpool.h"
#include <QThread>
#include <QMutexLocker>
#include <QElapsedTimer>
#include <QTimer>
#include <QDebug>
#include <QObject>

const int STOP_CHECK_INTERVAL = 5*1000;
VideoProcessingPool::VideoProcessingPool()
{
}

VideoProcessingPool::~VideoProcessingPool()
{
    ClearWaiting();
    // todo wait until all active tasks are done
    while(CountTasksLeftToProcess() > 0) {
        QApplication::processEvents();
    }
    RequestShutdown();
    while(CountWorkersStillRunning() > 0) {
        QApplication::processEvents();
        waitingQueueNotEmpty.wakeAll();
    }
    for (auto worker : workers) {
        worker->wait();
        worker->deleteLater();
    }
    workers.clear();
}

void VideoProcessingPool::AddTask(Video* video)
{
    QMutexLocker locker(&waitingTasksQueueMutex);
    waitingTasksQueue.enqueue(video);
    locker.unlock();
    if (workers.size() < QThread::idealThreadCount()) {
    // if (workers.size() < 1) { // for local debugging if parallelism is causing issues
        spawnWorker();
    } else {
        waitingQueueNotEmpty.wakeOne(); // no need when spawning worker, as it will wake up anyway
    }
}

uint VideoProcessingPool::CountTasksLeftToProcess() {
    QMutexLocker locker(&waitingTasksQueueMutex);
    uint count = waitingTasksQueue.size();
    count += activeTasksProgress.size();
    return count;
}

uint VideoProcessingPool::CountWorkersStillRunning() {
    waitingQueueNotEmpty.wakeAll();
    return workers.size();
}

void VideoProcessingPool::ClearWaiting()
{
    QMutexLocker locker(&waitingTasksQueueMutex);
    waitingTasksQueue.clear();
}

void VideoProcessingPool::RequestShutdown() {
    for (auto worker : workers) {
        worker->requestStop();
    }
    waitingQueueNotEmpty.wakeAll();
}

void VideoProcessingPool::spawnWorker()
{
    qDebug() << "Spawning worker with already" << workers.size() << "workers running";
    auto worker = new VideoWorkerThread(&waitingTasksQueueMutex, &waitingQueueNotEmpty, &waitingTasksQueue);
    
    connect(worker, &VideoWorkerThread::taskStarted, this, [this, worker](Video* video) {
        activeTasksProgress[video->_filePathName] = video->getProgress();

        auto stuckTimer = new QTimer();
        auto elapsedTimer = new QElapsedTimer();
        const QString videoPathName = video->_filePathName; // get a copy so that
        connect(stuckTimer, &QTimer::timeout, this, [this, stuckTimer, elapsedTimer, videoPathName, video, worker]() {
            if (activeTasksProgress.find(videoPathName) == activeTasksProgress.end()){
                // task finished already, removing timer
                stuckTimer->stop();
                stuckTimer->deleteLater();
                delete elapsedTimer;
                return;
            }
            QMutexLocker locker(&waitingTasksQueueMutex); // take lock on waiting tasks so thread being killed suddently gets unstuck and doesn't pick up new task
            auto newProgress = video->getProgress();
            if (activeTasksProgress[video->_filePathName] == newProgress) { // progress did not change after 1s, so it is stuck
                auto stop = worker->isStopped();
                worker->terminate();
                // worker->wait(); // can't do this, for some reason never returns, seems like terminate doesn't work...
                // worker->deleteLater(); // also therefore can't delete a running thread...
                workers.removeAll(worker); // ideally would be done when signal finished is emitted but as above, terminate doesn't seem to work properly
                activeTasksProgress.remove(video->_filePathName);

                locker.unlock();
                auto canceledResult = Video::ProcessingResult{
                    false, QString("Terminated process as video processing seemed to be stuck, after %1s").arg(elapsedTimer->elapsed()/1000), video
                };
                emit TaskResult(canceledResult);
                if (!stop)
                    spawnWorker();
                return;
            }
            activeTasksProgress[video->_filePathName] = newProgress;
        });
        stuckTimer->start(STOP_CHECK_INTERVAL);
        elapsedTimer->start();
    });

    connect(worker, &VideoWorkerThread::taskFinished, this, [this](Video::ProcessingResult result) {
        activeTasksProgress.remove(result.video->_filePathName);

        emit TaskResult(result);
    });

    connect(worker, &VideoWorkerThread::finished, this, [this, worker]() {
        workers.removeAll(worker); // removes thread when normal exit (already removed if terminated)
        worker->deleteLater();
    });


    workers.append(worker);
    worker->start();
}

// VideoWorkerThread implementation
VideoWorkerThread::VideoWorkerThread(QMutex* queueMutex, QWaitCondition* queueNotEmpty, QQueue<Video*>* taskQueue)
    : queueNotEmpty(queueNotEmpty)
    , queueMutex(queueMutex)
    , taskQueue(taskQueue)
{
}

void VideoWorkerThread::requestStop()
{
    QMutexLocker locker(&stopMutex);
    shouldStop = true;
}

bool VideoWorkerThread::isStopped()
{
    QMutexLocker locker(&stopMutex);
    return shouldStop;
}

void VideoWorkerThread::run()
{
    while (true) {
        Video* video = nullptr;

        QMutexLocker queueLocker(queueMutex);
        if (!taskQueue->isEmpty())
            video = taskQueue->dequeue();

        if(!video)
            queueNotEmpty->wait(queueMutex, 100); // just in case waking isn't done properly we wake up every 100ms to check for new tasks

        QMutexLocker stopLocker(&stopMutex);
        if (shouldStop)
            return;
        stopLocker.unlock();

        queueLocker.unlock();

        if (video) {
            emit taskStarted(video);
            Video::ProcessingResult result = video->process();
            emit taskFinished(result);
        }
    }
    // upon exit, qt emits the finished signal but not from the thread itself as it is not running so we don't use it
}
