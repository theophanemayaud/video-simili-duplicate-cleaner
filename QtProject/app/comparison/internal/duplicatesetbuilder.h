#ifndef DUPLICATESETBUILDER_H
#define DUPLICATESETBUILDER_H

#include "videopairmatcher.h"

#include <QVector>

// A duplicate set is a connected component in the graph of accepted matches.
// Member indexes intentionally retain the current video-vector order: that is
// the user's chosen sort order and therefore the stable manual-review order.
// Each set owns its direct evidence so member selection and revalidation scale
// with that component rather than the entire discovery result.
struct DuplicateSet {
    QVector<int> members;
    QVector<MatchedVideoPair> edges;
};

namespace DuplicateSetBuilder
{
// Consumes the accepted-match buffer. Set rebuilding produces that buffer just
// before this call, so its edges can move directly into their components.
QVector<DuplicateSet> build(int videoCount, QVector<MatchedVideoPair>&& matches);
} // namespace DuplicateSetBuilder

#endif // DUPLICATESETBUILDER_H
