#include <QApplication>
#include <QCryptographicHash>
#include <QSqlQuery>
#include <QUuid>
#include "db.h"
#include "video.h"

Db::Db(const QString &filename)
{
    // THEODEBUG : not using modified as we had problem with QT5 and datetime tostring in threads...
//    const QFileInfo file(filename);
//    _modified = file.lastModified(); // apparently if file doesn't exist, it still returns some random date...
    _connection = uniqueId(filename);       //connection name is unique (generated from full path+filename)
//    _id = uniqueId(file.fileName());        //primary key remains same even if file is moved to other folder
    _id = uniqueId(filename); // TODO : we can't use date here in QT5 because of bug, so won't stay cached if moved
    // maybe find a way to better identify files than simply their pathnames, because that could cause problems... !

    const QString dbfilename = QStringLiteral("%1/cache.db").arg(QApplication::applicationDirPath());
    _db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), _connection);
    _db.setDatabaseName(dbfilename);
    _db.open();

    createTables();
}

void Db::emptyAllDb(){
    const QString dbfilename = QStringLiteral("%1/cache.db").arg(QApplication::applicationDirPath());
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                QStringLiteral("emptyAllDb_%2"). // unique but meaningfull connexion name
                                                arg(QUuid::createUuid().toString()));
    db.setDatabaseName(dbfilename);
    db.open();

    QSqlQuery query(db);

//    query.exec(QStringLiteral("PRAGMA synchronous = OFF;"));
//    query.exec(QStringLiteral("PRAGMA journal_mode = WAL;"));

    query.exec(QStringLiteral("DROP TABLE IF EXISTS metadata"));

    query.exec(QStringLiteral("DROP TABLE IF EXISTS capture"));

    query.exec(QStringLiteral("DROP TABLE IF EXISTS version"));
    query.exec(QStringLiteral("VACUUM"));
    db.close();
}

QString Db::uniqueId(const QString &filename) const
{
    if(filename.isEmpty())
        return _id;

    // DEBUGTHEO : for windows, some calendar multithread bug in QT 5.15 needed to replace this unique ID
    //               Instead using Uuid, which seems to work quite well ! https://forum.qt.io/topic/120355/qdatetime-assert/6
//    const QString name_modified = QStringLiteral("%1_%2").arg(filename)
//                                  .arg(_modified.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")));
//    const QString name_modified = QStringLiteral("%1_%2").arg(filename)
//                                  .arg(QUuid::createUuid().toString());
    const QString name_modified = QStringLiteral("%1").arg(filename); // TODO : see if in QT6 can do otherwise...

    return QCryptographicHash::hash(name_modified.toLatin1(), QCryptographicHash::Md5).toHex();
}

void Db::createTables() const
{
    QSqlQuery query(_db);
    query.exec(QStringLiteral("PRAGMA synchronous = OFF;"));
    query.exec(QStringLiteral("PRAGMA journal_mode = WAL;"));

    query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS metadata (id TEXT PRIMARY KEY, "
                              "size INTEGER, duration INTEGER, bitrate INTEGER, framerate REAL, "
                              "codec TEXT, audio TEXT, width INTEGER, height INTEGER);"));

    query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS capture (id TEXT PRIMARY KEY, "
                              " at8 BLOB, at16 BLOB, at24 BLOB, at32 BLOB, at40 BLOB, at48 BLOB, "
                              "at56 BLOB, at64 BLOB, at72 BLOB, at80 BLOB, at88 BLOB, at96 BLOB);"));

    query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS version (version TEXT PRIMARY KEY);"));

    QFile file(":/version.txt");
    QString appVersion = "undefined";
    if (file.open(QIODevice::ReadOnly)){
        appVersion = file.readLine();
    }
    file.close();

    query.exec(QStringLiteral("INSERT OR REPLACE INTO version VALUES('%1');").arg(appVersion));
}

bool Db::readMetadata(Video &video) const
{
    QSqlQuery query(_db);
    query.exec(QStringLiteral("SELECT * FROM metadata WHERE id = '%1';").arg(_id));

    while(query.next())
    {
//        video.modified = _modified;
        video.size = query.value(1).toLongLong();
        video.duration = query.value(2).toLongLong();
        video.bitrate = query.value(3).toInt();
        video.framerate = query.value(4).toDouble();
        video.codec = query.value(5).toString();
        video.audio = query.value(6).toString();
        video.width = static_cast<short>(query.value(7).toInt());
        video.height = static_cast<short>(query.value(8).toInt());
        return true;
    } // TODO : should proooobably delete others if there are multiple results !!! Or produce error !
    return false;
}

void Db::writeMetadata(const Video &video) const
{
    QSqlQuery query(_db);
    query.exec(QStringLiteral("INSERT OR REPLACE INTO metadata VALUES('%1',%2,%3,%4,%5,'%6','%7',%8,%9);")
               .arg(_id).arg(video.size).arg(video.duration).arg(video.bitrate).arg(video.framerate)
               .arg(video.codec).arg(video.audio).arg(video.width).arg(video.height));
}

QByteArray Db::readCapture(const int &percent) const
{
    QSqlQuery query(_db);
    query.exec(QStringLiteral("SELECT at%1 FROM capture WHERE id = '%2';").arg(percent).arg(_id));

    while(query.next())
        return query.value(0).toByteArray();
    return nullptr;
}

void Db::writeCapture(const int &percent, const QByteArray &image) const
{
    QSqlQuery query(_db);
    query.exec(QStringLiteral("INSERT OR IGNORE INTO capture (id) VALUES('%1');").arg(_id));

    query.prepare(QStringLiteral("UPDATE capture SET at%1 = :image WHERE id = '%2';").arg(percent).arg(_id));
    query.bindValue(QStringLiteral(":image"), image);
    query.exec();
}

bool Db::removeVideo(const QString &id) const
{
    QSqlQuery query(_db);

    bool idCached = false;
    query.exec(QStringLiteral("SELECT id FROM metadata WHERE id = '%1';").arg(id));
    while(query.next())
        idCached = true;
    if(!idCached)
        return false;

    query.exec(QStringLiteral("DELETE FROM metadata WHERE id = '%1';").arg(id));
    query.exec(QStringLiteral("DELETE FROM capture WHERE id = '%1';").arg(id));

    query.exec(QStringLiteral("SELECT id FROM metadata WHERE id = '%1';").arg(id));
    while(query.next())
        return false;
    return true;
}
