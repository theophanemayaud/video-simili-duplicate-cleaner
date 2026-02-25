#include "db.h"

#include <QJsonObject>
#include <QJsonDocument>
#include <QVariantMap>

// ----------------------------------------------------------------------
// -------------------- START : public static functions -----------------
/**
 * @abstract: initialize/connect to db, make sure it works as expected (or will notify to user)
 * @input:
 *   -  prefs      for dialogs and will set the cache file location if a valid one is found
 * @return:
 *      - true if successfully found and connected to db/cache file, false if error
 **/
bool Db::initDbAndCacheLocation(Prefs &prefs){
    if(!prefs.cacheFilePathName().isEmpty()
        && initDbAndCache(prefs))
    {
            return true;
    }

    //attempt with system application local cache folder (doesn't seem to work on windows in dev mode
    QDir cacheFolder = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    QString dbfilename = QStringLiteral("%1/cache.db").arg(cacheFolder.path());
    if(!cacheFolder.exists())
        QDir().mkpath(cacheFolder.path());
    prefs.cacheFilePathName(dbfilename);
    if(initDbAndCache(prefs))
        return true;

    //attempt with application folder path
    prefs.cacheFilePathName(QStringLiteral("%1/cache.db").arg(QApplication::applicationDirPath()));
    if(initDbAndCache(prefs))
        return true;

    return false;
}

bool Db::initCustomDbAndCacheLocation(Prefs &prefs){
    QString dbfilename = getUserSelectedCacheNamePath(prefs);
    if(dbfilename.isNull())
        return false;
    prefs.cacheFilePathName(dbfilename);
    return initDbAndCache(prefs);
}


bool Db::emptyAllDb(const Prefs prefs){
    if(prefs.cacheFilePathName().isEmpty()){
        qDebug() << "Database path not set, can't empty cache.";
        return false;
    }

    const QString connexionName = QUuid::createUuid().toString();
    {     // isolate queries, so that when we removeDatabase, it doesn't warn of possible problems
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                connexionName);
        db.setDatabaseName(prefs.cacheFilePathName());
        db.open();

        QSqlQuery query(db);

        query.exec(QStringLiteral("DROP TABLE IF EXISTS metadata"));
        query.exec(QStringLiteral("DROP TABLE IF EXISTS capture"));
        query.exec(QStringLiteral("DROP TABLE IF EXISTS ignored_pairs"));
        query.exec(QStringLiteral("DROP TABLE IF EXISTS version"));

        query.exec(QStringLiteral("VACUUM")); // restructure sqlite file to make it smaller on disk

        Db::createTables(db, prefs.appVersion); // recreate empty tables

        db.close();
    }

    QSqlDatabase().removeDatabase(connexionName); // clear the connexion backlog, basically... !
    return true;
}

// -------------------- END : public static functions -------------------
// ----------------------------------------------------------------------

// ----------------------------------------------------------------------
// -------------------- START : private static functions ----------------
bool Db::initDbAndCache(const Prefs& prefs){
    if(prefs.cacheFilePathName().isEmpty())
        return false;

    const QString uniqueConnexionName = QUuid::createUuid().toString(); // each instance of Db must connect separately, uniquely
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), uniqueConnexionName);
        db.setDatabaseName(prefs.cacheFilePathName());
        if(!db.open()){
            Message::Get()->add(QString("Failed to open db file with driver %1\n    File: %2 - error: %3").arg(db.driverName(), prefs.cacheFilePathName(), db.lastError().text()));
            return false;
        }
        createTables(db, prefs.appVersion);
        db.close();
    }

    QSqlDatabase().removeDatabase(uniqueConnexionName); // clear the connexion backlog, basically... !
    return true;
}

void Db::createTables(QSqlDatabase db, const QString appVersion)
{
    QSqlQuery query(db);
    query.exec(QStringLiteral("PRAGMA synchronous = OFF;"));
    query.exec(QStringLiteral("PRAGMA journal_mode = WAL;"));

    query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS "
                "metadata ("
                    "id TEXT PRIMARY KEY, "
                    "size INTEGER, "
                    "duration INTEGER, "
                    "bitrate INTEGER, "
                    "framerate REAL, "
                    "codec TEXT, "
                    "audio TEXT, "
                    "width INTEGER, "
                    "height INTEGER, "
                    "additional_metadata TEXT"
                ");"
        ));

    query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS "
                "capture ("
                    "id TEXT PRIMARY KEY, "
                    "at8 BLOB, "
                    "at16 BLOB, "
                    "at24 BLOB, "
                    "at32 BLOB, "
                    "at40 BLOB, "
                    "at48 BLOB, "
                    "at56 BLOB, "
                    "at64 BLOB, "
                    "at72 BLOB, "
                    "at80 BLOB, "
                    "at88 BLOB, "
                    "at96 BLOB"
                ");"
        ));

    query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS "
                "ignored_pairs ("
                    "pathName1 TEXT NOT NULL, "
                    "pathName2 TEXT NOT NULL, "
                    "FOREIGN KEY (pathName1) REFERENCES metadata(id), "
                    "FOREIGN KEY (pathName2) REFERENCES metadata(id), "
                    "UNIQUE (pathName1, pathName2) "
                    "PRIMARY KEY (pathName1, pathName2) "
                ");"
        ));


    // Now create a version table and entry, that could help us in the future to check if the database contains old records
    //          and might need to be emptied... ! For now, not used.
    query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS "
                "version ("
                    "version TEXT PRIMARY KEY"
                ");"
        ));
    query.exec(QStringLiteral("INSERT OR REPLACE INTO version VALUES('%1');").arg(appVersion));
}

QString Db::getUserSelectedCacheNamePath(const Prefs &prefs){
    // attempt with user specified location
    QString dbfilename = QFileDialog::getSaveFileName(prefs._mainwPtr, "Save/Load Cache",
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

Db::~Db()
{
    _db.close();
    _db = QSqlDatabase(); // clear the database
    QSqlDatabase::removeDatabase(_uniqueConnexionName); // clear the connexion backlog, basically... !
}

bool Db::readMetadata(Video &video) const
{
    if(!_db.isOpen()){
        qDebug() << "Database not open, can't read Video cache.";
        return false;
    }
    QSqlQuery query(_db);
    query.prepare("SELECT * FROM metadata WHERE id = :id;");
    query.bindValue(":id", video._filePathName);
    query.exec();
    QSqlError error = query.lastError();
    if(error.isValid()){
        qDebug() << "Error with readMetadata query for video=" << video._filePathName << " query="<<query.lastQuery();
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

        QString jsonString = query.value(9).toString();
        QMap<QString, QString> map;
        if (!jsonString.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
            QVariantMap vmap = doc.object().toVariantMap();
            for (auto it = vmap.constBegin(); it != vmap.constEnd(); ++it) {
                map[it.key()] = it.value().toString();
            }
        }
        video.meta.additionalMetadata = map;
        video.meta.setRelevantValuesFromAdditionalMetadata();
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
                  "(id, size, duration, bitrate, framerate, codec, audio, width, height, additional_metadata) "
                  "VALUES(:id,:size,:duration,:bitrate,:framerate,:codec,:audio,:width,:height,:additional_metadata);");
    query.bindValue(":id",          video._filePathName);
    query.bindValue(":size",        static_cast<qlonglong>(video.size));
    query.bindValue(":duration",    static_cast<qlonglong>(video.duration));
    query.bindValue(":bitrate",     video.bitrate);
    query.bindValue(":framerate",   video.framerate);
    query.bindValue(":codec",       video.codec);
    query.bindValue(":audio",       video.audio);
    query.bindValue(":width",       video.width);
    query.bindValue(":height",      video.height);

    QVariantMap vmap;
    for (auto it = video.meta.additionalMetadata.constBegin(); it != video.meta.additionalMetadata.constEnd(); ++it)
        vmap.insert(it.key(), it.value());
    QString jsonString = QJsonDocument(QJsonObject::fromVariantMap(vmap)).toJson(QJsonDocument::Compact);
    query.bindValue(":additional_metadata", jsonString);
    query.exec();

    QSqlError error = query.lastError();
    if(error.isValid()){
        qDebug() << "Error with writeMetadata query for video=" << video._filePathName << " query="<<query.lastQuery();
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

QSet<QString> Db::getCachedVideoPathnamesInFolders(QStringList directoriesPaths) const{
    QSet<QString> videoPathNames;
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
        if(videoPathNames.contains(query.value(0).toString())){
            continue;
        }
        videoPathNames.insert(query.value(0).toString());
    }

    return videoPathNames;
}

void Db::writePairToIgnore(const QString filePathName1, const QString filePathName2) const {
    QSqlQuery query(_db);
    query.prepare("INSERT OR IGNORE INTO "
                    "ignored_pairs (pathName1, pathName2) "
                    "VALUES(:filePathName1, :filePathName2);");
    query.bindValue(":filePathName1", filePathName1);
    query.bindValue(":filePathName2", filePathName2);

    if(!query.exec()){
        qDebug() << "Error with writePairToIgnore query=" << query.lastQuery();
        qDebug() << query.lastError().text();
    }
}

bool Db::isPairToIgnore(const QString filePathName1, const QString filePathName2) const {
    QSqlQuery query(_db);
    query.prepare(
        "SELECT * "
            "FROM ignored_pairs "
            "WHERE "
                "(pathName1 = :filePathName1 AND pathName2 = :filePathName2) "
                "OR "
                "(pathName1 = :filePathName2 AND pathName2 = :filePathName1) "
            "LIMIT 1;"
    );
    query.bindValue(":filePathName1", filePathName1);
    query.bindValue(":filePathName2", filePathName2);

    if(!query.exec()){
        qDebug() << "Error with isPairToIgnore query=" << query.lastQuery();
        qDebug() << query.lastError().text();
        return false;
    }

    if(query.next())
        return true;
    else
        return false;
}
