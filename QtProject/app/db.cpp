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
        if(dbfilename.isNull())
            return false;
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
    query.prepare("SELECT * FROM metadata WHERE id = :id;");
    query.bindValue(":id", video.filename);
    query.exec();
    QSqlError error = query.lastError();
    if(error.isValid()){
        qDebug() << "Error with readMetadata query for video=" << video.filename << " query="<<query.lastQuery();
        qDebug() << error.text();
    }

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
    query.prepare("INSERT OR REPLACE INTO metadata "
                  "VALUES(:id,:size,:duration,:bitrate,:framerate,"
                  ":codec,:audio,:width,:height);");
    query.bindValue(":id",          video.filename);
    query.bindValue(":size",        video.size);
    query.bindValue(":duration",    video.duration);
    query.bindValue(":bitrate",     video.bitrate);
    query.bindValue(":framerate",   video.framerate);
    query.bindValue(":codec",       video.codec);
    query.bindValue(":audio",       video.audio);
    query.bindValue(":width",       video.width);
    query.bindValue(":height",      video.height);
    query.exec();

    QSqlError error = query.lastError();
    if(error.isValid()){
        qDebug() << "Error with readmetadata query for video=" << video.filename << " query="<<query.lastQuery();
        qDebug() << error.text();
    }
}

QByteArray Db::readCapture(const QString &filePathname, const int &percent) const
{
    if(!_db.isOpen()){
        qDebug() << "Database not open, can't read capture cache.";
        return nullptr;
    }

    QSqlQuery query(_db);
    query.prepare(QString("SELECT at%1 FROM capture WHERE id = :id;").arg(percent));
    query.bindValue(":id",filePathname);
    query.exec();

    QSqlError error = query.lastError();
    if(error.isValid()){
        qDebug() << "Error with readCapture query for video=" << filePathname << " query="<<query.lastQuery();
        qDebug() << error.text();
    }


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
    query.prepare("INSERT OR IGNORE INTO capture (id) VALUES(:id);");
    query.bindValue(":id", filePathname);
    query.exec();
    QSqlError error = query.lastError();
    if(error.isValid()){
        qDebug() << "Error with writeCapture query for video=" << filePathname << " query="<<query.lastQuery();
        qDebug() << error.text();
    }

    query.prepare(QString("UPDATE capture SET at%1 = :image WHERE id = :id;").arg(percent));
    query.bindValue(":id",filePathname);
    query.bindValue(":image", image);
    query.exec();

    error = query.lastError();
    if(error.isValid()){
        qDebug() << "Error with writeCapture query for video=" << filePathname << " query="<<query.lastQuery();
        qDebug() << error.text();
    }

}

bool Db::removeVideo(const QString &filePathname) const
{
    if(!_db.isOpen()){
        qDebug() << "Database not open, can't remove Video cache.";
        return false;
    }

    QSqlQuery query(_db);

    bool idCached = false;
    query.prepare("SELECT id FROM metadata WHERE id = :id;");
    query.bindValue(":id", filePathname);
    query.exec();
    while(query.next())
        idCached = true;
    if(!idCached)
        return false;

    query.prepare("DELETE FROM metadata WHERE id = :id;");
    query.bindValue(":id", filePathname);
    query.exec();
    QSqlError error = query.lastError();
    if(error.isValid()){
        qDebug() << "Error with removeVideo query for video=" << filePathname << " query="<<query.lastQuery();
        qDebug() << error.text();
    }

    query.prepare("DELETE FROM capture WHERE id = :id;");
    query.bindValue(":id", filePathname);
    query.exec();
    error = query.lastError();
    if(error.isValid()){
        qDebug() << "Error with removeVideo query for video=" << filePathname << " query="<<query.lastQuery();
        qDebug() << error.text();
    }

    query.prepare("SELECT id FROM metadata WHERE id = :id;");
    query.bindValue(":id", filePathname);
    query.exec();
    error = query.lastError();
    if(error.isValid()){
        qDebug() << "Error with removeVideo query for video=" << filePathname << " query="<<query.lastQuery();
        qDebug() << error.text();
    }

    while(query.next())
        return false;
    return true;
}

QStringList Db::getCachedVideoPathnamesInFolders(QStringList directoriesPaths) const{
    QStringList videoPathNames;
    directoriesPaths.removeAll("");

    if(!_db.isOpen()){
        qDebug() << "Database not open, can't read Video cache.";
        return videoPathNames;
    }
    else if(directoriesPaths.isEmpty()){
        qDebug() << "No directories selected.";
        return videoPathNames;
    }

    QString queryText = "SELECT id FROM metadata WHERE id LIKE ? || '%'";
    for(auto i = directoriesPaths.count() - 1; i>=1; i--){ //first is already handled
        queryText = queryText + " OR id LIKE ? || '%'";
    }

    QSqlQuery query(_db);
    query.prepare(queryText);
    foreach(QString dirPath, directoriesPaths){
        //according to https://stackoverflow.com/questions/1428197/how-to-escape-a-string-for-use-with-the-like-operator-in-sql-server
        // only these three characters need to be handled and encapsulated
        query.addBindValue(QDir(dirPath).absolutePath().replace("[","[[]").replace("%", "[%]").replace("_","[_]"));
    }
    query.exec();
    QSqlError error = query.lastError();
    if(error.isValid()){
        qDebug() << "Error with get cached pathnames query="<<queryText;
        qDebug() << error.text();
    }

    while(query.next()){
        if(videoPathNames.contains(query.value(0))){
            qDebug() << query.value(0) << " was already in list !!!"; // this shouldn't happen but debug just in case...
        }
        videoPathNames.append(query.value(0).toString());
    }

    return videoPathNames;
}
