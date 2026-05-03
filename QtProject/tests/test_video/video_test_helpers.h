#ifndef VIDEO_TEST_HELPERS_H
#define VIDEO_TEST_HELPERS_H

#include <QDir>
#include <QFileInfo>
#include <QList>

#include "../helpers.h"

class TestHelpers
{
  public:
    static bool doThumbnailsLookSameWindow(const QByteArray ref_thumb, const QByteArray new_thumb, const QString title);
};

#endif // VIDEO_TEST_HELPERS_H
