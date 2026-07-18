#include "backgroundmatchdiscovery.h"

#include "videopairspace.h"

#include <QMetaObject>
#include <QThread>
#include <QtConcurrent/QtConcurrent>

BackgroundMatchDiscovery::BackgroundMatchDiscovery(int chunkSize, int workerCount, QObject* parent)
    : QObject(parent), _chunkSize(qMax(1, chunkSize)), _requestedWorkerCount(workerCount)
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
    _safeEnd = 0;
    _nextChunkToCommit = 0;
    _matches.clear();
    _started = true;

    const int chunkCount = int((_maxPosition + _chunkSize - 1) / _chunkSize);
    _completedChunks = QBitArray(chunkCount, false);
    emit safeEndChanged(0);

    if (chunkCount == 0) {
        emit finished();
        return;
    }

    const quint64 generation = _generation;
    const QVector<Video*> videosSnapshot = videos;
    const VideoPairMatchConfig configSnapshot = config;
    const int64_t maxPosition = _maxPosition;
    _runState = std::make_shared<RunState>();
    const auto runState = _runState;

    const int workerCount = workerCountForRun(chunkCount);
    _workers.reserve(workerCount);
    for (int worker = 0; worker < workerCount; ++worker) {
        _workers.append(QtConcurrent::run(
            [this, generation, videosSnapshot, configSnapshot, maxPosition, chunkCount, runState]() {
                while (!runState->cancelled) {
                    const int chunk = runState->nextChunk.fetch_add(1);
                    if (chunk >= chunkCount)
                        return;

                    const int64_t firstPosition = int64_t(chunk) * _chunkSize + 1;
                    const int64_t lastPosition = qMin(firstPosition + _chunkSize - 1, maxPosition);
                    QVector<MatchedVideoPair> matches;

                    auto pair = VideoPairSpace::pairAtPosition(videosSnapshot.size(), firstPosition);
                    while (!runState->cancelled) {
                        const auto result = VideoPairMatcher::match(*videosSnapshot[pair.left],
                                                                  *videosSnapshot[pair.right], configSnapshot);
                        if (result.matches) {
                            matches.append({pair.left, pair.right, pair.position, result.phashSimilarity,
                                            result.ssimSimilarity});
                        }

                        if (pair.position == lastPosition)
                            break;
                        VideoPairSpace::advancePair(videosSnapshot.size(), pair);
                    }

                    if (runState->cancelled)
                        return;

                    QMetaObject::invokeMethod(
                        this,
                        [this, generation, chunk, matches]() { acceptCompletedChunk(generation, chunk, matches); },
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

std::optional<MatchedVideoPair> BackgroundMatchDiscovery::nextCandidateAfter(int64_t position) const
{
    const auto candidate = _matches.upperBound(position);
    if (candidate == _matches.end() || candidate.key() > _safeEnd)
        return std::nullopt;
    return candidate.value();
}

std::optional<MatchedVideoPair> BackgroundMatchDiscovery::previousCandidateBefore(int64_t position) const
{
    if (_matches.isEmpty() || _safeEnd < 1)
        return std::nullopt;

    auto candidate = _matches.lowerBound(qMin(position, _safeEnd + 1));
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

    const int oldSafeEnd = _safeEnd;
    while (_nextChunkToCommit < _completedChunks.size() && _completedChunks.testBit(_nextChunkToCommit))
        ++_nextChunkToCommit;

    _safeEnd = qMin(int64_t(_nextChunkToCommit) * _chunkSize, _maxPosition);
    if (_safeEnd != oldSafeEnd)
        emit safeEndChanged(_safeEnd);
    if (_safeEnd == _maxPosition)
        emit finished();
}
