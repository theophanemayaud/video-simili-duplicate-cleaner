#ifndef BACKGROUNDMATCHDISCOVERY_H
#define BACKGROUNDMATCHDISCOVERY_H

#include "videopairmatcher.h"

#include <QBitArray>
#include <QFuture>
#include <QMap>
#include <QObject>
#include <QVector>

#include <atomic>
#include <memory>
#include <optional>

class BackgroundMatchDiscovery : public QObject
{
    Q_OBJECT

  public:
    // Discovery deliberately owns no navigation state. Foreground navigation
    // may duplicate work beyond safeEnd rather than coordinating with workers.
    explicit BackgroundMatchDiscovery(int chunkSize = 4096, int workerCount = 0, QObject* parent = nullptr);
    ~BackgroundMatchDiscovery() override;

    void start(const QVector<Video*>& videos, const VideoPairMatchConfig& config);
    void stop();

    bool hasStarted() const { return _started; }
    int64_t safeEnd() const { return _safeEnd; }
    int64_t maxPosition() const { return _maxPosition; }
    int discoveredMatchCount() const { return _matches.size(); }

    std::optional<MatchedVideoPair> nextCandidateAfter(int64_t position) const;
    std::optional<MatchedVideoPair> previousCandidateBefore(int64_t position) const;

  signals:
    void safeEndChanged(int64_t safeEnd);
    void finished();

  private:
    struct RunState {
        std::atomic_int nextChunk = 0;
        std::atomic_bool cancelled = false;
    };

    const int _chunkSize;
    const int _requestedWorkerCount;
    int64_t _maxPosition = 0;
    int64_t _safeEnd = 0;
    int _nextChunkToCommit = 0;
    bool _started = false;
    quint64 _generation = 0;
    QBitArray _completedChunks;
    QMap<int64_t, MatchedVideoPair> _matches;
    QVector<QFuture<void>> _workers;
    std::shared_ptr<RunState> _runState;

    int workerCountForRun(int chunkCount) const;
    void acceptCompletedChunk(quint64 generation, int chunk, const QVector<MatchedVideoPair>& matches);
};

#endif // BACKGROUNDMATCHDISCOVERY_H
