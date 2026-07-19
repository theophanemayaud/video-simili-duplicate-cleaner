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
    // A positive chunkSize overrides the size tiers, primarily for focused tests.
    explicit BackgroundMatchDiscovery(int chunkSize = 0, int workerCount = 0, QObject* parent = nullptr);
    ~BackgroundMatchDiscovery() override;

    void start(const QVector<Video*>& videos, const VideoPairMatchConfig& config);
    void stop();

    bool hasStarted() const { return _started; }
    int64_t preScannedEnd() const { return _lastContiguousScannedPairPosition; }
    int discoveredMatchCount() const { return _matches.size(); }

    std::optional<MatchedVideoPair> nextCandidateAfter(int64_t position) const;
    std::optional<MatchedVideoPair> previousCandidateBefore(int64_t position) const;

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
    // Keyed by one-based pair-space position so navigation can efficiently find
    // the next or previous sparse match. Results from chunks completed out of
    // order are inserted immediately, so this map may contain matches beyond
    // _lastContiguousScannedPairPosition; query methods hide those until the contiguous prefix catches up.
    QMap<int64_t, MatchedVideoPair> _matches;
    QVector<QFuture<void>> _workers;
    std::shared_ptr<RunState> _runState;

    int chunkSizeForRun(int videoCount) const;
    int workerCountForRun(int chunkCount) const;
    void acceptCompletedChunk(quint64 generation, int chunk, const QVector<MatchedVideoPair>& matches);
};

#endif // BACKGROUNDMATCHDISCOVERY_H
