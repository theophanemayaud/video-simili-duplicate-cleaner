#include <QFileDialog>
#include <QtConcurrent/QtConcurrent>
#include <QScrollBar>
#include "mainwindow.h"

MainWindow::MainWindow() : ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    _prefs._mainwPtr = this;

    QFile file(":/version.txt");
    QString appVersion = "undefined";
    if (file.open(QIODevice::ReadOnly)){
        appVersion = file.readLine();
    }
    file.close();

    ui->statusBox->append(QStringLiteral("%1 v%2").arg(APP_NAME, appVersion));
#ifdef Q_OS_MACOS
    // Check mac app store receipt
    QString receiptLocation = QString("%1/../_MASReceipt/receipt").arg(QCoreApplication::applicationDirPath());
    bool foundReceipt = QFile::exists(receiptLocation);
#ifdef QT_DEBUG
    QString boolText = foundReceipt ? "true" : "false";
    ui->statusBox->append("Receipt is found : " + boolText);
    ui->statusBox->append("At location : " + receiptLocation);
    ui->statusBox->append("   ");
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
    calculateThreshold(ui->thresholdSlider->sliderPosition());

    ui->blocksizeCombo->addItems( { QStringLiteral("2"), QStringLiteral("4"),
                                    QStringLiteral("8"), QStringLiteral("16") } );
    ui->blocksizeCombo->setCurrentIndex(3);
    Thumbnail thumb;
    for(int i=0; i<thumb.countModes(); i++)
        ui->selectThumbnails->addItem(thumb.modeName(i));
    ui->selectThumbnails->setCurrentIndex(7);

    for(int i=0; i<=5; i++)
    {
        ui->differentDurationCombo->addItem(QStringLiteral("%1").arg(i));
        ui->sameDurationCombo->addItem(QStringLiteral("%1").arg(i));
    }
    ui->differentDurationCombo->setCurrentIndex(4);
    ui->sameDurationCombo->setCurrentIndex(1);

    ui->directoryBox->setFocus();
    ui->browseFolders->setIcon(ui->browseFolders->style()->standardIcon(QStyle::SP_DirOpenIcon));
    ui->processedFiles->setVisible(false);
    ui->progressBar->setVisible(false);
    ui->mainToolBar->setVisible(false);
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

void MainWindow::dropEvent(QDropEvent *event)
{
    const QString fileName = event->mimeData()->urls().first().toLocalFile();
    const QFileInfo file(fileName);
    if(file.isDir())
        ui->directoryBox->insert(QStringLiteral(";%1").arg(QDir::toNativeSeparators(fileName)));
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
        _extensionList << line.replace(QRegExp("\\*?\\."), "*.").split(QStringLiteral(" "));
        addStatusMessage(line.remove(QStringLiteral("*")));
    }
    file.close();
}

void MainWindow::calculateThreshold(const int &value)
{
    _prefs._thresholdSSIM = value / 100.0;
    const int matchingBitsOf64 = static_cast<int>(round(64 * _prefs._thresholdSSIM));
    _prefs._thresholdPhash = matchingBitsOf64;

    const QString thresholdMessage = QStringLiteral(
                "Threshold: %1% (%2/64 bits = match)   Default: %3%\n"
                "Smaller: less strict, can match different videos (false positive)\n"
                "Larger: more strict, can miss identical videos (false negative)")
            .arg(value).arg(matchingBitsOf64).arg((int)(100*SSIM_THRESHOLD+0.5));
    ui->thresholdSlider->setToolTip(thresholdMessage);
}

void MainWindow::on_browseFolders_clicked() const
{
    const QString dir = QFileDialog::getExistingDirectory(ui->browseFolders,
                                                          QByteArrayLiteral("Open folder"),
                                                          QStandardPaths::standardLocations(QStandardPaths::MoviesLocation).first() /*defines where the chooser opens at*/,
                                                          QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if(dir.isEmpty()){ //empty because error or none chosen in dialog
        return;
    }
    ui->directoryBox->insert(QStringLiteral(";%1").arg(QDir::toNativeSeparators(dir)));
    ui->directoryBox->setFocus();
}

void MainWindow::on_findDuplicates_clicked()
{
    if(ui->findDuplicates->text() == QLatin1String("Stop"))     //pressing "find duplicates" button will morph into a
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
    if(foldersToSearch != _previousRunFolders || _prefs._thumbnails != _previousRunThumbnails)
    {
        ui->statusBox->append(QStringLiteral("\n\rSearching for videos..."));
        ui->statusBar->setVisible(true);

        for(const auto &video : _videoList)                     //new search: delete videos from previous search
            delete video;
        _videoList.clear();
        _everyVideo.clear();

        const QStringList directories = foldersToSearch.split(QStringLiteral(";"));
        QString notFound;
        for(auto directory : directories)               //add all video files from entered paths to list
        {
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

        processVideos();
    }

    if(_videoList.count() > 1)
    {
        _comparison = new Comparison(sortVideosBySize(), _prefs);
        if(foldersToSearch != _previousRunFolders || _prefs._thumbnails != _previousRunThumbnails)
        {
            QFuture<void> future = QtConcurrent::run(_comparison, &Comparison::reportMatchingVideos);   //run in background
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
        _previousRunThumbnails = _prefs._thumbnails;            //folders to search or thumbnail mode are changed
    }

    ui->findDuplicates->setText(QStringLiteral("Find duplicates"));
}

void MainWindow::findVideos(QDir &dir)
{
    dir.setNameFilters(_extensionList);
    QDirIterator iter(dir, QDirIterator::Subdirectories);
    while(iter.hasNext())
    {
        if(_userPressedStop){
            _userPressedStop=false; //user needs to press 2x to stop the find videos process, then process videos process.
            return;
        }
        const QFile file(iter.next());
        const QString filename = file.fileName();

        bool duplicate = false;                 //don't want duplicates of same file
        for(const auto &alreadyAddedFile : _everyVideo)
            if(filename.toLower() == alreadyAddedFile.toLower())
            {
                duplicate = true;
                break;
            }
        if(!duplicate)
            _everyVideo << filename;

        ui->statusBar->showMessage(QStringLiteral("Found %1 videos | %2")
                                   .arg(_everyVideo.size())
                                   .arg(QDir::toNativeSeparators(filename)), 10);
        QApplication::processEvents();
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

    QMap<int64_t, int>::const_iterator mappedVidSize = mappedVideos.constBegin(); //video size, then video index
    while (mappedVidSize != mappedVideos.constEnd()) { // iterating from smaller size to bigger sizes
        sortedVideosList.insert(0, _videoList[mappedVidSize.value()]);
        mappedVidSize++;
    }

    if(sortedVideosList.size()!=_videoList.size())
        qDebug() << "Problem sorting through videos, sorted size=" << sortedVideosList.size() << ", but unsorted size=" << _videoList.size();

    return sortedVideosList;
}

void MainWindow::processVideos()
{
    _prefs._numberOfVideos = _everyVideo.count();
    ui->statusBox->append(QStringLiteral("\nFound %1 video file(s):\n").arg(_prefs._numberOfVideos));
    if(_prefs._numberOfVideos > 0)
    {
        ui->selectThumbnails->setDisabled(true);
        ui->processedFiles->setVisible(true);
        ui->processedFiles->setText(QStringLiteral("0/%1").arg(_prefs._numberOfVideos));
        if(ui->statusBar->currentMessage().indexOf(QStringLiteral("Cannot find folder")) == -1)
            ui->statusBar->setVisible(false);
        ui->progressBar->setVisible(true);
        ui->progressBar->setValue(0);
        ui->progressBar->setMaximum(_prefs._numberOfVideos);
        ui->statusBox->verticalScrollBar()->triggerAction(QScrollBar::SliderToMaximum);
    }
    else return;

    QThreadPool threadPool;
    for(const auto &filename : _everyVideo)
    {
        if(_userPressedStop)
        {
            threadPool.clear();
            break;
        }
        while(threadPool.activeThreadCount() == threadPool.maxThreadCount())
//        while(threadPool.activeThreadCount() == 1) // useful to debug manually, where threading causes debug logs confusion !
            QApplication::processEvents();          //avoid blocking signals in event loop

        auto *videoTask = new Video(_prefs, filename);
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
    ui->statusBox->repaint();
#ifdef VID_SIMILI_IN_TESTS
    qDebug() << message;
#endif
}

void MainWindow::addVideo(Video *addMe)
{
    if(ui->verboseCheckbox->isChecked()){
        addStatusMessage(QStringLiteral("[%1] %2").arg(QTime::currentTime().toString(),
                                                       QDir::toNativeSeparators(addMe->filename)));
    }
    ui->progressBar->setValue(ui->progressBar->value() + 1);
    ui->processedFiles->setText(QStringLiteral("%1/%2").arg(ui->progressBar->value()).arg(ui->progressBar->maximum()));
    _videoList << addMe;
}

void MainWindow::removeVideo(Video *deleteMe, QString errorMsg)
{
    addStatusMessage(QStringLiteral("[%1] ERROR with %2 : %3")
                     .arg(QTime::currentTime().toString())
                     .arg(QDir::toNativeSeparators(deleteMe->filename))
                     .arg(errorMsg));
    ui->progressBar->setValue(ui->progressBar->value() + 1);
    ui->processedFiles->setText(QStringLiteral("%1/%2").arg(ui->progressBar->value()).arg(ui->progressBar->maximum()));
    _rejectedVideos << QDir::toNativeSeparators(deleteMe->filename);
    delete deleteMe;
}

void MainWindow::on_actionAbout_triggered()
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
    QLabel *label = new QLabel(ui_credits);
    label->setWordWrap(true);
    label->setText(credits);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(label);

    ui_credits->setLayout(layout);
    ui_credits->show();
}
