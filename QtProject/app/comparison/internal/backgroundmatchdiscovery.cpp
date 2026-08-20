#include "backgroundmatchdiscovery.h"

#include "videopairspace.h"

#include <QMetaObject>
#include <QThread>
#include <QtConcurrent/QtConcurrent>

namespace
{
// Historical large-library pHash timings averaged about 0.6 ms per 1,000 possible
// pairs, but that scan stopped each row after its first match. Discovery scans
// every pair, so use a conservative estimate of 1 ms per 1,000 pairs: 100,000
// pairs should keep each worker chunk around 100 ms in the common pHash-dominated
// case. SSIM chunks can take longer when many pairs pass their pHash prefilter.
constexpr int DEFAULT_CHUNK_SIZE = 100'000;
} // namespace

BackgroundMatchDiscovery::BackgroundMatchDiscovery(int chunkSize, int workerCount, QObject* parent)
    : QObject(parent), _requestedChunkSize(qMax(0, chunkSize)), _requestedWorkerCount(workerCount)
{
}

BackgroundMatchDiscovery::~BackgroundMatchDiscovery()
{
    stop();
}

void BackgroundMatchDiscovery::start(const QVector<Video*>& videos, const VideoPairMatchConfig& config)
{
    stop();

    _maxPosition = VideoPairSpace::comparisonCount(videos.size());
    _chunkSize = _requestedChunkSize > 0 ? _requestedChunkSize : DEFAULT_CHUNK_SIZE;
    _lastContiguousScannedPairPosition = 0;
    _lastContiguousScannedChunk = -1;
    _matches.clear();
    _started = true;

    const int chunkCount = int((_maxPosition + _chunkSize - 1) / _chunkSize);
    _completedChunks = QBitArray(chunkCount, false);
    emit preScannedEndChanged(0);

    if (chunkCount == 0)
        return;

    const quint64 generation = _generation;
    const QVector<Video*> videosSnapshot = videos;
    const VideoPairMatchConfig configSnapshot = config;
    const int64_t maxPosition = _maxPosition;
    _runState = std::make_shared<RunState>();
    const auto runState = _runState;

    const int workerCount = workerCountForRun(chunkCount);
    _workers.reserve(workerCount);
    for (int worker = 0; worker < workerCount; ++worker) {
        _workers.append(QtConcurrent::run([this, generation, videosSnapshot, configSnapshot, maxPosition, chunkCount,
                                           runState]() {
            while (!runState->cancelled) {
                // Chunk indexes are zero-based. fetch_add() returns the
                // current value to this worker, then increments it for
                // the next worker claiming a chunk.
                const int chunk = runState->nextChunk.fetch_add(1);
                if (chunk >= chunkCount)
                    return;

                // Pair-space positions are one-based, hence the +1.
                const int64_t firstPosition = int64_t(chunk) * _chunkSize + 1;
                const int64_t lastPosition = qMin(firstPosition + _chunkSize - 1, maxPosition);
                QVector<MatchedVideoPair> matches;

                // To keep chunking simple, we get the first pair position via pairAtPosition
                // though it is O(log(videoCount)), so make sure chunks are big enough
                auto pair = VideoPairSpace::pairAtPosition(videosSnapshot.size(), firstPosition);
                while (!runState->cancelled) {
                    const auto result = VideoPairMatcher::match(*videosSnapshot[pair.left], *videosSnapshot[pair.right],
                                                                configSnapshot);
                    if (result.matches) {
                        matches.append(
                            {pair.left, pair.right, pair.position, result.phashSimilarity, result.ssimSimilarity});
                    }

                    if (pair.position >= lastPosition)
                        break;
                    VideoPairSpace::advancePair(videosSnapshot.size(), pair);
                }

                if (runState->cancelled)
                    return;

                QMetaObject::invokeMethod(
                    this, [this, generation, chunk, matches]() { acceptCompletedChunk(generation, chunk, matches); },
                    Qt::QueuedConnection);
            }
        }));
    }
}

void BackgroundMatchDiscovery::stop()
{
    ++_generation;
    if (_runState)
        _runState->cancelled = true;

    for (auto& worker : _workers)
        worker.waitForFinished();

    _workers.clear();
    _runState.reset();
    _started = false;
}

QVector<MatchedVideoPair> BackgroundMatchDiscovery::safeMatches() const
{
    QVector<MatchedVideoPair> matches;
    for (auto it = _matches.cbegin(); it != _matches.cend() && it.key() <= _lastContiguousScannedPairPosition; ++it)
        matches.append(it.value());
    return matches;
}

std::optional<MatchedVideoPair> BackgroundMatchDiscovery::nextCandidateAfter(int64_t position) const
{
    const auto candidate = _matches.upperBound(position);
    if (candidate == _matches.end() || candidate.key() > _lastContiguousScannedPairPosition)
        return std::nullopt;
    return candidate.value();
}

std::optional<MatchedVideoPair> BackgroundMatchDiscovery::previousCandidateBefore(int64_t position) const
{
    if (_matches.isEmpty() || _lastContiguousScannedPairPosition < 1)
        return std::nullopt;

    auto candidate = _matches.lowerBound(qMin(position, _lastContiguousScannedPairPosition + 1));
    if (candidate == _matches.begin())
        return std::nullopt;
    --candidate;
    return candidate.value();
}

int BackgroundMatchDiscovery::workerCountForRun(int chunkCount) const
{
    if (_requestedWorkerCount > 0)
        return qMin(_requestedWorkerCount, chunkCount);

    const int idealThreadCount = QThread::idealThreadCount();
    const int availableWorkers = idealThreadCount > 1 ? idealThreadCount - 1 : 1;
    return qMin(availableWorkers, chunkCount);
}

void BackgroundMatchDiscovery::acceptCompletedChunk(quint64 generation, int chunk,
                                                    const QVector<MatchedVideoPair>& matches)
{
    if (generation != _generation || chunk < 0 || chunk >= _completedChunks.size())
        return;

    for (const auto& match : matches)
        _matches.insert(match.position, match);
    _completedChunks.setBit(chunk);

    // The last contiguous scanned chunk is kept as state to avoid re scanning from start the entire bit array
    const int64_t oldLastContiguousScannedPairPosition = _lastContiguousScannedPairPosition;
    while (_lastContiguousScannedChunk + 1 < _completedChunks.size()
           && _completedChunks.testBit(_lastContiguousScannedChunk + 1))
        ++_lastContiguousScannedChunk;

    _lastContiguousScannedPairPosition = qMin(int64_t(_lastContiguousScannedChunk + 1) * _chunkSize, _maxPosition);
    if (_lastContiguousScannedPairPosition != oldLastContiguousScannedPairPosition)
        emit preScannedEndChanged(_lastContiguousScannedPairPosition);
}
