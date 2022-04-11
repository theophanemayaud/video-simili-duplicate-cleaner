#ifndef DB_H
#define DB_H

#include <QSqlDatabase>
#include <QSqlQuery>
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
    static QString getUserSelectedCacheNamePath(Prefs *prefs);

public:
    static bool initDbAndCacheLocation(Prefs *prefs);

    static bool initCustomDbAndCacheLocation(Prefs *prefs);

    static void emptyAllDb(const Prefs prefs);

    //return md5 hash of parameter's file, used internally as "unique id" for each file
    static QString pathnameHashId(const QString &filename=QStringLiteral(""));

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
};

#endif // DB_H
