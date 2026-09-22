#ifndef BACKGROUNDMATCHDISCOVERY_H
#define BACKGROUNDMATCHDISCOVERY_H

#include "videopairmatcher.h"

#include <QBitArray>
#include <QFuture>
#include <QMap>
#include <QObject>
#include <QVector>

#include <atomic>
#include <functional>
#include <memory>

class BackgroundMatchDiscovery : public QObject
{
    Q_OBJECT
    friend class test_comparison;

  public:
    // Discovery owns scan state; the comparison browser owns navigation.
    // A positive chunkSize overrides the fixed default, primarily for focused tests.
    explicit BackgroundMatchDiscovery(int chunkSize = 0, int workerCount = 0, QObject* parent = nullptr);
    ~BackgroundMatchDiscovery() override;

    void start(const QVector<Video*>& videos, const VideoPairMatchConfig& config);
    void stop();

    bool hasStarted() const { return _started; }
    int64_t preScannedEnd() const { return _lastContiguousScannedPairPosition; }
    int discoveredMatchCount() const { return _matches.size(); }
    // Visits results only from the completed contiguous prefix. This keeps
    // consumers from showing later chunks before earlier work is known, while
    // avoiding a copy of the discovered-match graph for set rebuilding.
    void forEachSafeMatch(const std::function<void(const MatchedVideoPair&)>& visitor) const;
    bool isComplete() const { return _started && _lastContiguousScannedPairPosition == _maxPosition; }

  signals:
    void preScannedEndChanged(int64_t preScannedEnd);

  private:
    struct RunState {
        std::atomic_int nextChunk = 0;
        std::atomic_bool cancelled = false;
    };

    const int _requestedChunkSize;
    const int _requestedWorkerCount;
    int _chunkSize = 0;
    int64_t _maxPosition = 0;
    // Highest one-based pair-space position for which every position from 1
    // through this value has been scanned. Out-of-order completed chunks beyond
    // this point do not advance it until all preceding chunks are complete.
    int64_t _lastContiguousScannedPairPosition = 0;
    // Zero-based index of the last chunk included in the contiguous completed
    // prefix. -1 means that no chunk from the beginning has completed yet.
    int _lastContiguousScannedChunk = -1;
    bool _started = false;
    quint64 _generation = 0;
    // One bit per zero-based chunk: set when that chunk's worker result has
    // reached the owner thread. Chunks may complete out of order; the leading
    // contiguous set of bits is what advances _lastContiguousScannedPairPosition.
    QBitArray _completedChunks;
    // Pair-space order keeps set construction deterministic even when worker
    // chunks complete out of order. Iteration hides results beyond the safe prefix.
    QMap<int64_t, MatchedVideoPair> _matches;
    QVector<QFuture<void>> _workers;
    std::shared_ptr<RunState> _runState;

    int workerCountForRun(int chunkCount) const;
    void acceptCompletedChunk(quint64 generation, int chunk, const QVector<MatchedVideoPair>& matches);
};

#endif // BACKGROUNDMATCHDISCOVERY_H
