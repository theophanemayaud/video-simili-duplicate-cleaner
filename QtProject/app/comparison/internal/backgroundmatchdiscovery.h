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
    // may duplicate work beyond preScannedEnd rather than coordinating with workers.
    explicit BackgroundMatchDiscovery(int chunkSize = 4096, int workerCount = 0, QObject* parent = nullptr);
    ~BackgroundMatchDiscovery() override;

    void start(const QVector<Video*>& videos, const VideoPairMatchConfig& config);
    void stop();

    bool hasStarted() const { return _started; }
    int64_t preScannedEnd() const { return _contiguousScannedEnd; }
    int64_t maxPosition() const { return _maxPosition; }
    int discoveredMatchCount() const { return _matches.size(); }

    std::optional<MatchedVideoPair> nextCandidateAfter(int64_t position) const;
    std::optional<MatchedVideoPair> previousCandidateBefore(int64_t position) const;

  signals:
    void preScannedEndChanged(int64_t preScannedEnd);
    void finished();

  private:
    struct RunState {
        std::atomic_int nextChunk = 0;
        std::atomic_bool cancelled = false;
    };

    const int _chunkSize;
    const int _requestedWorkerCount;
    int64_t _maxPosition = 0;
    // Highest one-based pair-space position for which every position from 1
    // through this value has been scanned. Out-of-order completed chunks beyond
    // this point do not advance it until all preceding chunks are complete.
    int64_t _contiguousScannedEnd = 0;
    int _nextChunkToCommit = 0;
    bool _started = false;
    quint64 _generation = 0;
    QBitArray _completedChunks;
    // Keyed by one-based pair-space position so navigation can efficiently find
    // the next or previous sparse match. Results from chunks completed out of
    // order are inserted immediately, so this map may contain matches beyond
    // _contiguousScannedEnd; query methods hide those until the contiguous prefix catches up.
    QMap<int64_t, MatchedVideoPair> _matches;
    QVector<QFuture<void>> _workers;
    std::shared_ptr<RunState> _runState;

    int workerCountForRun(int chunkCount) const;
    void acceptCompletedChunk(quint64 generation, int chunk, const QVector<MatchedVideoPair>& matches);
};

#endif // BACKGROUNDMATCHDISCOVERY_H
