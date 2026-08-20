#ifndef DUPLICATESETBUILDER_H
#define DUPLICATESETBUILDER_H

#include "videopairmatcher.h"

#include <QVector>

// A duplicate set is a connected component in the graph of accepted matches.
// Member indexes intentionally retain the current video-vector order: that is
// the user's chosen sort order and therefore the stable manual-review order.
struct DuplicateSet {
    QVector<int> members;
};

namespace DuplicateSetBuilder
{
QVector<DuplicateSet> build(int videoCount, const QVector<MatchedVideoPair>& matches);
}

#endif // DUPLICATESETBUILDER_H
