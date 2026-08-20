#include "duplicatesetbuilder.h"

#include <algorithm>
#include <numeric>

namespace
{
class UnionFind
{
  public:
    explicit UnionFind(int count) : _parents(count), _sizes(count, 1)
    {
        std::iota(_parents.begin(), _parents.end(), 0);
    }

    int find(int value)
    {
        while (_parents[value] != value) {
            _parents[value] = _parents[_parents[value]];
            value = _parents[value];
        }
        return value;
    }

    void unite(int left, int right)
    {
        left = find(left);
        right = find(right);
        if (left == right)
            return;
        if (_sizes[left] < _sizes[right])
            std::swap(left, right);
        _parents[right] = left;
        _sizes[left] += _sizes[right];
    }

  private:
    QVector<int> _parents;
    QVector<int> _sizes;
};
} // namespace

QVector<DuplicateSet> DuplicateSetBuilder::build(int videoCount, const QVector<MatchedVideoPair>& matches)
{
    if (videoCount < 2)
        return {};

    UnionFind sets(videoCount);
    for (const MatchedVideoPair& match : matches) {
        if (match.left < 0 || match.right < 0 || match.left >= videoCount || match.right >= videoCount
            || match.left == match.right)
            continue;
        sets.unite(match.left, match.right);
    }

    QVector<QVector<int>> membersByRoot(videoCount);
    for (int video = 0; video < videoCount; ++video)
        membersByRoot[sets.find(video)].append(video);

    QVector<DuplicateSet> result;
    for (const QVector<int>& members : membersByRoot)
        if (members.size() > 1)
            result.append({members});
    std::sort(result.begin(), result.end(), [](const DuplicateSet& left, const DuplicateSet& right) {
        return left.members.first() < right.members.first();
    });
    return result;
}
