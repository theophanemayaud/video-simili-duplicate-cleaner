#include "mainwindow.h"
#include "prefs.h"

MainWindow::MainWindow() : ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    _prefs._mainwPtr = this;

    connect(Message::Get(), SIGNAL(statusMessage(const QString &)), this, SLOT(addStatusMessage(const QString &)));

    QFile file(":/version.txt");
    if (file.open(QIODevice::ReadOnly)){
        _prefs.appVersion = file.readLine();
    }
    file.close();

    Message::Get()->add(QStringLiteral("%1 v%2").arg(APP_NAME, _prefs.appVersion));

#ifdef Q_OS_MACOS
    // Check mac app store receipt
    QString receiptLocation = QString("%1/../_MASReceipt/receipt").arg(QCoreApplication::applicationDirPath());
    bool foundReceipt = QFile::exists(receiptLocation);
#ifdef QT_DEBUG
    QString boolText = foundReceipt ? "true" : "false";
    addStatusMessage("Receipt is found : " + boolText);
    addStatusMessage("At location : " + receiptLocation);
    addStatusMessage("   ");
#else
    if(foundReceipt==false){
        exit(173); // error code as per apple guideline https://developer.apple.com/library/archive/releasenotes/General/ValidateAppStoreReceipt/Chapters/ValidateLocally.html#//apple_ref/doc/uid/TP40010573-CH1-SW21
    }
#endif
#elif defined (Q_OS_WIN)
    //Might want to so something here, maybe with Qt purchasing library with "app" keyword... ?
    // https://doc.qt.io/qt-5/qtpurchasing-windowsstore.html
#endif

    deleteTemporaryFiles();
    loadExtensions();
    setMatchSimilarityThreshold(this->_prefs.matchSimilarityThreshold());

    ui->blocksizeCombo->addItems( { QStringLiteral("2"), QStringLiteral("4"),
                                    QStringLiteral("8"), QStringLiteral("16") } );
    ui->blocksizeCombo->setCurrentIndex(3);
    Thumbnail thumb;
    for(int i=0; i<thumb.countModes(); i++)
        ui->selectThumbnails->addItem(thumb.modeName(i));

    for(int i=0; i<=5; i++)
    {
        ui->differentDurationCombo->addItem(QStringLiteral("%1").arg(i));
        ui->sameDurationCombo->addItem(QStringLiteral("%1").arg(i));
    }
    ui->differentDurationCombo->setCurrentIndex(4);
    ui->sameDurationCombo->setCurrentIndex(1);

    ui->directoryBox->setFocus();
    ui->browseFolders->setIcon(ui->browseFolders->style()->standardIcon(QStyle::SP_DirOpenIcon));
#ifndef Q_OS_MACOS
    ui->browseApplePhotos->hide();
#endif
    ui->processedFiles->setVisible(false);
    ui->progressBar->setVisible(false);
    ui->mainToolBar->setVisible(false);

    if(Db::initDbAndCacheLocation(_prefs))
        addStatusMessage("\nCache located at: " + _prefs.cacheFilePathName() + "\n");
    else
        addStatusMessage("\nError accessing cache, will not use any.\n");

    // load saved/default settings
    if(this->_prefs.isVerbose())
        ui->verboseCheckbox->setCheckState(Qt::Checked);
    foreach(QString folder, this->_prefs.scanLocations()){
        if(!folder.isEmpty())
            this->ui->directoryBox->insert(QStringLiteral("%1;").arg(QDir::toNativeSeparators(folder)));
    }

    on_selectThumbnails_activated(this->_prefs.thumbnailsMode());

    setComparisonMode(this->_prefs.comparisonMode());

    setUseCacheOption(this->_prefs.useCacheOption());
}

void MainWindow::deleteTemporaryFiles() const
{
    QDir tempDir = QDir::tempPath();    //QTemporaryDir remains if program force quit
    tempDir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    QDirIterator iter(tempDir);
    while(iter.hasNext())
    {
        QDir dir = iter.next();
        if(dir.dirName().compare(QStringLiteral("DupVids-")) == 1) //TODO : temporary vids have new identifier...
            dir.removeRecursively();
    }
}

void MainWindow::loadExtensions()
{
    //DEBUGTHEO for windows QFile file(QStringLiteral("%1/extensions.ini").arg(QApplication::applicationDirPath()));
    //But for mac here it is
    //Working on mac : QFile file(QStringLiteral("%1/../Frameworks/extensions.ini").arg(QApplication::applicationDirPath()));
    //Test witth qrc file with extensions.ini at / :
    QFile file(QStringLiteral(":/extensions.ini")); //using qrc ressource path ! This works on mac at least !
    if(!file.open(QIODevice::ReadOnly))
    {
        addStatusMessage(QStringLiteral("Error: extensions.ini not found. No video file will be searched."));
        return;
    }
    addStatusMessage(QStringLiteral("Currently supported file extensions:"));
    QTextStream text(&file);
    while(!text.atEnd())
    {
        QString line = text.readLine();
        if(line.startsWith(QStringLiteral(";")) || line.isEmpty())
            continue;
        _extensionList << line.replace(QRegularExpression("\\*?\\."), "*.").split(QStringLiteral(" "));
        addStatusMessage(line.remove(QStringLiteral("*")));
    }
    file.close();
}

// ---------------------------------------------------------------------------------------------
// -----------------------START: add scan locations  -------------------------------------------
void MainWindow::on_browseFolders_clicked()
{
    QString dir = this->_prefs.browseFoldersLastPath();
    if(dir.isEmpty())
        dir = QStandardPaths::standardLocations(QStandardPaths::MoviesLocation).first();

    dir = QFileDialog::getExistingDirectory(ui->browseFolders,
                                              QByteArrayLiteral("Open folder"),
                                              dir /*defines where the chooser opens at*/,
                                              QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if(dir.isEmpty()){ //empty because error or none chosen in dialog
        return;
    }
    ui->directoryBox->insert(QStringLiteral(";%1").arg(QDir::toNativeSeparators(dir)));
    ui->directoryBox->setFocus();
    this->_prefs.browseFoldersLastPath(dir);
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QString fileName = event->mimeData()->urls().first().toLocalFile();
    const QFileInfo file(fileName);
    if(file.isDir())
        ui->directoryBox->insert(QStringLiteral(";%1").arg(QDir::toNativeSeparators(fileName)));
}

void MainWindow::on_browseApplePhotos_clicked()
{
    QString dir = this->_prefs.browseApplePhotosLastPath();
    if(dir.isEmpty())
        dir = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation).first();

    const QString path = QFileDialog::getOpenFileName(ui->browseFolders,
                                                    QByteArrayLiteral("Select Apple Photos Library"),
                                                    dir /*defines where the chooser opens at*/,
                                                    tr("Apple Photos Library (*.photoslibrary)"));
    if(path.isEmpty()){ //empty because error or none chosen in dialog
        return;
    }
    ui->directoryBox->insert(QStringLiteral(";%1").arg(QDir::toNativeSeparators(path)));
    ui->directoryBox->setFocus();
    this->_prefs.browseApplePhotosLastPath(QFileInfo(path).absolutePath());
}
// ----------------------- END: add scan locations ---------------------------------------------
// ---------------------------------------------------------------------------------------------

void MainWindow::on_directoryBox_textChanged(const QString &arg1)
{
    if(arg1.trimmed().isEmpty())
        this->_prefs.scanLocations(QStringList());
}

void MainWindow::on_findDuplicates_clicked()
{
    if(ui->findDuplicates->text() == "Stop")     //pressing "find duplicates" button will morph into a
    {                                                           //stop button. a lengthy search can thus be stopped and
        _userPressedStop = true;                                //those videos already processed are compared w/each other
        return;
    }
    else
    {
        ui->findDuplicates->setText(QStringLiteral("Stop"));
        _userPressedStop = false;
    }
    if(_extensionList.isEmpty())
    {
        addStatusMessage(QStringLiteral("Error: No extensions found in extensions.ini. No video file will be searched."));
        return;
    }

    const QString foldersToSearch = ui->directoryBox->text();   //search only if folder or thumbnail settings have changed
    if(foldersToSearch != _previousRunFolders || this->_prefs.thumbnailsMode() != _previousRunThumbnails)
    {
        addStatusMessage(QStringLiteral("\n\rSearching for videos..."));
        ui->statusBar->setVisible(true);

        for(const auto &video : _videoList)                     //new search: delete videos from previous search
            delete video;
        _videoList.clear();
        _everyVideo.clear();

        const QStringList directories = foldersToSearch.split(QStringLiteral(";"));
        this->_prefs.scanLocations(directories);
        if(this->_prefs.useCacheOption()!=Prefs::CACHE_ONLY){
            QString notFound;
            for(auto directory : directories)               //add all video files from entered paths to list
            {
                QApplication::processEvents();
                if(directory.isEmpty())
                    continue;
                QDir dir = directory.remove(QStringLiteral("\""));
                if(dir.exists())
                    findVideos(dir);
                else
                {
                    addStatusMessage(QStringLiteral("Cannot find folder: %1").arg(QDir::toNativeSeparators(dir.path())));
                    notFound += QStringLiteral("%1 ").arg(QDir::toNativeSeparators(dir.path()));
                }
            }
            if(!notFound.isEmpty())
                ui->statusBar->showMessage(QStringLiteral("Cannot find folder: %1").arg(notFound));
        }
        else{
            _everyVideo = Db(_prefs.cacheFilePathName()).getCachedVideoPathnamesInFolders(directories);
        }

        processVideos();
    }

    if(_videoList.count() > 1)
    {
        _comparison = new Comparison(sortVideosBySize(), _prefs);
        if(foldersToSearch != _previousRunFolders || this->_prefs.thumbnailsMode() != _previousRunThumbnails)
        {
            QFuture<int> future = QtConcurrent::run(&Comparison::reportMatchingVideos, _comparison);   //run in background
            _comparison->exec();          //open dialog, but if it is closed while reportMatchingVideos() still running...
            QApplication::setOverrideCursor(Qt::WaitCursor);
            future.waitForFinished();   //...must wait until finished (crash when going out of scope destroys instance)
            QApplication::restoreOverrideCursor();
        }
        else
            _comparison->exec();

        delete _comparison;
        _comparison = nullptr;

        _previousRunFolders = foldersToSearch;                  //videos are still held in memory until
        _previousRunThumbnails = this->_prefs.thumbnailsMode();            //folders to search or thumbnail mode are changed
    }

    ui->findDuplicates->setText(QStringLiteral("Find duplicates"));
}

void MainWindow::findVideos(QDir &dir)
{
    dir.setNameFilters(_extensionList);
    QDirIterator iter(dir, QDirIterator::Subdirectories);
    while(iter.hasNext())
    {
        QApplication::processEvents();
        if(_userPressedStop){
            _userPressedStop=false; //user needs to press 2x to stop the find videos process, then process videos process.
            return;
        }
        const QString filePathName = iter.nextFileInfo().canonicalFilePath();

        if(_everyVideo.contains(filePathName)) //don't want duplicates of same file
            continue;
        _everyVideo.insert(filePathName);

        ui->statusBar->showMessage(QStringLiteral("Found %1 videos | %2")
                                   .arg(_everyVideo.size())
                                   .arg(QDir::toNativeSeparators(filePathName)), 10);
    }
}

QVector<Video *> MainWindow::sortVideosBySize() const {
    QVector<Video *> sortedVideosList;

    if(_prefs._numberOfVideos <= 0) //no videos to sort
        return sortedVideosList;

    QMultiMap<int64_t, int> mappedVideos; // key is video size, value is index in video QVector (maps are sorted automagically by key)
    for(int i=0; i<_videoList.size(); i++){
        mappedVideos.insert(_videoList[i]->size, i);
    }

    QMultiMapIterator<int64_t, int> mappedVidSize(mappedVideos); //video size, then video index
    while (mappedVidSize.hasNext()) { // iterating from smaller size to bigger sizes
        sortedVideosList.insert(0, _videoList[mappedVidSize.next().value()]);
    }

    if(sortedVideosList.size()!=_videoList.size())
        qDebug() << "Problem sorting through videos, sorted size=" << sortedVideosList.size() << ", but unsorted size=" << _videoList.size();

    return sortedVideosList;
}

void MainWindow::processVideos()
{
    _prefs._numberOfVideos = _everyVideo.count();
    if(this->_prefs.useCacheOption()!=Prefs::CACHE_ONLY)
        addStatusMessage(QStringLiteral("\nFound %1 video file(s):\n").arg(_prefs._numberOfVideos));
    else
        addStatusMessage(QStringLiteral("\nFound %1 cached video file(s):\n").arg(_prefs._numberOfVideos));
    if(_prefs._numberOfVideos > 0)
    {
        ui->selectThumbnails->setDisabled(true);
        ui->processedFiles->setVisible(true);
        if(this->_prefs.useCacheOption()!=Prefs::CACHE_ONLY)
            ui->processedFiles->setText(QStringLiteral("0/%1").arg(_prefs._numberOfVideos));
        else
            ui->processedFiles->setText(QStringLiteral("Loading cache 0/%1").arg(_prefs._numberOfVideos));
        if(ui->statusBar->currentMessage().indexOf(QStringLiteral("Cannot find folder")) == -1)
            ui->statusBar->setVisible(false);
        ui->progressBar->setVisible(true);
        ui->progressBar->setValue(0);
        ui->progressBar->setMaximum(_prefs._numberOfVideos);
        ui->statusBox->verticalScrollBar()->triggerAction(QScrollBar::SliderToMaximum);
    }
    else return;

    QThreadPool threadPool;
    for(auto vidIter = _everyVideo.constBegin(), end = _everyVideo.constEnd(); vidIter != end; ++vidIter)
    {
        if(_userPressedStop)
        {
            threadPool.clear();
            break;
        }
        while(threadPool.activeThreadCount() == threadPool.maxThreadCount()){
//        while(threadPool.activeThreadCount() == 1){ // useful to debug manually, where threading causes debug logs confusion !
            QApplication::processEvents();          //avoid blocking signals in event loop
        }
        auto *videoTask = new Video(_prefs, *vidIter);
        videoTask->setAutoDelete(false);
        threadPool.start(videoTask);
    }
    threadPool.waitForDone();
    QApplication::processEvents();                  //process signals from last threads

    ui->selectThumbnails->setDisabled(false);
    ui->processedFiles->setVisible(false);
    ui->progressBar->setVisible(false);
    ui->statusBar->setVisible(false);
    _prefs._numberOfVideos = _videoList.count();    //minus rejected ones now
    videoSummary();
}

void MainWindow::videoSummary()
{
    if(_rejectedVideos.empty())
        addStatusMessage(QStringLiteral("\n\r%1 intact video(s) found").arg(_prefs._numberOfVideos));
    else
    {
        addStatusMessage(QStringLiteral("\n\r%1 intact video(s) out of %2 total").arg(_prefs._numberOfVideos)
                                                                             .arg(_everyVideo.count()));
        //Following prints are not necessary as it already prints each error as it happens.
//        addStatusMessage(QStringLiteral("\nThe following %1 video(s) could not be added due to errors:")
//                         .arg(_rejectedVideos.count()));
//        for(const auto &filename : _rejectedVideos)
//            addStatusMessage(filename);
    }
    _rejectedVideos.clear();
}

void MainWindow::addStatusMessage(const QString &message) const
{
    ui->statusBox->append(message);

    if(this->_prefs.isVerbose()){
        QDir cacheFolder = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
        if(!cacheFolder.exists())
            QDir().mkpath(cacheFolder.path());
        static QFile logFile = QStringLiteral("%1/%2_%3.vsdc.logs.txt")
                                   .arg(cacheFolder.path(),
                                        this->_prefs.appVersion.trimmed(),
                                        QDateTime::currentDateTime().toString("yyyy-MM-dd hh_mm"));
        static QTextStream logStream;
        if (!logFile.isOpen()) {
            // Open the file in Append mode and keep it open
            if (!logFile.open(QIODevice::Append | QIODevice::Text)) {
                ui->statusBox->append("Failed to open log file");
            }
            else {
                // Initialize the QTextStream using the open file
                logStream.setDevice(&logFile);
            }
        }
        else {
            logStream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")
                      << "->" << message << Qt::endl;
        }
    }

    ui->statusBox->repaint();
#ifdef VID_SIMILI_IN_TESTS
    qDebug() << message;
#endif
}

void MainWindow::addVideo(Video *addMe)
{
    if(this->_prefs.isVerbose()){
        addStatusMessage(QStringLiteral("[%1] SUCCESS %2").arg(QTime::currentTime().toString(),
                                                       QDir::toNativeSeparators(addMe->_filePathName)));
    }
    ui->progressBar->setValue(ui->progressBar->value() + 1);
    if(this->_prefs.useCacheOption()!=Prefs::CACHE_ONLY)
        ui->processedFiles->setText(QStringLiteral("%1/%2").arg(ui->progressBar->value()).arg(ui->progressBar->maximum()));
    else
        ui->processedFiles->setText(QStringLiteral("Loading cache %1/%2").arg(ui->progressBar->value()).arg(ui->progressBar->maximum()));

    _videoList << addMe;
}

void MainWindow::removeVideo(Video *deleteMe, QString errorMsg)
{
    if (deleteMe != nullptr) {
        addStatusMessage(QStringLiteral("[%1] ERROR with %2 : %3")
                         .arg(QTime::currentTime().toString())
                         .arg(QDir::toNativeSeparators(deleteMe->_filePathName))
                         .arg(errorMsg));
        ui->progressBar->setValue(ui->progressBar->value() + 1);
        ui->processedFiles->setText(QStringLiteral("%1/%2").arg(ui->progressBar->value()).arg(ui->progressBar->maximum()));
        _rejectedVideos << QDir::toNativeSeparators(deleteMe->_filePathName);
        delete deleteMe;
    }
    else{
        qCritical() << Q_FUNC_INFO << ": attempted to delete deleteMe but is already null";
    }
}

void MainWindow::on_actionAbout_triggered()
{
    QDesktopServices::openUrl(QUrl("https://theophanemayaud.github.io/video-simili-duplicate-cleaner/"));
}

void MainWindow::on_actionEmpty_cache_triggered()
{
    if(Db::emptyAllDb(_prefs))
        addStatusMessage(QString("\nEmptied cache at:  %1\n").arg(_prefs.cacheFilePathName()));
    else
        addStatusMessage(QString("\nFailed to empty cache at:  %1\n").arg(_prefs.cacheFilePathName()));
}

void MainWindow::on_actionSet_custom_cache_location_triggered()
{
    if(Db::initCustomDbAndCacheLocation(_prefs)){
        addStatusMessage(QString("\nCache now used:  %1\n").arg(_prefs.cacheFilePathName()));
    }
    else
        addStatusMessage(QString("\nError selecting custom cache. Probably no cache now.\n"));
}

void MainWindow::on_actionRestore_all_settings_triggered()
{
    this->_prefs.resetSettings();

    on_actionRestore_default_cache_location_triggered();

    on_actionRestoreMoveToTrash_triggered();
    ui->verboseCheckbox->setCheckState(Qt::Unchecked);

    this->ui->directoryBox->clear();

    on_selectThumbnails_activated(cutEnds);

    setComparisonMode(this->_prefs.comparisonMode());

    setMatchSimilarityThreshold(this->_prefs.matchSimilarityThreshold());

    setUseCacheOption(this->_prefs.useCacheOption());
}

void MainWindow::on_actionRestore_default_cache_location_triggered()
{
    _prefs.cacheFilePathName("");
    if(Db::initDbAndCacheLocation(_prefs))
        addStatusMessage("\nCache restored to: " + _prefs.cacheFilePathName() + "\n");
    else
        addStatusMessage(QString("\nError restoring default cache. Probably no cache now.\n"));
}

void MainWindow::on_actionCredits_triggered()
{
    QFile file(":/CREDITS.md");
    QString credits = "undefined";
    if (file.open(QIODevice::ReadOnly)){
        credits = file.readAll();
    }
    file.close();

    QWidget *ui_credits = new QWidget;
    ui_credits->setWindowTitle("Credits");
    QVBoxLayout *layout = new QVBoxLayout(ui_credits);
    layout->setContentsMargins(0,0,0,0);
    QTextEdit *label = new QTextEdit(ui_credits);
    label->setDisabled(true);
    label->setMarkdown(credits);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    label->setMinimumSize(ui->centralWidget->size());
    layout->addWidget(label);

    ui_credits->setLayout(layout);
    ui_credits->show();
}

void MainWindow::on_actionContact_triggered()
{
    QDesktopServices::openUrl(QUrl("https://github.com/theophanemayaud/video-simili-duplicate-cleaner/discussions"));
}

// ----------------------------------------------------------------------------
// ------------------- BEGIN: File deletion configuration methods -----------
void MainWindow::on_actionChange_trash_folder_triggered()
{
    // initially, files will be moved to trash. With this button, another folder can be selected
    // into which to move files upon removal, instead of trash.
    QString dir = QFileDialog::getExistingDirectory(ui->browseFolders,
                          QByteArrayLiteral("Open folder"),
                          QStandardPaths::standardLocations(QStandardPaths::MoviesLocation).first() /*defines where the chooser opens at*/,
                          QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if(dir.isEmpty()){ //empty because error or none chosen in dialog
        QMessageBox::information(this, "", "The folder selected seems not defined, try another one");
        return;
    }
    if(!QDir(dir).exists()){ // we must make sure it exists, or we fail and reset to default root (signals trash)
        QMessageBox::information(this, "", "The folder selected doesn't seem to exist, please create it first");
        return;
    }
    _prefs.trashDir = QDir(dir);
    // we successfully set custom trash location
    _prefs.delMode = Prefs::CUSTOM_TRASH;
    ui->actionChange_trash_folder->setEnabled(false);
    ui->actionEnable_direct_deletion_instead_of_trash->setEnabled(true);
    ui->actionRestoreMoveToTrash->setEnabled(true);
    addStatusMessage(QString("\nRemoved files will now be moved to %1 folder instead of trash\n").arg(_prefs.trashDir.absolutePath()));
}

void MainWindow::on_actionEnable_direct_deletion_instead_of_trash_triggered()
{
    _prefs.delMode = Prefs::DIRECT_DELETION;
    ui->actionChange_trash_folder->setEnabled(true);
    ui->actionEnable_direct_deletion_instead_of_trash->setEnabled(false);
    ui->actionRestoreMoveToTrash->setEnabled(true);
    addStatusMessage("\nRemoved files will now be deleted directly and completely. Be extra careful what you do !\n");
}

void MainWindow::on_actionRestoreMoveToTrash_triggered()
{
    // A  click on this button restores the default "trash" as "move to trash" destination
    // here we must restore the default "move to trash" behavior
    _prefs.delMode = Prefs::STANDARD_TRASH;
    _prefs.trashDir = QDir::root(); //reset to known, controlled state
    ui->actionChange_trash_folder->setEnabled(true);
    ui->actionEnable_direct_deletion_instead_of_trash->setEnabled(true);
    ui->actionRestoreMoveToTrash->setEnabled(false);
    addStatusMessage(QString("\nRemoved files will now be moved to trash\n"));
}
// ------------------- END: File deletion configuration methods -----------
// ----------------------------------------------------------------------------

void MainWindow::on_actionDelete_log_files_triggered()
{
    QDir logsFolder = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    // Check if the directory exists
    if (!logsFolder.exists()) {
        addStatusMessage(QString("Logs folder does not exist: %1").arg(logsFolder.absolutePath()));
        return;
    }

    // List all .txt files in the cache folder
    QStringList txtFiles = logsFolder.entryList(QStringList() << "*.vsdc.logs.txt", QDir::Files);

    if(txtFiles.isEmpty())
        addStatusMessage(QString("No log files found in logs folder: %1").arg(logsFolder.absolutePath()));

    // Loop through the list of .txt files and delete them
    for (const QString &fileName : txtFiles) {
        QString filePath = logsFolder.absoluteFilePath(fileName);  // Get the full file path
        if (QFile::remove(filePath)) {
            addStatusMessage(QString("Deleted log file: %1").arg(filePath));
        } else {
            addStatusMessage(QString("Failed to delete log file: %1").arg(filePath));
        }
    }
}

void MainWindow::on_actionOpen_logs_folder_triggered()
{
    QDir logsFolder = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    if (!logsFolder.exists()) {
        addStatusMessage(QString("Logs folder does not exist: %1").arg(logsFolder.absolutePath()));
        return;
    }

#ifdef Q_OS_WIN
    QProcess::startDetached("explorer", {"/select,", logsFolder.absolutePath()});
#elif defined(Q_OS_MACOS)
    QProcess::startDetached("open", QStringList() << logsFolder.absolutePath());
#elif defined(Q_OS_X11)
    QProcess::startDetached(QStringLiteral("xdg-open \"%1\"").arg(logsFolder.absolutePath()));
#endif

}

// ----------------------------------------------------------------------------
// ------------------- BEGIN: Scan settings -----------------------------------
void MainWindow::setComparisonMode(const int &mode) { // slot used from comparison.cpp to stay in sync
    this->_prefs.comparisonMode(static_cast<Prefs::VisualComparisonModes>(mode));
    if(mode == Prefs::_PHASH)
        ui->selectPhash->setChecked(true);
    else
        ui->selectSSIM->setChecked(true);
    ui->directoryBox->setFocus();
}
void MainWindow::on_selectThumbnails_activated(const int &index) {
    this->_prefs.thumbnailsMode(index);
    this->ui->directoryBox->setFocus();
    this->ui->selectThumbnails->setCurrentIndex(index); // set even if was just selected as function is also called in code
    if(index == cutEnds)
        this->ui->differentDurationCombo->setCurrentIndex(0); // cutends already implicitely raises bar for duration, so no need to adjust threshold higher
}
void MainWindow::on_selectPhash_clicked(const bool &checked) {
    if(checked)
        setComparisonMode(Prefs::_PHASH);
}
void MainWindow::on_selectSSIM_clicked(const bool &checked) {
    if(checked)
        setComparisonMode(_prefs._SSIM);
}
void MainWindow::on_blocksizeCombo_activated(const int &index) {
    _prefs._ssimBlockSize = static_cast<int>(pow(2, index+1));
    ui->directoryBox->setFocus();
}
void MainWindow::on_differentDurationCombo_activated(const int &index) {
    _prefs._differentDurationModifier = index;
    ui->directoryBox->setFocus();
}
void MainWindow::on_sameDurationCombo_activated(const int &index) {
    _prefs._sameDurationModifier = index;
    ui->directoryBox->setFocus();
}
void MainWindow::on_thresholdSlider_valueChanged(const int &value) {
    setMatchSimilarityThreshold(value);
    ui->directoryBox->setFocus();
}
void MainWindow::setMatchSimilarityThreshold(const int &value) {
    this->_prefs.matchSimilarityThreshold(value);

    _prefs._thresholdSSIM = value / 100.0;
    const int matchingBitsOf64 = static_cast<int>(round(64 * _prefs._thresholdSSIM));
    _prefs._thresholdPhash = matchingBitsOf64;

    this->ui->thresholdSlider->setValue(value);
    this->ui->threshPercent->setNum(value);
    const QString thresholdMessage = QStringLiteral(
        "Threshold: %1% (%2/64 bits = match)   Default: %3%\n"
        "Smaller: less strict, can match different videos (false positive)\n"
        "Larger: more strict, can miss identical videos (false negative)")
        .arg(value).arg(matchingBitsOf64).arg((int)(100*Prefs::DEFAULT_SSIM_THRESHOLD+0.5));
    ui->thresholdSlider->setToolTip(thresholdMessage);
}
void MainWindow::setUseCacheOption(Prefs::USE_CACHE_OPTION opt) {
    this->_prefs.useCacheOption(opt);
    switch (opt) {
    case Prefs::NO_CACHE:
        this->ui->radio_UseCacheNo->setChecked(true);
        break;
    case Prefs::CACHE_ONLY:
        this->ui->radio_UseCacheOnly->setChecked(true);
        break;
    default:
        this->ui->radio_UseCacheYes->setChecked(true);
        break;
    }
}
// ------------------- END: Scan settings -------------------------------------
// ----------------------------------------------------------------------------
