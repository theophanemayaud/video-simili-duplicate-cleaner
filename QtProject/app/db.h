#ifndef DB_H
#define DB_H

#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

#include "prefs.h"
#include "video.h"

class Video;

class Db
{

  public:
    struct FailedVideoCacheLookup
    {
        enum State {
            LookupError,
            NotFound,
            Unchanged,
            Changed
        } state = LookupError;
        QString error;
    };

    struct ApplePhotosNameCacheEntry
    {
        enum State {
            Unknown,
            Found,
            Failed
        } state = Unknown;
        QString name;
    };

    explicit Db(const QString cacheFilePathName);
    ~Db();

  private:
    QSqlDatabase _db;
    QString _uniqueConnexionName;
    static void createTables(QSqlDatabase db, const QString appVersion);
    static QString getUserSelectedCacheNamePath(const Prefs& prefs);
    static bool initDbAndCache(const Prefs& prefs);

  public:
    static bool initDbAndCacheLocation(Prefs& prefs);

    static bool initCustomDbAndCacheLocation(Prefs& prefs);

    static bool emptyAllDb(const Prefs prefs);

    //    //return md5 hash of parameter's file, used internally as "unique id" for each file
    //    static QString pathnameHashId(const QString &filename=QStringLiteral(""));

    //constructor creates a database file if there is none already
    void createTables() const;

    //return true and updates member variables if the video metadata was cached
    bool readMetadata(Video& video) const;

    //save video properties in cache
    void writeMetadata(const Video& video) const;

    // Apple Photos lookups are deliberately cached independently from video metadata:
    // no row means never attempted, while a failed row prevents another slow query.
    ApplePhotosNameCacheEntry readApplePhotosName(const QString& filePathname) const;
    void writeApplePhotosName(const QString& filePathname, const QString& name) const;
    void writeApplePhotosNameFailure(const QString& filePathname) const;

    //returns screen capture if it was cached, else return null ptr
    QByteArray readCapture(const QString& filePathname, const int& percent) const;

    //save image in cache
    void writeCapture(const QString& filePathname, const int& percent, const QByteArray& image) const;

    // Failed videos are keyed by path and sampling mode. A changed file invalidates all path-keyed cache data.
    FailedVideoCacheLookup lookupFailedVideo(const QString& filePathname, qint64 size, qint64 modifiedMs,
                                             int thumbnailMode) const;
    bool writeFailedVideo(const QString& filePathname, qint64 size, qint64 modifiedMs, int thumbnailMode,
                          const QString& error) const;
    // A changed failed file must never reuse a partial old cache; false leaves every row untouched for a later retry.
    bool invalidateChangedFailedVideo(const QString& filePathname);
    // Returns the number of distinct videos prepared for retry, or -1 when the database operation fails.
    int clearFailedVideos();

    //returns false was not cached or could not be removed
    bool removeVideo(const QString& filePathname) const;

    // returns a list of unique cached video pathNames within specified folders
    QSet<QString> getCachedVideoPathnamesInFolders(QStringList directoriesPaths) const;

    // when wanting to save a pair or videos to be ignored on next runs, this will save a record in cache
    void writePairToIgnore(const QString filePathName1, const QString filePathName2) const;

    // when wanting to check if a pair was previous saved to be ignored in cache
    bool isPairToIgnore(const QString filePathName1, const QString filePathName2) const;
};

#endif // DB_H
