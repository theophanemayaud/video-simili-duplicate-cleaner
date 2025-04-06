#ifndef VIDEOPROCESSINGPOOL_H
#define VIDEOPROCESSINGPOOL_H

#include <QObject>
#include <QThread>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QTimer>
#include <QElapsedTimer>
#include <QMap>
#include "video.h"

class VideoWorkerThread;

class VideoProcessingPool : public QObject
{
    Q_OBJECT

signals:
    void TaskResult(Video::ProcessingResult result);

public:
    explicit VideoProcessingPool();
    ~VideoProcessingPool();

    // Add a video task to the queue
    void AddTask(Video* video);

    uint CountTasksLeftToProcess();
    uint CountWorkersStillRunning();

    // Request all processing to stop and clear those still waiting, need to wait for done after calling this to make sure all tasks are stopped
    void ClearWaiting();

    // Initiate shutdown, wait for done after calling this to make sure all tasks are stopped
    void RequestShutdown();

private:
    void spawnWorker();

    struct TaskInfo {
        Video* video;
        VideoWorkerThread* worker;
        uint lastProgress;
    };

    // Thread management
    QList<VideoWorkerThread*> workers;      // All alive threads

    // Tasks waiting to start, protected by mutex as used in worker threads to pick up new tasks
    QQueue<Video*> waitingTasksQueue;
    QMutex waitingTasksQueueMutex;
    QWaitCondition waitingQueueNotEmpty;    // Workers wait on this to wake up and check for new tasks

    // Tasks currently being processed, not protected by mutex as only access from main thread
    QMap<QString, uint> activeTasksProgress;    // Tasks currently being processed

    friend class VideoWorkerThread;
};

class VideoWorkerThread : public QThread
{
    Q_OBJECT

public:
    explicit VideoWorkerThread(QMutex* queueMutex, QWaitCondition* queueNotEmpty, QQueue<Video*>* taskQueue);
    void run() override;
    void requestStop();
    bool isStopped();

signals:
    void taskStarted(Video* video);
    // Emitted when a task is finished, wether it succeeded or failed
    void taskFinished(Video::ProcessingResult result);

private:
    QMutex stopMutex;
    bool shouldStop = false;
    QWaitCondition* queueNotEmpty;
    QMutex* queueMutex;
    QQueue<Video*>* taskQueue;
};

#endif // VIDEOPROCESSINGPOOL_H
