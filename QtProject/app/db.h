#ifndef DB_H
#define DB_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QApplication>
#include <QCryptographicHash>
#include <QUuid>
#include <QStandardPaths>
#include <QFileDialog>
#include <QMessageBox>

#include "prefs.h"
#include "video.h"

class Video;

class Db
{

public:
    explicit Db(const QString cacheFilePathName);
    ~Db() { _db.close(); _db = QSqlDatabase(); _db.removeDatabase(_uniqueConnexionName); }

private:
    QSqlDatabase _db;
    QString _uniqueConnexionName;
    static void createTables(QSqlDatabase db, const QString appVersion);
    static QString getUserSelectedCacheNamePath(const Prefs &prefs);
    static bool initDbAndCache(const Prefs& prefs);

public:
    static bool initDbAndCacheLocation(Prefs &prefs);

    static bool initCustomDbAndCacheLocation(Prefs &prefs);

    static bool emptyAllDb(const Prefs prefs);

//    //return md5 hash of parameter's file, used internally as "unique id" for each file
//    static QString pathnameHashId(const QString &filename=QStringLiteral(""));

    //constructor creates a database file if there is none already
    void createTables() const;

    //return true and updates member variables if the video metadata was cached
    bool readMetadata(Video &video) const;

    //save video properties in cache
    void writeMetadata(const Video &video) const;

    //returns screen capture if it was cached, else return null ptr
    QByteArray readCapture(const QString &filePathname, const int &percent) const;

    //save image in cache
    void writeCapture(const QString &filePathname, const int &percent, const QByteArray &image) const;

    //returns false was not cached or could not be removed
    bool removeVideo(const QString &filePathname) const;

    // returns a list of unique cached video pathNames within specified folders
    QStringList getCachedVideoPathnamesInFolders(QStringList directoriesPaths) const;

    // when wanting to save a pair or videos to be ignored on next runs, this will save a record in cache
    void writePairToIgnore(const QString filePathName1, const QString filePathName2) const;

    // when wanting to check if a pair was previous saved to be ignored in cache
    bool isPairToIgnore(const QString filePathName1, const QString filePathName2) const;

};

#endif // DB_H
