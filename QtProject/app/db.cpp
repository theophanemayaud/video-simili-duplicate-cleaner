#include "db.h"

// ----------------------------------------------------------------------
// -------------------- START : public static functions -----------------
/**
 * @abstract: initialize/connect to db, make sure it works as expected (or will notify to user)
 * @input:
 *   -  prefs      for dialogs and will set the cache file location if a valid one is found
 * @return:
 *      - true if successfully found and connected to db/cache file, false if error
 **/
bool Db::initDbAndCacheLocation(Prefs *prefs){
    if(!prefs->cacheFilePathName.isEmpty()){
        qDebug() << "Already set cache location, should not do it again !";
        return true;
    }

    const QString uniqueConnexionName = QUuid::createUuid().toString(); // each instance of Db must connect separately, uniquely
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), uniqueConnexionName);

        //attempt with system application local cache folder (doesn't seem to work on windows in dev mode
        QDir cacheFolder = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
        if(!cacheFolder.exists())
            QDir().mkpath(cacheFolder.path());
        QString dbfilename = QStringLiteral("%1/cache.db").arg(cacheFolder.path());
        db.setDatabaseName(dbfilename);

        if(!db.open()){
            //attempt with application folder path
            cacheFolder = QApplication::applicationDirPath();
            dbfilename = QStringLiteral("%1/cache.db").arg(cacheFolder.path());
            db.setDatabaseName(dbfilename);

            if(!db.open()){
                // attempt with user specified location
                QMessageBox::about(prefs->_mainwPtr,
                                   "Default cache location not accessible",
                                   "The default cache location was not writable, please manually select a location for the cache file.");
                dbfilename = getUserSelectedCacheNamePath(prefs);
                db.setDatabaseName(dbfilename);
                if(!db.open()){
                    return false;
                }
            }
        }
        prefs->cacheFilePathName = dbfilename;
        createTables(db, prefs->appVersion);
        db.close();
    }

    QSqlDatabase().removeDatabase(uniqueConnexionName); // clear the connexion backlog, basically... !
    return true;
}

bool Db::initCustomDbAndCacheLocation(Prefs *prefs){
    const QString uniqueConnexionName = QUuid::createUuid().toString(); // each instance of Db must connect separately, uniquely
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), uniqueConnexionName);
        QString dbfilename = getUserSelectedCacheNamePath(prefs);
        db.setDatabaseName(dbfilename);
        if(!db.open()){
            return false;
        }
        prefs->cacheFilePathName = dbfilename;
        createTables(db, prefs->appVersion);
        db.close();
    }

    QSqlDatabase().removeDatabase(uniqueConnexionName); // clear the connexion backlog, basically... !
    return true;
}

void Db::emptyAllDb(const Prefs prefs){
    if(prefs.cacheFilePathName.isEmpty()){
        qDebug() << "Database path not set, can't empty cache.";
        return;
    }

    const QString connexionName = QUuid::createUuid().toString();
    {     // isolate queries, so that when we removeDatabase, it doesn't warn of possible problems
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                connexionName);
        db.setDatabaseName(prefs.cacheFilePathName);
        db.open();

        QSqlQuery query(db);

        query.exec(QStringLiteral("DROP TABLE IF EXISTS metadata"));
        query.exec(QStringLiteral("DROP TABLE IF EXISTS capture"));
        query.exec(QStringLiteral("DROP TABLE IF EXISTS version"));

        query.exec(QStringLiteral("VACUUM")); // restructure sqlite file to make it smaller on disk

        Db::createTables(db, prefs.appVersion); // recreate empty tables

        db.close();
    }

    QSqlDatabase().removeDatabase(connexionName); // clear the connexion backlog, basically... !
}

//QString Db::pathnameHashId(const QString &filename)
//{
//    // Before : usesd file name and modified date, but could lead to two same identified files
//    //          if two files in seperate folders had the same name and modified date.
//    //          It was nice because even after moving files, they could still be identified
//    //          in the database.
//    // Instead simply using full path and name, but we'd need to find a way to better uniquely
//    //          identify, that doesn't depend on path, to have file moving proofness !
//    return QCryptographicHash::hash(filename.toLatin1(), QCryptographicHash::Md5).toHex();
//}
// -------------------- END : public static functions -------------------
// ----------------------------------------------------------------------

// ----------------------------------------------------------------------
// -------------------- START : private static functions ----------------
void Db::createTables(QSqlDatabase db, const QString appVersion)
{
    QSqlQuery query(db);
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
    query.exec(QStringLiteral("INSERT OR REPLACE INTO version VALUES('%1');").arg(appVersion));
}

QString Db::getUserSelectedCacheNamePath(Prefs *prefs){
    // attempt with user specified location
    QString dbfilename = QFileDialog::getSaveFileName(prefs->_mainwPtr, "Save/Load Cache",
                               QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                               "Cache (*.db)");
    return dbfilename;
}
// -------------------- END : private static functions ------------------
// ----------------------------------------------------------------------

Db::Db(const QString cacheFilePathName)
{
    if(cacheFilePathName.isEmpty())
       return;

    _uniqueConnexionName = QUuid::createUuid().toString(); // each instance of Db must connect separately, uniquely
    _db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), _uniqueConnexionName);
    _db.setDatabaseName(cacheFilePathName);
    _db.open();
}

bool Db::readMetadata(Video &video) const
{
    if(!_db.isOpen()){
        qDebug() << "Database not open, can't read Video cache.";
        return false;
    }
    QSqlQuery query(_db);
    query.exec(QStringLiteral("SELECT * FROM metadata WHERE id = '%1';").arg(video.filename));

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
    if(!_db.isOpen()){
        qDebug() << "Database not open, can't write Video cache.";
        return;
    }

    QSqlQuery query(_db);
    query.exec(QStringLiteral("INSERT OR REPLACE INTO metadata VALUES('%1',%2,%3,%4,%5,'%6','%7',%8,%9);")
               .arg(video.filename).arg(video.size).arg(video.duration).arg(video.bitrate).arg(video.framerate)
               .arg(video.codec).arg(video.audio).arg(video.width).arg(video.height));
}

QByteArray Db::readCapture(const QString &filePathname, const int &percent) const
{
    if(!_db.isOpen()){
        qDebug() << "Database not open, can't read capture cache.";
        return nullptr;
    }

    QSqlQuery query(_db);
    query.exec(QStringLiteral("SELECT at%1 FROM capture WHERE id = '%2';").arg(percent).arg(filePathname));
    while(query.next())
        return query.value(0).toByteArray();
    return nullptr;
}

void Db::writeCapture(const QString &filePathname, const int &percent, const QByteArray &image) const
{
    if(!_db.isOpen()){
        qDebug() << "Database not open, can't write capture to cache.";
        return;
    }

    QSqlQuery query(_db);
    query.exec(QStringLiteral("INSERT OR IGNORE INTO capture (id) VALUES('%1');").arg(filePathname));

    query.prepare(QStringLiteral("UPDATE capture SET at%1 = :image WHERE id = '%2';").arg(percent).arg(filePathname));
    query.bindValue(QStringLiteral(":image"), image);
    query.exec();
}

bool Db::removeVideo(const QString &filePathname) const
{
    if(!_db.isOpen()){
        qDebug() << "Database not open, can't remove Video cache.";
        return false;
    }

    QSqlQuery query(_db);

    bool idCached = false;
    query.exec(QStringLiteral("SELECT id FROM metadata WHERE id = '%1';").arg(filePathname));
    while(query.next())
        idCached = true;
    if(!idCached)
        return false;

    query.exec(QStringLiteral("DELETE FROM metadata WHERE id = '%1';").arg(filePathname));
    query.exec(QStringLiteral("DELETE FROM capture WHERE id = '%1';").arg(filePathname));

    query.exec(QStringLiteral("SELECT id FROM metadata WHERE id = '%1';").arg(filePathname));
    while(query.next())
        return false;
    return true;
}
