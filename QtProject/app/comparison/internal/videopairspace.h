#ifndef VIDEOPAIRSPACE_H
#define VIDEOPAIRSPACE_H

#include <QtGlobal>

#include <cstdint>

struct VideoPairPosition {
    int left = 0;
    int right = 0;
    int64_t position = 0;
};

namespace VideoPairSpace
{
inline int64_t comparisonCount(int videoCount)
{
    const int64_t count = videoCount;
    return count * (count - 1) / 2;
}

inline int64_t positionForPair(int videoCount, int left, int right)
{
    // Positions are one-based, and the right video is always after the
    // left video in the list. For example, with 5 videos from number 0 to 4:
    //
    // (0, 1), (0, 2), (0, 3), (0, 4)   positions 1, 2, 3, 4
    // (1, 2), (1, 3), (1, 4)           positions 5, 6, 7
    // (2, 3), (2, 4)                   positions 8, 9
    // (3, 4)                           position  10
    //
    // (1, 3) is the second pair in the row starting at position 5, giving position 6.
    //
    // With n = videoCount, videos from 0 to n - 1, pair space is laid out in rows by the left video:
    //
    //   (0, 1), (0, 2), ... (0, n-1), n-1 comparisons,        positions 1,             2,  ...              n - 1
    //   (1, 2), (1, 3), ... (1, n-1), n-2 comparisons,        positions n-1  +1, n-1  +2, ... n-1  + n-2 = 2n - 3
    //   (2, 3), (2, 4), ... (2, n-1), n-3 comparisons,        positions 2n-3 +1, 2n-3 +2, ... 2n-3 + n-3 = 3n - 6
    //   ...
    //   (n-2, n-1)                    n-(n-1) = 1 comparison, position  (n-2)*n - (n-2)*(n-1)/2 + 1      = n*(n-1)/2
    //
    //  Before row `left` there are
    //
    //   (n - 1) + (n - 2) + ... + (n - left)
    //   = left * n - left * (left + 1) / 2
    //
    // pairs. Within the row, (left, left + 1) is offset 1,
    // so `right - left` supplies the one-based offset.
    Q_ASSERT(0 <= left && left < right && right < videoCount);
    const int64_t count = videoCount;
    const int64_t leftIndex = left;
    return leftIndex * count - (leftIndex * (leftIndex + 1) / 2) + (right - left);
}

inline VideoPairPosition pairAtPosition(int videoCount, int64_t position)
{
    // This is the inverse of positionForPair(): it converts a one-based
    // position in the pair space back into the two video indexes.
    Q_ASSERT(videoCount >= 2);
    Q_ASSERT(1 <= position && position <= comparisonCount(videoCount));

    // Every possible left index starts one row. Find the last row whose
    // first pair is at or before `position`; that is the row containing the
    // requested pair. Row starts are ordered, so a binary search avoids
    // walking through all preceding rows.
    int low = 0;
    int high = videoCount - 2;
    int left = 0;
    while (low <= high) {
        const int middle = (low + high) / 2;
        const int64_t rowStart = positionForPair(videoCount, middle, middle + 1);
        if (rowStart <= position) {
            left = middle;
            low = middle + 1;
        }
        else {
            high = middle - 1;
        }
    }

    // The first pair in this row is (left, left + 1). Moving forward from
    // rowStart advances only the right index.
    const int64_t rowStart = positionForPair(videoCount, left, left + 1);
    return {left, left + 1 + int(position - rowStart), position};
}

// pairAtPosition() is useful for random access, but its binary search costs
// O(log videoCount). Discovery and foreground matching scan contiguous ranges,
// where calling it for every position would make pair traversal unnecessarily
// O(pairCount * log videoCount). They call it once at the range boundary and
// then use these O(1) operations for each subsequent pair.
inline void advancePair(int videoCount, VideoPairPosition& pair)
{
    // Move to the next pair in row order without converting the new position
    // through pairAtPosition(). Crossing a row resets `right` after advancing
    // `left`.
    Q_ASSERT(0 <= pair.left && pair.left < pair.right && pair.right < videoCount);
    Q_ASSERT(pair.position < comparisonCount(videoCount));

    ++pair.position;
    ++pair.right;
    if (pair.right == videoCount) {
        ++pair.left;
        pair.right = pair.left + 1;
    }
}

inline void retreatPair(int videoCount, VideoPairPosition& pair)
{
    // Moving back from the first pair of a row lands on the final pair of the
    // preceding row.
    Q_ASSERT(0 <= pair.left && pair.left < pair.right && pair.right < videoCount);
    Q_ASSERT(pair.position > 1);

    --pair.position;
    if (pair.right > pair.left + 1) {
        --pair.right;
    }
    else {
        --pair.left;
        pair.right = videoCount - 1;
    }
}
} // namespace VideoPairSpace

#endif // VIDEOPAIRSPACE_H
