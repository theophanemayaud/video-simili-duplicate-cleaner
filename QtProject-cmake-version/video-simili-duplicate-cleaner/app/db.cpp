#include "db.h"

// ----------------------------------------------------------------------
// -------------------- START : static functions --------------------------

void Db::emptyAllDb(){
    const QString dbfilename = QStringLiteral("%1/cache.db").arg(QApplication::applicationDirPath());
    const QString connexionName = QUuid::createUuid().toString();

    {     // isolate queries, so that when we removeDatabase, it doesn't warn of possible problems
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                connexionName);
        db.setDatabaseName(dbfilename);
        db.open();

        QSqlQuery query(db);

        query.exec(QStringLiteral("DROP TABLE IF EXISTS metadata"));
        query.exec(QStringLiteral("DROP TABLE IF EXISTS capture"));
        query.exec(QStringLiteral("DROP TABLE IF EXISTS version"));

        query.exec(QStringLiteral("VACUUM")); // restructure sqlite file to make it smaller on disk

        db.close();
    }

    QSqlDatabase().removeDatabase(connexionName); // clear the connexion backlog, basically... !
}

QString Db::pathnameHashId(const QString &filename)
{
    // Before : usesd file name and modified date, but could lead to two same identified files
    //          if two files in seperate folders had the same name and modified date.
    //          It was nice because even after moving files, they could still be identified
    //          in the database.
    // Instead simply using full path and name, but we'd need to find a way to better uniquely
    //          identify, that doesn't depend on path, to have file moving proofness !
    return QCryptographicHash::hash(filename.toLatin1(), QCryptographicHash::Md5).toHex();
}

// -------------------- END : static functions ----------------------------
// ----------------------------------------------------------------------


Db::Db()
{
    _uniqueConnexionName = QUuid::createUuid().toString(); // each instance of Db must connect separately, uniquely
    const QString dbfilename = QStringLiteral("%1/cache.db").arg(QApplication::applicationDirPath());
    _db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), _uniqueConnexionName);
    _db.setDatabaseName(dbfilename);
    _db.open();

    createTables();
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

    // Now create a version key, that could help us in the future to check if the database contains old records
    //          and might need to be emptied... ! For now, not used.
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
    const QString id = pathnameHashId(video.filename);
    QSqlQuery query(_db);
    query.exec(QStringLiteral("SELECT * FROM metadata WHERE id = '%1';").arg(id));

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
    const QString id = pathnameHashId(video.filename);
    QSqlQuery query(_db);
    query.exec(QStringLiteral("INSERT OR REPLACE INTO metadata VALUES('%1',%2,%3,%4,%5,'%6','%7',%8,%9);")
               .arg(id).arg(video.size).arg(video.duration).arg(video.bitrate).arg(video.framerate)
               .arg(video.codec).arg(video.audio).arg(video.width).arg(video.height));
}

QByteArray Db::readCapture(const QString &filePathname, const int &percent) const
{
    const QString id = pathnameHashId(filePathname);
    QSqlQuery query(_db);
    query.exec(QStringLiteral("SELECT at%1 FROM capture WHERE id = '%2';").arg(percent).arg(id));

    while(query.next())
        return query.value(0).toByteArray();
    return nullptr;
}

void Db::writeCapture(const QString &filePathname, const int &percent, const QByteArray &image) const
{
    const QString id = pathnameHashId(filePathname);
    QSqlQuery query(_db);
    query.exec(QStringLiteral("INSERT OR IGNORE INTO capture (id) VALUES('%1');").arg(id));

    query.prepare(QStringLiteral("UPDATE capture SET at%1 = :image WHERE id = '%2';").arg(percent).arg(id));
    query.bindValue(QStringLiteral(":image"), image);
    query.exec();
}

bool Db::removeVideo(const QString &filePathname) const
{
    const QString id = pathnameHashId(filePathname);
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
