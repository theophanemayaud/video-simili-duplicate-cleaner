#ifndef VIDEO_TEST_HELPERS_H
#define VIDEO_TEST_HELPERS_H

#include <QFileInfo>
#include <QDir>
#include <QList>
#include "../helpers.h"

class TestHelpers {
public:
    static bool doThumbnailsLookSameWindow(const QByteArray ref_thumb, const QByteArray new_thumb, const QString title);
    // returns an empty list if either csv didn't exist, or something else happened.
    static QList<VideoParam> importCSVtoVideoParamQList(const QFileInfo csvInfo, const QDir videoDir, const QDir thumbDir); // CSV : sep  seperated values

    // will not overwrite it already exists
    static bool saveVideoParamQListToCSV(const QList<VideoParam> videoParamQList, const QFileInfo csvInfo, const QDir videoBaseDir); // false->failed to save
};

#endif // VIDEO_TEST_HELPERS_H

