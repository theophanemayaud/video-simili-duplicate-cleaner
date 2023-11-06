#include "comparison.h"

#include "ui_comparison.h" // WARNING : don't include this in the header file, otherwise includes from other files will be broken

enum FILENAME_CONTAINED_WITHIN_ANOTHER : int
{
    NOT_CONTAINED,
    LEFT_CONTAINS_RIGHT,
    RIGHT_CONTAINS_LEFT
};

const QString TEXT_STYLE_ORANGE = QStringLiteral("QLabel { color : peru; }");

const int64_t FILE_SIZE_BYTES_DIFF_STILL_EQUALS = 100*1024;
const int64_t VIDEO_DURATION_STILL_EQUALS_MS = 1000; //if this close in duration then it's considered equal
const int BITRATE_DIFF_STILL_EQUAL_kbs = 5;

Comparison::Comparison(const QVector<Video *> &videosParam, Prefs &prefsParam) :
    QDialog(prefsParam._mainwPtr, Qt::Window), ui(new Ui::Comparison), _videos(videosParam), _prefs(prefsParam)
{
    ui->setupUi(this);

    connect(this, SIGNAL(sendStatusMessage(const QString &)), _prefs._mainwPtr, SLOT(addStatusMessage(const QString &)));
    connect(this, SIGNAL(switchComparisonMode(const int &)),  _prefs._mainwPtr, SLOT(setComparisonMode(const int &)));
    connect(this, SIGNAL(adjustThresholdSlider(const int &)), _prefs._mainwPtr, SLOT(on_thresholdSlider_valueChanged(const int &)));
    connect(this, SIGNAL(adjustThresholdSlider(const int &)), ui->percentSim, SLOT(setNum(const int &)));
    connect(ui->progressBar, SIGNAL(valueChanged(const int &)), ui->currentVideo, SLOT(setNum(const int &)));

    if(_prefs._comparisonMode == _prefs._SSIM)
        ui->selectSSIM->setChecked(true);
    ui->thresholdSlider->setValue(QVariant(_prefs._thresholdSSIM * 100).toInt());
    ui->percentSim->setNum(QVariant(_prefs._thresholdSSIM * 100).toInt());
    ui->progressBar->setMaximum(_prefs._numberOfVideos * (_prefs._numberOfVideos - 1) / 2);

    ui->trashedFiles->setVisible(false); // hide until at least one file is deleted
    ui->totalVideos->setNum(int(_videos.size() * (_videos.size() - 1) / 2)); // all possible combinations

    // hide as not implemented yet
    // Auto trash based on folder settings
    ui->label_folderSettingsChoice->setVisible(false);
    ui->label_folderSettingsChoice_Description->setVisible(false);
    ui->pushButton_folderSettingsChoiceAutoTrash->setVisible(false);
    // Settings for important folders
    ui->label_importantFolders->setVisible(false);
    ui->label_importantFoldersDescript->setVisible(false);
    ui->importantFoldersListWidget->setVisible(false);
    ui->pushButton_importantFoldersAdd->setVisible(false);

    // important and locked folders list stuff
    ui->pushButton_importantFoldersAdd->setIcon(ui->pushButton_importantFoldersAdd->style()->standardIcon(QStyle::SP_DirOpenIcon));
    ui->lockedFolderButton->setIcon(ui->lockedFolderButton->style()->standardIcon(QStyle::SP_DirOpenIcon));
    connect(ui->importantFoldersListWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showImportantFolderContextMenu(QPoint)));
    connect(ui->lockedFolderslistWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showLockedFolderContextMenu(QPoint)));
    // (pressing DEL activates the slots only when list widget has focus)
    QShortcut* importantFoldersShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), ui->importantFoldersListWidget); // doesn't seem to work...
    connect(importantFoldersShortcut, SIGNAL(activated()), this, SLOT(eraseImportantFolderItem()));
    QShortcut* lockedFoldersShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), ui->importantFoldersListWidget);
    connect(lockedFoldersShortcut, SIGNAL(activated()), this, SLOT(eraseLockedFolderItem()));

    //delete right, left, and go right, left shortcuts
    QShortcut* rightDelShortcut = new QShortcut(QKeySequence(QKeySequence::MoveToNextChar), ui->tabManual);
    connect(rightDelShortcut, SIGNAL(activated()), this, SLOT(on_rightDelete_clicked()));
    QShortcut* leftDelShortcut = new QShortcut(QKeySequence(QKeySequence::MoveToPreviousChar), ui->tabManual);
    connect(leftDelShortcut, SIGNAL(activated()), this, SLOT(on_leftDelete_clicked()));

    QShortcut* downShortcut = new QShortcut(QKeySequence(QKeySequence::MoveToNextLine), ui->tabManual);
    connect(downShortcut, SIGNAL(activated()), this, SLOT(on_nextVideo_clicked()));
    QShortcut* upShortcut = new QShortcut(QKeySequence(QKeySequence::MoveToPreviousLine), ui->tabManual);
    connect(upShortcut, SIGNAL(activated()), this, SLOT(on_prevVideo_clicked()));

    on_nextVideo_clicked();
}

Comparison::~Comparison()
{
    delete ui;
}

int Comparison::reportMatchingVideos()
{
    int64_t combinedFilesize = 0;
    int foundMatches = 0;

    QVector<Video*>::const_iterator left, right, end = _videos.cend();
    for(left=_videos.cbegin(); left<end; left++)
        for(right=left+1; right<end; right++)
            if(bothVideosMatch(*left, *right))
            {   //smaller of two matching videos is likely the one to be deleted
                combinedFilesize += std::min((*left)->size , (*right)->size);
                foundMatches++;
                break;
            }

    if(foundMatches)
        emit sendStatusMessage(QStringLiteral("\n[%1] Found %2 video(s) (%3) with one or more matches")
             .arg(QTime::currentTime().toString()).arg(foundMatches).arg(readableFileSize(combinedFilesize)));

    return foundMatches;
}

void Comparison::confirmToExit()
{
    int confirm = QMessageBox::Yes;
    if(!ui->leftFileName->text().isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setWindowTitle(QStringLiteral("Out of videos to compare"));
        msgBox.setText(QStringLiteral("Close window?                  "));
        msgBox.setIcon(QMessageBox::QMessageBox::Question);
        msgBox.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        confirm = msgBox.exec();
    }
    if(confirm == QMessageBox::Yes)
    {
        if(_someWereMovedInApplePhotosLibrary)
            displayApplePhotosAlbumDeletionMessage();
        if(_videosDeleted)
            emit sendStatusMessage(QStringLiteral("\n%1 file(s) removed, %2 freed")
                                   .arg(_videosDeleted).arg(readableFileSize(_spaceSaved)));
        if(!ui->leftFileName->text().isEmpty())
            emit sendStatusMessage(QStringLiteral("\nPressing Find duplicates button opens comparison window "
                                                 "again if thumbnail mode and directories remain the same"));
        else
            emit sendStatusMessage(QStringLiteral("\nComparison window closed because no matching videos found "
                                                 "(a lower threshold may help to find more matches)"));

        QKeyEvent *closeEvent = new QKeyEvent(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::postEvent(this, closeEvent);  //"pressing" ESC closes dialog
    }
}

void Comparison::on_prevVideo_clicked()
{
    _seekForwards = false;
    QVector<Video*>::const_iterator left, right, begin = _videos.cbegin();
    for(_rightVideo--, left=begin+_leftVideo; left>=begin; left--, _leftVideo--)
    {
        for(right=begin+_rightVideo; right>left; right--, _rightVideo--)
            if(bothVideosMatch(*left, *right)
                    && QFileInfo::exists((*left)->_filePathName) && !(*left)->trashed // check trashed in case it is from Apple Photos
                    && QFileInfo::exists((*right)->_filePathName) && !(*right)->trashed
                    && (ui->settingNamesInAnotherCheckbox->isChecked()==false
                        || whichFilenameContainsTheOther((*left)->_filePathName, (*right)->_filePathName) != NOT_CONTAINED ) // check wether name in another is enabled
                    )
            {
                showVideo(QStringLiteral("left"));
                showVideo(QStringLiteral("right"));
                highlightBetterProperties();
                updateUI();
                return;
            }
        ui->progressBar->setValue(comparisonsSoFar());
        _rightVideo = _prefs._numberOfVideos - 1;
    }

    on_nextVideo_clicked();     //went over limit, go forwards until first match
}

void Comparison::on_nextVideo_clicked()
{
    _seekForwards = true;
    const int oldLeft = _leftVideo;
    const int oldRight = _rightVideo;

    QVector<Video*>::const_iterator left, right, begin = _videos.cbegin(), end = _videos.cend();
    for(left=begin+_leftVideo; left<end; left++, _leftVideo++)
    {
        for(_rightVideo++, right=begin+_rightVideo; right<end; right++, _rightVideo++)
            if(bothVideosMatch(*left, *right)
                    && QFileInfo::exists((*left)->_filePathName) && !(*left)->trashed // check trashed in case it is from Apple Photos
                    && QFileInfo::exists((*right)->_filePathName) && !(*right)->trashed
                    && (ui->settingNamesInAnotherCheckbox->isChecked()==false
                        || whichFilenameContainsTheOther((*left)->_filePathName, (*right)->_filePathName) != NOT_CONTAINED ) // check wether name in another is enabled
                    )
            {
                showVideo(QStringLiteral("left"));
                showVideo(QStringLiteral("right"));
                highlightBetterProperties();
                updateUI();
                return;
            }
        ui->progressBar->setValue(comparisonsSoFar());
        _rightVideo = _leftVideo + 1;
    }

    _leftVideo = oldLeft;       //went over limit, go to last matching pair
    _rightVideo = oldRight;
    confirmToExit();
}

bool Comparison::bothVideosMatch(const Video *left, const Video *right)
{
    bool theyMatch = false;
    if(left==nullptr || right==nullptr){
        qCritical() << Q_FUNC_INFO << ": left or right video for comparison was null";
        return theyMatch;
    }

    // check if pair is flagged as not dupplicate in DB
    if(Db(_prefs.cacheFilePathName).isPairToIgnore(left->_filePathName, right->_filePathName))
        return false;

    _phashSimilarity = 0;

    const int hashes = _prefs._thumbnails == cutEnds? 2 : 1;
    for(int hash=0; hash<hashes; hash++)
    {                               //if cutEnds mode: similarity is always the best one of both comparisons
        _phashSimilarity = qMax( _phashSimilarity, phashSimilarity(left, right, hash));
        if(_prefs._comparisonMode == _prefs._PHASH)
        {
            if(_phashSimilarity >= _prefs._thresholdPhash)
                theyMatch = true;
        }                           //ssim comparison is slow, only do it if pHash differs at most 20 bits of 64
        else if(_phashSimilarity >= qMax(_prefs._thresholdPhash, 44))
        {
            _ssimSimilarity = ssim(left->grayThumb[hash], right->grayThumb[hash], _prefs._ssimBlockSize);
            _ssimSimilarity = _ssimSimilarity + _durationModifier / 64.0;   // b/64 bits (phash) <=> p/100 % (ssim)
            if(_ssimSimilarity > _prefs._thresholdSSIM)
                theyMatch = true;
        }
        if(theyMatch)               //if cutEnds mode: first comparison matched already, skip second
            break;
    }
    return theyMatch;
}

int Comparison::phashSimilarity(const Video *left, const Video *right, const int &nthHash)
{
    if(left->hash[nthHash] == 0 && right->hash[nthHash] == 0)
        return 0;

    int distance = 64;
    uint64_t differentBits = left->hash[nthHash] ^ right->hash[nthHash];    //XOR to value (only ones for differing bits)
    while(differentBits)
    {
        differentBits &= differentBits - 1;                 //count number of bits of value
        distance--;
    }

    if( qAbs(left->duration - right->duration) <= 1000 )
        _durationModifier = 0 + _prefs._sameDurationModifier;               //lower distance if both durations within 1s
    else
        _durationModifier = 0 - _prefs._differentDurationModifier;          //raise distance if both durations differ 1s

    distance = distance + _durationModifier;
    return distance > 64? 64 : distance;
}

void Comparison::showVideo(const QString &side)
{
    int thisVideo = _leftVideo;
    if(side == "right")
        thisVideo = _rightVideo;

    auto *Image = this->findChild<ClickableLabel *>(side + QStringLiteral("Image"));
    QBuffer pixels(&_videos[thisVideo]->thumbnail);
    QImage image;
    image.load(&pixels, QByteArrayLiteral("JPG"));
    Image->setPixmap(QPixmap::fromImage(image).scaled(Image->width(), Image->height(), Qt::KeepAspectRatio));

#ifdef Q_OS_MACOS
    // Get video name from apple photos if applicable. Can't do in in video directly as it is very slow
    if(_videos[thisVideo]->_filePathName.contains(".photoslibrary")){
        const QString fileNameNoExt = QFileInfo(_videos[thisVideo]->_filePathName).completeBaseName();
        if (!fileNameNoExt.contains("_")){
            QString resultString = QString::fromLocal8Bit(Obj_C::obj_C_getMediaName(fileNameNoExt.toLocal8Bit().data()));
            if(!resultString.contains(OBJ_C_FAILURE_STRING)){
                _videos[thisVideo]->nameInApplePhotos = resultString;
            }
            else{
                emit sendStatusMessage(QString("Unknown error getting name of %1 from Apple Photos Library. "
                                               "If you have multiple libraries this might be normal, "
                                               "it will only work, only with the currently open library.")
                                       .arg(_videos[thisVideo]->_filePathName));
            }
        }
    }
#endif

    auto *FileName = this->findChild<ClickableLabel *>(side + QStringLiteral("FileName"));
    if(_videos[thisVideo]->nameInApplePhotos.isEmpty())
        FileName->setText(QFileInfo(_videos[thisVideo]->_filePathName).fileName());
    else
        FileName->setText(_videos[thisVideo]->nameInApplePhotos);
    FileName->setToolTip(QStringLiteral("%1\nOpen in file manager")
                                        .arg(QDir::toNativeSeparators(_videos[thisVideo]->_filePathName)));

    QFileInfo videoFile(_videos[thisVideo]->_filePathName);
    auto *PathName = this->findChild<QLabel *>(side + QStringLiteral("PathName"));
    PathName->setText(QDir::toNativeSeparators(videoFile.absolutePath()));

    auto *FileSize = this->findChild<QLabel *>(side + QStringLiteral("FileSize"));
    FileSize->setText(readableFileSize(_videos[thisVideo]->size));

    auto *Duration = this->findChild<QLabel *>(side + QStringLiteral("Duration"));
    Duration->setText(readableDuration(_videos[thisVideo]->duration));

    auto *Modified = this->findChild<QLabel *>(side + QStringLiteral("Modified"));
    Modified->setText(_videos[thisVideo]->modified.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));

    // File create date
    if(side=="left"){
        this->ui->leftFileCreated->setText(_videos[thisVideo]->_fileCreateDate.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));
    }
    else{
        this->ui->rightFileCreated->setText(_videos[thisVideo]->_fileCreateDate.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));
    }

    const QString resolutionString = QStringLiteral("%1x%2").
                  arg(_videos[thisVideo]->width).arg(_videos[thisVideo]->height);
    auto *Resolution = this->findChild<QLabel *>(side + QStringLiteral("Resolution"));
    Resolution->setText(resolutionString);

    auto *FrameRate = this->findChild<QLabel *>(side + QStringLiteral("FrameRate"));
    const double fps = _videos[thisVideo]->framerate;
    if(fps == 0.0)
        FrameRate->clear();
    else
        FrameRate->setText(QStringLiteral("%1 FPS").arg(fps));

    auto *BitRate = this->findChild<QLabel *>(side + QStringLiteral("BitRate"));
    BitRate->setText(readableBitRate(_videos[thisVideo]->bitrate));

    auto *Codec = this->findChild<QLabel *>(side + QStringLiteral("Codec"));
    Codec->setText(_videos[thisVideo]->codec);

    auto *Audio = this->findChild<QLabel *>(side + QStringLiteral("Audio"));
    Audio->setText(_videos[thisVideo]->audio);
}

QString Comparison::readableDuration(const int64_t &milliseconds) const
{
    if(milliseconds == 0)
        return QStringLiteral("");

    const int hours   = milliseconds / (1000*60*60) % 24;
    const int minutes = milliseconds / (1000*60) % 60;
    const int seconds = milliseconds / 1000 % 60;

    QString readableDuration;
    if(hours > 0)
        readableDuration = QStringLiteral("%1h").arg(hours);
    if(minutes > 0)
        readableDuration = QStringLiteral("%1%2m").arg(readableDuration).arg(minutes);
    if(seconds > 0)
        readableDuration = QStringLiteral("%1%2s").arg(readableDuration).arg(seconds);

    return readableDuration;
}

QString Comparison::readableFileSize(const int64_t &filesize) const
{
    //FileSizes are in bytes
    if(filesize < 1024 * 1024)
        return(QStringLiteral("%1 kB").arg(QString::number(filesize / 1024.0, 'i', 0))); //even kBs
    else if(filesize < 1024 * 1024 * 1024)                          //larger files have one decimal point
        return QStringLiteral("%1 MB").arg(QString::number(filesize / (1024.0 * 1024.0), 'f', 1));
    else
        return QStringLiteral("%1 GB").arg(QString::number(filesize / (1024.0 * 1024.0 * 1024.0), 'f', 1));
}

QString Comparison::readableBitRate(const double &kbps) const
{
    if(kbps == 0.0)
        return QStringLiteral("");
    return QStringLiteral("%1 kb/s").arg(kbps);
}

void Comparison::highlightBetterProperties() const
{
    ui->leftFileSize->setStyleSheet(QStringLiteral(""));
    ui->rightFileSize->setStyleSheet(QStringLiteral(""));       //both filesizes within 100 kB
    if(qAbs(_videos[_leftVideo]->size - _videos[_rightVideo]->size) <= FILE_SIZE_BYTES_DIFF_STILL_EQUALS)
    {
        ui->leftFileSize->setStyleSheet(QStringLiteral("QLabel { color : peru; }"));
        ui->rightFileSize->setStyleSheet(QStringLiteral("QLabel { color : peru; }"));
    }
    else if(_videos[_leftVideo]->size > _videos[_rightVideo]->size)
        ui->leftFileSize->setStyleSheet(QStringLiteral("QLabel { color : green; }"));
    else if(_videos[_leftVideo]->size < _videos[_rightVideo]->size)
        ui->rightFileSize->setStyleSheet(QStringLiteral("QLabel { color : green; }"));

    ui->leftDuration->setStyleSheet(QStringLiteral(""));
    ui->rightDuration->setStyleSheet(QStringLiteral(""));       //both runtimes within 1 second
    if(qAbs(_videos[_leftVideo]->duration - _videos[_rightVideo]->duration) <= VIDEO_DURATION_STILL_EQUALS_MS)
    {
        ui->leftDuration->setStyleSheet(QStringLiteral("QLabel { color : peru; }"));
        ui->rightDuration->setStyleSheet(QStringLiteral("QLabel { color : peru; }"));
    }
    else if(_videos[_leftVideo]->duration > _videos[_rightVideo]->duration)
        ui->leftDuration->setStyleSheet(QStringLiteral("QLabel { color : green; }"));
    else if(_videos[_leftVideo]->duration < _videos[_rightVideo]->duration)
        ui->rightDuration->setStyleSheet(QStringLiteral("QLabel { color : green; }"));

    ui->leftBitRate->setStyleSheet(QStringLiteral(""));
    ui->rightBitRate->setStyleSheet(QStringLiteral(""));
    if(qAbs(_videos[_leftVideo]->bitrate - _videos[_rightVideo]->bitrate)<=BITRATE_DIFF_STILL_EQUAL_kbs) //leave some margin due to decoding error
    {
        ui->leftBitRate->setStyleSheet(QStringLiteral("QLabel { color : peru; }"));
        ui->rightBitRate->setStyleSheet(QStringLiteral("QLabel { color : peru; }"));
    }
    else if(_videos[_leftVideo]->bitrate > _videos[_rightVideo]->bitrate)
        ui->leftBitRate->setStyleSheet(QStringLiteral("QLabel { color : green; }"));
    else if(_videos[_leftVideo]->bitrate < _videos[_rightVideo]->bitrate)
        ui->rightBitRate->setStyleSheet(QStringLiteral("QLabel { color : green; }"));

    ui->leftFrameRate->setStyleSheet(QStringLiteral(""));
    ui->rightFrameRate->setStyleSheet(QStringLiteral(""));      //both framerates within 0.1 fps
    if(qAbs(_videos[_leftVideo]->framerate - _videos[_rightVideo]->framerate) <= 0.1)
    {
        ui->leftFrameRate->setStyleSheet(QStringLiteral("QLabel { color : peru; }"));
        ui->rightFrameRate->setStyleSheet(QStringLiteral("QLabel { color : peru; }"));
    }
    else if(_videos[_leftVideo]->framerate > _videos[_rightVideo]->framerate)
        ui->leftFrameRate->setStyleSheet(QStringLiteral("QLabel { color : green; }"));
    else if(_videos[_leftVideo]->framerate < _videos[_rightVideo]->framerate)
        ui->rightFrameRate->setStyleSheet(QStringLiteral("QLabel { color : green; }"));

    // Set file modified date
    ui->leftModified->setStyleSheet(QStringLiteral(""));
    ui->rightModified->setStyleSheet(QStringLiteral(""));
    if(_videos[_leftVideo]->modified == _videos[_rightVideo]->modified)
    {
        ui->leftModified->setStyleSheet(QStringLiteral("QLabel { color : peru; }"));
        ui->rightModified->setStyleSheet(QStringLiteral("QLabel { color : peru; }"));
    }
    else if(_videos[_leftVideo]->modified < _videos[_rightVideo]->modified)
        ui->leftModified->setStyleSheet(QStringLiteral("QLabel { color : green; }"));
    else if(_videos[_leftVideo]->modified > _videos[_rightVideo]->modified)
        ui->rightModified->setStyleSheet(QStringLiteral("QLabel { color : green; }"));

    // Set file create date (earlier is better, ie green)
    ui->leftFileCreated->setStyleSheet(QStringLiteral(""));
    ui->rightFileCreated->setStyleSheet(QStringLiteral(""));
    if(_videos[_leftVideo]->_fileCreateDate == _videos[_rightVideo]->_fileCreateDate)
    {
        ui->leftFileCreated->setStyleSheet(QStringLiteral("QLabel { color : peru; }"));
        ui->rightFileCreated->setStyleSheet(QStringLiteral("QLabel { color : peru; }"));
    }
    else if(_videos[_leftVideo]->_fileCreateDate < _videos[_rightVideo]->_fileCreateDate)
        ui->leftFileCreated->setStyleSheet(QStringLiteral("QLabel { color : green; }"));
    else if(_videos[_leftVideo]->_fileCreateDate > _videos[_rightVideo]->_fileCreateDate)
        ui->rightFileCreated->setStyleSheet(QStringLiteral("QLabel { color : green; }"));

    // Set resolution
    ui->leftResolution->setStyleSheet(QStringLiteral(""));
    ui->rightResolution->setStyleSheet(QStringLiteral(""));

    if(_videos[_leftVideo]->width * _videos[_leftVideo]->height ==
       _videos[_rightVideo]->width * _videos[_rightVideo]->height)
    {
        ui->leftResolution->setStyleSheet(QStringLiteral("QLabel { color : peru; }"));
        ui->rightResolution->setStyleSheet(QStringLiteral("QLabel { color : peru; }"));
    }
    else if(_videos[_leftVideo]->width * _videos[_leftVideo]->height >
       _videos[_rightVideo]->width * _videos[_rightVideo]->height)
        ui->leftResolution->setStyleSheet(QStringLiteral("QLabel { color : green; }"));
    else if(_videos[_leftVideo]->width * _videos[_leftVideo]->height <
            _videos[_rightVideo]->width * _videos[_rightVideo]->height)
        ui->rightResolution->setStyleSheet(QStringLiteral("QLabel { color : green; }"));

    // show if video codecs are the same
    ui->leftCodec->setStyleSheet("");
    ui->rightCodec->setStyleSheet("");
    if(_videos[_leftVideo]->codec.localeAwareCompare(_videos[_rightVideo]->codec)==0){
        ui->leftCodec->setStyleSheet(TEXT_STYLE_ORANGE);
        ui->rightCodec->setStyleSheet(TEXT_STYLE_ORANGE);
    }

    // show if audio codecs are the same
    ui->leftAudio->setStyleSheet("");
    ui->rightAudio->setStyleSheet("");
    if(_videos[_leftVideo]->audio.localeAwareCompare(_videos[_rightVideo]->audio)==0){
        ui->leftAudio->setStyleSheet(TEXT_STYLE_ORANGE);
        ui->rightAudio->setStyleSheet(TEXT_STYLE_ORANGE);
    }
}

void Comparison::updateUI()
{
    if(ui->leftPathName->text() == ui->rightPathName->text())    //gray out move button if both videos in same folder
    {
        ui->leftMove->setDisabled(true);
        ui->rightMove->setDisabled(true);
    }
    else
    {
        ui->leftMove->setDisabled(false);
        ui->rightMove->setDisabled(false);
    }

    if(_prefs._comparisonMode == _prefs._PHASH)
        ui->identicalBits->setText(QString("%1/64 same bits").arg(_phashSimilarity));
    if(_prefs._comparisonMode == _prefs._SSIM)
        ui->identicalBits->setText(QString("%1 SSIM index").arg(QString::number(qMin(_ssimSimilarity, 1.0), 'f', 3)));
    _zoomLevel = 0;
    ui->progressBar->setValue(comparisonsSoFar());
}

int Comparison::comparisonsSoFar() const
{
    const int cmpFirst = _prefs._numberOfVideos;                    //comparisons done for first video
    const int cmpThis = cmpFirst - _leftVideo;                      //comparisons done for current video
    const int remaining = cmpThis * (cmpThis - 1) / 2;              //comparisons for remaining videos
    const int maxComparisons = cmpFirst * (cmpFirst - 1) / 2;       //comparing all videos with each other
    return maxComparisons - remaining + _rightVideo - _leftVideo;
}

void Comparison::openFileManager(const QString &filename)
{
#ifdef Q_OS_WIN
    // TODO : for UWP, can't use process, so maybe change behavior to open folder, without file already selected :
    // https://stackoverflow.com/questions/48243245/qdesktopservicesopenurl-cannot-open-directory-in-mac-finder
    QProcess::startDetached("explorer", {"/select,", QDir::toNativeSeparators(filename)});
#elif defined(Q_OS_MACOS)
    if(!filename.contains(".photoslibrary")){
        QProcess::startDetached("open", QStringList() << "-R" << filename);
    }
    else{
        const QString fileNameNoExt = QFileInfo(filename).completeBaseName();
        QString returnValue = QString::fromLocal8Bit(
                    Obj_C::obj_C_revealMediaInPhotosApp(fileNameNoExt.toLocal8Bit().data())
                    );
        if(!returnValue.contains(OBJ_C_SUCCESS_STRING)){
            QMessageBox::information(this, "", QString(
                    "Unknown error revealing in Apple Photos Library album, sorry. "
                    "\nInstead, will open in file manager. "
                    "\n\nError:%1").arg(returnValue));
            QProcess::startDetached("open", QStringList() << "-R" << filename);
        }

    }
#elif defined(Q_OS_X11)
        QProcess::startDetached(QStringLiteral("xdg-open \"%1\"").arg(filename.left(filename.lastIndexOf("/"))));
#endif
}

void Comparison::openMedia(const QString filename) {
#ifdef Q_OS_MACOS
    if(!filename.contains(".photoslibrary")){
#endif
        QDesktopServices::openUrl(QUrl::fromLocalFile(filename));
#ifdef Q_OS_MACOS
    }
    else{
        const QString fileNameNoExt = QFileInfo(filename).completeBaseName();
        QString returnValue = QString::fromLocal8Bit(
                    Obj_C::obj_C_revealMediaInPhotosApp(fileNameNoExt.toLocal8Bit().data())
                    );
        if(!returnValue.contains(OBJ_C_SUCCESS_STRING)){
            QMessageBox::information(this, "", QString(
                    "Unknown error revealing in Apple Photos Library album, sorry. "
                    "\nInstead, will open in default player manager. "
                    "\n\nError:%1").arg(returnValue));
            QDesktopServices::openUrl(QUrl::fromLocalFile(filename));
        }

    }
#endif
}

void Comparison::deleteVideo(const int &side, const bool auto_trash_mode)
{
    const QString filename = _videos[side]->_filePathName;
    const QString onlyFilename = filename.right(filename.length() - filename.lastIndexOf("/") - 1);

    // find if it is the elft or right video in ui to tell used in trash confirmation
    QString videoSide = "left";
    if(side == _rightVideo)
        videoSide = "right";

    if(_videos[side]->trashed || !QFileInfo::exists(filename))  //video was already manually deleted, skip to next
    {
        _seekForwards? on_nextVideo_clicked() : on_prevVideo_clicked();
        return;
    }
    QString question;
    switch (_prefs.delMode) {
    case Prefs::STANDARD_TRASH:
        question = QString("Are you sure you want to move the %1 file to trash?\n\n%2")
                .arg(videoSide) //show if it is the left or right file
                .arg(_videos[side]->nameInApplePhotos.isEmpty()? onlyFilename : _videos[side]->nameInApplePhotos);
        break;
    case Prefs::CUSTOM_TRASH:
        question = QString("Are you sure you want to move the %1 file to the selected folder?\n\n%2")
                            .arg(videoSide) //show if it is the left or right file
                            .arg(onlyFilename);
        break;
    case Prefs::DIRECT_DELETION:
        question = QString("Are you sure you want to delete the %1 file ?\n\n%2")
                            .arg(videoSide) //show if it is the left or right file
                            .arg(onlyFilename);
        break;
    default:
        break;
    }
    if(ui->disableDeleteConfirmationCheckbox->isChecked() ||
            QMessageBox::question(this, "Delete file",
                                  question,
                                  QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
    {
        // check if file is in locked folder set by user
        if(isFileInProtectedFolder(filename)){
            if(!auto_trash_mode)
                QMessageBox::information(this, "", "This file is locked, cannot delete !");
            else{
                emit sendStatusMessage(QString("Skipped %1 as it is locked.").arg(QDir::toNativeSeparators(filename)));
            }
            // no need to seek as in auto trash mode, the seeking is already handled, and manual will not want to seek
            return;
        }
#ifdef Q_OS_MACOS
        else if(filename.contains(".photoslibrary")){ // we must never delete files from the Apple Photos Library, although we can detect them !
            if(!filename.contains(".photoslibrary/originals/")){
                if(!auto_trash_mode)
                    QMessageBox::information(this, "", QString(
                            "The file is a derivative media created by Apple Photos. "
                            "It shouldn't have been detected in the first place, sorry. "
                            "It must and will not be deleted."));
                emit sendStatusMessage(QString("Error, file %1 was an Apple Photos Library derivative not an original.").arg(QDir::toNativeSeparators(filename)));
                return;
            }
            else{ // only video in subfolder originals are true videos
                // We'll now tell Apple Photos via AppleScript to add videos to be deleted to a specific album so the user can manually delete them all at once
                const QString fileNameNoExt = QFileInfo(filename).completeBaseName();
                if(fileNameNoExt.contains("_")) { // TODO : if contains _ then video is probably a live photo media, so should not modify it ! -> should preferably discard at scan time... ?
                    if(!auto_trash_mode)
                        QMessageBox::information(this, "", "This video is in an Apple Photos Libray, and seems to be from a Live Photo, not a real video. \n"
                                                            "You should use duplicate photo scanners to deal with it.");
                    emit sendStatusMessage(QString("Did not add %1 into Apple Photos Library album : it seems to be a live photo, so deal with it as a photo.").arg(QDir::toNativeSeparators(filename)));
                    return;
                }
                else { // only videos that aren't live photos
                    QString returnValue = QString::fromLocal8Bit(
                                Obj_C::obj_C_addMediaToAlbum(QString(APP_NAME).toLocal8Bit().data(),
                                                          fileNameNoExt.toLocal8Bit().data()));
                    if(!returnValue.contains(OBJ_C_SUCCESS_STRING)){
                        if(!auto_trash_mode)
                            QMessageBox::information(this, "", QString(
                                    "Unknown error adding into Apple Photos Library album, sorry. "
                                    "Video might be in Apple Photos trash. "
                                    "Make sure to empty Apple Photos trash."
                                    "\n\nError:%1").arg(returnValue));
                        emit sendStatusMessage(QString("Unknown error adding %1 into Apple Photos Library album.").arg(QDir::toNativeSeparators(filename)));
                    }
                    // Finally if reached here: it is "deleted" so remove from DB
                    // NB : we only delete the file from the disk, and not from _videos, as we check
                    //      when going to the next/prev video that each exists, or skip it.
                    _someWereMovedInApplePhotosLibrary = true; // used to check at the very end, to display reminder message to user
                    _videos[side]->trashed = true; // could check simply if file still exists on disk but not in case of Apple Photos...
                    _videosDeleted++;
                    _spaceSaved = _spaceSaved + _videos[side]->size;

                    ui->trashedFiles->setVisible(true);
                    ui->trashedFiles->setText(QStringLiteral("Moved %1 to trash").arg(_videosDeleted));
                    emit sendStatusMessage(QString("Moved %1 to album 'Trash from %2' of Apple Photos Library")
                                           .arg(QDir::toNativeSeparators(filename), APP_NAME));

                    Db(_prefs.cacheFilePathName).removeVideo(filename); // remove it from the cache as it is not needed anymore !
                    if(!auto_trash_mode) // in auto trash mode, the seeking is already handled
                        _seekForwards? on_nextVideo_clicked() : on_prevVideo_clicked();
                    return;
                }
            }
        }
#endif
        else{
            if(_prefs.delMode == Prefs::DIRECT_DELETION){
                if(!QFile::remove(filename)){
                    if(!auto_trash_mode)
                        QMessageBox::information(this, "", "Could not delete. Check file permissions");
                    else
                        emit sendStatusMessage(QString("Error deleting video %1").arg(QDir::toNativeSeparators(filename)));
                    return;
                }
            }
            else if(_prefs.delMode == Prefs::CUSTOM_TRASH){ // otherwise we move to the custom folder selected by user
                if(!_prefs.trashDir.exists()){
                    if(!auto_trash_mode)
                        QMessageBox::information(this, "", "Selected folder to move files into doesn't seem to exist");
                    else
                        emit sendStatusMessage(QString("Error moving to selected folder, it doesn't seem to exist for video %1").arg(QDir::toNativeSeparators(filename)));
                    return;
                }
                else { // the destination directory does exist
                    QFileInfo newFileInfo(_prefs.trashDir, QFileInfo(filename).fileName());
                    if(newFileInfo.exists()) // create random name to make sure it doesn't exist
                        newFileInfo.setFile(_prefs.trashDir, QFileInfo(filename).completeBaseName()+"-"+QUuid::createUuid().toString().remove("{").remove("}") + "." + QFileInfo(filename).suffix());
                    if(!QFile(filename).rename(newFileInfo.absoluteFilePath())){ // rename actually moves to new path !
                        if(!auto_trash_mode)
                            QMessageBox::information(this, "", "Could not move file to selected folder. Check file permissions.");
                        else
                            emit sendStatusMessage(QString("Error moving to selected folder, check file permissions of video %1").arg(QDir::toNativeSeparators(filename)));
                        return;
                    }
                }
            }
            else { // meaning _prefs.delMode==Prefs::STANDARD_TRASH
                if(!QFile::moveToTrash(filename)){
                    if(!auto_trash_mode)
                        QMessageBox::information(this, "", "Could not move file to trash. Check file permissions, "
                                                           "and if a trash exists in your file system "
                                                           "(eg network locations do not have a trash).\n\n"
                                                           "You could try again with direct deletion enabled, or "
                                                           "with a custom trash folder.");
                    else
                        emit sendStatusMessage(QString("Error moving to trash video %1").arg(QDir::toNativeSeparators(filename)));
                    return;
                }
            }

            // Reaches here if video was successfully handled (except apple photo case)
            // NB : we only delete the file from the disk, and not from _videos, as we check
            //      when going to the next/prev video that each exists, or skip it.
            _videos[side]->trashed = true; // could check simply if file still exists on disk but not in case of Apple Photos...
            _videosDeleted++;
            _spaceSaved = _spaceSaved + _videos[side]->size;
            ui->trashedFiles->setVisible(true);

            switch (_prefs.delMode) {
            case Prefs::STANDARD_TRASH:
                ui->trashedFiles->setText(QStringLiteral("Moved %1 to trash").arg(_videosDeleted));
                emit sendStatusMessage(QString("Moved %1 to trash").arg(QDir::toNativeSeparators(filename)));
                break;
            case Prefs::CUSTOM_TRASH:
                ui->trashedFiles->setText(QStringLiteral("Moved %1 to selected folder").arg(_videosDeleted));
                emit sendStatusMessage(QString("Moved %1 to selected folder").arg(QDir::toNativeSeparators(filename)));
                break;
            case Prefs::DIRECT_DELETION:
                ui->trashedFiles->setText(QStringLiteral("Deleted %1").arg(_videosDeleted));
                emit sendStatusMessage(QString("Deleted %1").arg(QDir::toNativeSeparators(filename)));
                break;
            default:
                break;
            }

            Db(_prefs.cacheFilePathName).removeVideo(filename); // remove it from the cache as it is not needed anymore !
            if(!auto_trash_mode) // in auto trash mode, the seeking is already handled
                _seekForwards? on_nextVideo_clicked() : on_prevVideo_clicked();
        }
    }
}

void Comparison::moveVideo(const QString &from, const QString &to)
{
#ifdef Q_OS_MACOS
    if(from.contains(".photoslibrary")){
        QMessageBox::information(this, "", "This file is in an Apple Photos Library, cannot move !");
        return;
    }
#endif
    if(QMessageBox::question(this, "Move", "This file is in a locked folder, are you sure you want to move it ?",
                             QMessageBox::Yes|QMessageBox::No) == QMessageBox::No)
        return;

    if(!QFileInfo::exists(from))
    {
        _seekForwards? on_nextVideo_clicked() : on_prevVideo_clicked();
        return;
    }

    const QString fromPath = from.left(from.lastIndexOf("/"));
    const QString toPath   = to.left(to.lastIndexOf("/"));
    const QString question = QString("Are you sure you want to move this file?\n\nFrom: %1\nTo:     %2")
                             .arg(QDir::toNativeSeparators(fromPath), QDir::toNativeSeparators(toPath));
    if(QMessageBox::question(this, "Move", question, QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes)
    {
        QFile moveThisFile(from);
        if(!moveThisFile.rename(QString("%1/%2").arg(toPath, from.right(from.length() - from.lastIndexOf("/") - 1))))
            QMessageBox::information(this, "", "Could not move file. Check file permissions and available disk space.");
        else
        {
            emit sendStatusMessage(QString("Moved %1 to %2").arg(QDir::toNativeSeparators(from), toPath));
            _seekForwards? on_nextVideo_clicked() : on_prevVideo_clicked();
        }
    }
}

void Comparison::on_swapFilenames_clicked() const
{
    const QFileInfo leftVideoFile(_videos[_leftVideo]->_filePathName);
    const QString leftPathname = leftVideoFile.absolutePath();
    const QString oldLeftFilename = leftVideoFile.fileName();
    const QString oldLeftNoExtension = oldLeftFilename.left(oldLeftFilename.lastIndexOf("."));
    const QString leftExtension = oldLeftFilename.right(oldLeftFilename.length() - oldLeftFilename.lastIndexOf("."));

    const QFileInfo rightVideoFile(_videos[_rightVideo]->_filePathName);
    const QString rightPathname = rightVideoFile.absolutePath();
    const QString oldRightFilename = rightVideoFile.fileName();
    const QString oldRightNoExtension = oldRightFilename.left(oldRightFilename.lastIndexOf("."));
    const QString rightExtension = oldRightFilename.right(oldRightFilename.length() - oldRightFilename.lastIndexOf("."));

    const QString newLeftFilename = QStringLiteral("%1%2").arg(oldRightNoExtension, leftExtension);
    const QString newLeftPathAndFilename = QStringLiteral("%1/%2").arg(leftPathname, newLeftFilename);

    const QString newRightFilename = QStringLiteral("%1%2").arg(oldLeftNoExtension, rightExtension);
    const QString newRightPathAndFilename = QStringLiteral("%1/%2").arg(rightPathname, newRightFilename);

    QFile leftFile(_videos[_leftVideo]->_filePathName);                  //rename files
    QFile rightFile(_videos[_rightVideo]->_filePathName);
    leftFile.rename(QStringLiteral("%1/DuplicateRenamedVideo.avi").arg(leftPathname));
    rightFile.rename(newRightPathAndFilename);
    leftFile.rename(newLeftPathAndFilename);

    _videos[_leftVideo]->_filePathName = newLeftPathAndFilename;         //update filename in object
    _videos[_rightVideo]->_filePathName = newRightPathAndFilename;

    ui->leftFileName->setText(newLeftFilename);                     //update UI
    ui->rightFileName->setText(newRightFilename);

    // remove both from cache, otherwise they will be stored in the cache inverted from their full path names
    // TODO : could just rename them in the cache... ?
    Db cache(_prefs.cacheFilePathName); // opening connexion to database
    cache.removeVideo(oldLeftFilename);
    cache.removeVideo(oldRightFilename);
}

void Comparison::on_thresholdSlider_valueChanged(const int &value)
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

    emit adjustThresholdSlider(ui->thresholdSlider->value());
}

void Comparison::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)

    if(ui->leftFileName->text().isEmpty() || _leftVideo >= _prefs._numberOfVideos || _rightVideo >= _prefs._numberOfVideos)
        return;     //automatic initial resize event can happen before closing when values went over limit

    QImage image;
    QBuffer leftPixels(&_videos[_leftVideo]->thumbnail);
    image.load(&leftPixels, QByteArrayLiteral("JPG"));
    ui->leftImage->setPixmap(QPixmap::fromImage(image).scaled(
                             ui->leftImage->width(), ui->leftImage->height(), Qt::KeepAspectRatio));
    QBuffer rightPixels(&_videos[_rightVideo]->thumbnail);
    image.load(&rightPixels, QByteArrayLiteral("JPG"));
    ui->rightImage->setPixmap(QPixmap::fromImage(image).scaled(
                              ui->rightImage->width(), ui->rightImage->height(), Qt::KeepAspectRatio));
}

void Comparison::wheelEvent(QWheelEvent *event)
{
    const QPoint pos = QCursor::pos();
    if(!QApplication::widgetAt(pos))
        return;
    ClickableLabel *imagePtr;
    if(QApplication::widgetAt(pos)->objectName() == "leftImage")
        imagePtr = ui->leftImage;
    else if(QApplication::widgetAt(pos)->objectName() == "rightImage")
        imagePtr = ui->rightImage;
    else
        return;

    // THEO : pixmap()->xxx didn't seem to work, as imagePtr is a pointer but imagePtr->pixmap() returns the object directly and not a pointer
    const int wmax = imagePtr->mapToGlobal(QPoint(imagePtr->pixmap().width(), 0)).x();         //image right edge
    const int hmax = imagePtr->mapToGlobal(QPoint(0, imagePtr->pixmap().height())).y();        //image bottom edge
    const double ratiox = 1-static_cast<double>(wmax-pos.x()) / imagePtr->pixmap().width();    //mouse pos inside image
    const double ratioy = 1-static_cast<double>(hmax-pos.y()) / imagePtr->pixmap().height();

    const int widescreenBlack = (imagePtr->height() - imagePtr->pixmap().height()) / 2;
    const int imgTop = imagePtr->mapToGlobal(QPoint(0,0)).y() + widescreenBlack;
    const int imgBtm = imgTop + imagePtr->pixmap().height();
    if(pos.x() > wmax || pos.y() < imgTop || pos.y() > imgBtm)      //image is smaller than label underneath
        return;

    if(_zoomLevel == 0)     //first mouse wheel movement: retrieve actual screen captures in full resolution
    {
        QApplication::setOverrideCursor(Qt::WaitCursor);

        QImage image;
        image = _videos[_leftVideo]->ffmpegLib_captureAt(10);
        ui->leftImage->setPixmap(QPixmap::fromImage(image).scaled(
                                 ui->leftImage->width(), ui->leftImage->height(), Qt::KeepAspectRatio));
        _leftZoomed = QPixmap::fromImage(image);      //keep it in memory
        _leftW = image.width();
        _leftH = image.height();

        image = _videos[_rightVideo]->ffmpegLib_captureAt(10);
        ui->rightImage->setPixmap(QPixmap::fromImage(image).scaled(
                                  ui->rightImage->width(), ui->rightImage->height(), Qt::KeepAspectRatio));
        _rightZoomed = QPixmap::fromImage(image);
        _rightW = image.width();
        _rightH = image.height();

        _zoomLevel = 1;
        QApplication::restoreOverrideCursor();
        return;
    }

    // THEO : event.delta() stopped working as of QT 5.15, need to use either pixel or angleDelta (check later if y is the correct logic to do here)
    if(event->angleDelta().y() > 0 && _zoomLevel < 10)   //mouse wheel up
        _zoomLevel = _zoomLevel * 2;
    if(event->angleDelta().y() < 0 && _zoomLevel > 1)    //mouse wheel down
        _zoomLevel = _zoomLevel / 2;

    QPixmap pix;
    pix = _leftZoomed.copy(static_cast<int>(_leftW*ratiox-_leftW*ratiox/_zoomLevel),
                           static_cast<int>(_leftH*ratioy-_leftH*ratioy/_zoomLevel),
                           _leftW/_zoomLevel, _leftH/_zoomLevel);
    ui->leftImage->setPixmap(pix.scaled(ui->leftImage->width(), ui->leftImage->height(),
                                        Qt::KeepAspectRatio, Qt::FastTransformation));

    pix = _rightZoomed.copy(static_cast<int>(_rightW*ratiox-_rightW*ratiox/_zoomLevel),
                           static_cast<int>(_rightH*ratioy-_rightH*ratioy/_zoomLevel),
                           _rightW/_zoomLevel, _rightH/_zoomLevel);
    ui->rightImage->setPixmap(pix.scaled(ui->rightImage->width(), ui->rightImage->height(),
                                         Qt::KeepAspectRatio, Qt::FastTransformation));
}

bool Comparison::isFileInProtectedFolder(const QString filePathName) const {
    const QListWidget* list = ui->lockedFolderslistWidget;
    for (int i = 0; i < list->count(); ++i) {
        const QString folderPath = list->item(i)->text();
        if(filePathName.contains(folderPath))
            return true;
    }
    return false;
}
// ------------------------------------------------------------------------
// ------------------ Automatic video deletion functions ------------------

void Comparison::on_identicalFilesAutoTrash_clicked()
{
    // Loop through all files
    // If both files have all equal parameters, except name and path.
    // Keep the left one (just a random choice but either could be kept).

    int initialDeletedNumber = _videosDeleted;
    int64_t initialSpaceSaved = _spaceSaved;
    bool userWantsToStop = false;

    // Go over all videos from begin to end
    _leftVideo = 0; // reset to first video
    _rightVideo = 0;

    ui->tabWidget->setCurrentIndex(0); // switch to manual tab so that user can see progress and details if confirmation is still on

    QVector<Video*>::const_iterator left, right, begin = _videos.cbegin(), end = _videos.cend();
    for(left=begin+_leftVideo; left<end; left++, _leftVideo++)
    {
        for(_rightVideo++, right=begin+_rightVideo; right<end; right++, _rightVideo++)
        {
            if(bothVideosMatch(*left, *right)
                    && QFileInfo::exists((*left)->_filePathName) && !(*left)->trashed // check trashed in case it is from Apple Photos
                    && QFileInfo::exists((*right)->_filePathName) && !(*right)->trashed )
            {
                showVideo(QStringLiteral("left"));
                showVideo(QStringLiteral("right"));
                highlightBetterProperties();
                updateUI();

                // Check if params are equal and perform deletion, then go to next
                if(qAbs(_videos[_leftVideo]->size - _videos[_rightVideo]->size)>FILE_SIZE_BYTES_DIFF_STILL_EQUALS)
                    continue;
                // TODO mklemewmqwhoi13u18134tih2g
                if(_videos[_leftVideo]->modified != _videos[_rightVideo]->modified)
                    continue;
                if(_videos[_leftVideo]->duration != _videos[_rightVideo]->duration)
                    continue;
                if(_videos[_leftVideo]->height != _videos[_rightVideo]->height)
                    continue;
                if(_videos[_leftVideo]->width != _videos[_rightVideo]->width)
                    continue;
                if(qAbs(_videos[_leftVideo]->bitrate - _videos[_rightVideo]->bitrate)>BITRATE_DIFF_STILL_EQUAL_kbs) //leave some margin due to decoding error
                    continue;
                if(_videos[_leftVideo]->framerate != _videos[_rightVideo]->framerate)
                    continue;
                if(_videos[_leftVideo]->codec != _videos[_rightVideo]->codec)
                    continue;
                if(_videos[_leftVideo]->audio != _videos[_rightVideo]->audio)
                    continue;

                int containedStatus = whichFilenameContainsTheOther((*left)->_filePathName, (*right)->_filePathName);

                if(ui->settingNamesInAnotherCheckbox->isChecked()
                        && containedStatus == NOT_CONTAINED)
                    continue; // the file names were not contained in one another : we go to the next comparison

                if(containedStatus == LEFT_CONTAINS_RIGHT)
                    deleteVideo(_leftVideo, true);
                else // by default and in specific name contained case : delete right video
                    deleteVideo(_rightVideo, true);

                // ask user if he wants to continue or stop the auto deletion, and maybe disable confirmations
                if(!ui->disableDeleteConfirmationCheckbox->isChecked()){
                    QMessageBox message;
                    message.setWindowTitle("Auto trash confirmation");
                    message.setText("Do you want to continue the auto deletion, and maybe disable confirmations ?");
                    message.addButton(tr("Continue"), QMessageBox::AcceptRole);
                    QPushButton *stopButton = message.addButton(tr("Stop"), QMessageBox::RejectRole);
                    QPushButton *disableConfirmationsButton = message.addButton(tr("Disable"), QMessageBox::ActionRole);
                    message.exec();
                    if (message.clickedButton() == stopButton) {
                        userWantsToStop = true;
                        break;
                    } else if (message.clickedButton() == disableConfirmationsButton)
                        ui->disableDeleteConfirmationCheckbox->setCheckState(Qt::Checked);

                    // after prompting the user, if the left video was deleted we must break out of the
                    // inner for loop to go to the next left/reference video
                    if(containedStatus == LEFT_CONTAINS_RIGHT)
                        break;
                }
            }
        }
        ui->progressBar->setValue(comparisonsSoFar());
        _rightVideo = _leftVideo + 1;
        if(userWantsToStop)
            break;
    }

    if(!userWantsToStop) //finished going through all videos, check if there are still some matches from beginning
    {
        ui->tabWidget->setCurrentIndex(1); // switch back to auto tab
        _leftVideo = 0;
        _rightVideo = 0;
    }
    // display statistics of deletions
    QMessageBox::information(this, "Auto identical files deletion complete",
                             QString("%1 dupplicate files were moved to trash, saving %2 of disk space !")
                             .arg(_videosDeleted-initialDeletedNumber).arg(readableFileSize(_spaceSaved-initialSpaceSaved)));
    if(_someWereMovedInApplePhotosLibrary)
        displayApplePhotosAlbumDeletionMessage();
    on_nextVideo_clicked();
}

void Comparison::on_autoDelOnlySizeDiffersButton_clicked()
{
    // Loop through all files
    // If both have :
    // - same time duration
    // - same resolution
    // - same FPS
    // - different file sizes
    // Keeps the bigger file.

    int initialDeletedNumber = _videosDeleted;
    int64_t initialSpaceSaved = _spaceSaved;
    bool userWantsToStop = false;

    // Go over all videos from begin to end
    _leftVideo = 0; // reset to first video
    _rightVideo = 0;

    ui->tabWidget->setCurrentIndex(0); // switch to manual tab so that user can see progress and details if confirmation is on
    QCoreApplication::processEvents(); //next operations are blocking, might need to find a way to make it work nicer !

    QVector<Video*>::const_iterator left, right, begin = _videos.cbegin(), end = _videos.cend();
    for(left=begin+_leftVideo; left<end; left++, _leftVideo++)
    {
        for(_rightVideo++, right=begin+_rightVideo; right<end; right++, _rightVideo++)
        {
            if(bothVideosMatch(*left, *right)
                    && QFileInfo::exists((*left)->_filePathName) && !(*left)->trashed // check trashed in case it is from Apple Photos
                    && QFileInfo::exists((*right)->_filePathName) && !(*right)->trashed )
            {
                ui->progressBar->setValue(comparisonsSoFar()); //update visible progress for user

                // Check if params are as required and perform deletion, then go to next
                if(qAbs(_videos[_leftVideo]->duration - _videos[_rightVideo]->duration) > VIDEO_DURATION_STILL_EQUALS_MS) // video durations more than 1 second length difference
                    continue;
                if(!ui->autoOnlySizeDontCheckResFpsCheckbox->isChecked())
                {
                    if(_videos[_leftVideo]->height != _videos[_rightVideo]->height)
                        continue;
                    if(_videos[_leftVideo]->width != _videos[_rightVideo]->width)
                        continue;
                    if(qAbs(_videos[_leftVideo]->framerate - _videos[_rightVideo]->framerate) > 0.1) //both framerates more than 0.1 fps different
                        continue;
                }
                if(qAbs(_videos[_leftVideo]->size - _videos[_rightVideo]->size) <= FILE_SIZE_BYTES_DIFF_STILL_EQUALS) // When sizes are identical, results are treated in specific other functionality
                    continue;
                if(ui->settingNamesInAnotherCheckbox->isChecked()
                        && whichFilenameContainsTheOther((*left)->_filePathName, (*right)->_filePathName) == NOT_CONTAINED)
                    continue; // the file names were not contained in one another : we go to the next comparison

                showVideo(QStringLiteral("left"));
                showVideo(QStringLiteral("right"));
                highlightBetterProperties();
                updateUI();

                if(_videos[_leftVideo]->size > _videos[_rightVideo]->size)
                    deleteVideo(_rightVideo, true);
                else
                    deleteVideo(_leftVideo, true);

                // ask user if he wants to continue or stop the auto deletion, and maybe disable confirmations
                if(!ui->disableDeleteConfirmationCheckbox->isChecked()){
                    QMessageBox message;
                    message.setWindowTitle("Auto trash smaller file sizes confirmation");
                    message.setText("Do you want to continue the auto deletion of smaller file sizes, and maybe disable confirmations ?");
                    message.addButton(tr("Continue"), QMessageBox::AcceptRole);
                    QPushButton *stopButton = message.addButton(tr("Stop"), QMessageBox::RejectRole);
                    QPushButton *disableConfirmationsButton = message.addButton(tr("Disable confirmations"), QMessageBox::ActionRole);
                    message.exec();
                    if (message.clickedButton() == stopButton) {
                        userWantsToStop = true;
                        break;
                    } else if (message.clickedButton() == disableConfirmationsButton)
                        ui->disableDeleteConfirmationCheckbox->setCheckState(Qt::Checked);
                }

                // when left video was deleted (i.e. right video size is bigger), we need to break
                // out of the inner for loop to go to the next left/reference video
                if(!(_videos[_leftVideo]->size > _videos[_rightVideo]->size))
                    break;
            }
        }
        ui->progressBar->setValue(comparisonsSoFar());
        _rightVideo = _leftVideo + 1;
        if(userWantsToStop)
            break;
    }

    if(!userWantsToStop) //finished going through all videos, check if there are still some matches from beginning
    {
        ui->tabWidget->setCurrentIndex(1); // switch back to auto tab
        _leftVideo = 0;
        _rightVideo = 0;
    }
    // display statistics of deletions
    QMessageBox::information(this, "Auto trash smaller file sizes complete",
                             QString("%1 dupplicate files were moved to trash, saving %2 of disk space !")
                             .arg(_videosDeleted-initialDeletedNumber).arg(readableFileSize(_spaceSaved-initialSpaceSaved)));

    if(_someWereMovedInApplePhotosLibrary)
        displayApplePhotosAlbumDeletionMessage();
    on_nextVideo_clicked();
}

void Comparison::autoDeleteLoopthrough(const AutoDeleteConfig autoDelConfig){
    // loop through all files
    // and maybe trash one each time depending on config

    int initialDeletedNumber = _videosDeleted;
    int64_t initialSpaceSaved = _spaceSaved;
    bool userWantsToStop = false;

    // Go over all videos from begin to end
    _leftVideo = 0; // reset to first video
    _rightVideo = 0;

    ui->tabWidget->setCurrentIndex(0); // switch to manual tab so that user can see progress and details if confirmation is on
    QCoreApplication::processEvents(); //next operations are blocking, might need to find a way to make it work nicer !

    QVector<Video*>::const_iterator left, right, begin = _videos.cbegin(), end = _videos.cend();
    for(left=begin+_leftVideo; left<end; left++, _leftVideo++)
    {
        for(_rightVideo++, right=begin+_rightVideo; right<end; right++, _rightVideo++)
        {
            if(bothVideosMatch(*left, *right)
                    && QFileInfo::exists((*left)->_filePathName) && !(*left)->trashed // check trashed in case it is from Apple Photos
                    && QFileInfo::exists((*right)->_filePathName) && !(*right)->trashed )
            {
                ui->progressBar->setValue(comparisonsSoFar()); //update visible progress for user
                QCoreApplication::processEvents();

                // Check if params are as required or go to next
                if(ui->settingNamesInAnotherCheckbox->isChecked()
                        && whichFilenameContainsTheOther((*left)->_filePathName, (*right)->_filePathName) == NOT_CONTAINED)
                    continue; // the file names were not contained in one another : we go to the next comparison

                //find for the specific auto mode if one video needs to be deleted
                const VideoMetadata leftVidMeta = Video::videoToMetadata(*_videos[_leftVideo]);
                const VideoMetadata rightVidMeta = Video::videoToMetadata(*_videos[_rightVideo]);

                const VideoMetadata* vidToDeleteMetaPtr = autoDelConfig.videoToDelete(&leftVidMeta, &rightVidMeta,
                                            AutoDeleteUserSettings(ui->radioButton_onlyTimeDiffers_trashEarlier->isChecked()));
                if(vidToDeleteMetaPtr==nullptr) // null means the videos don't match in the auto mode
                    continue;

                // now we know videos are matched, we show them and the auto deletion goes through
                showVideo(QStringLiteral("left"));
                showVideo(QStringLiteral("right"));
                highlightBetterProperties();
                updateUI();

                if(vidToDeleteMetaPtr == &leftVidMeta)
                    deleteVideo(_leftVideo, true);
                else
                    deleteVideo(_rightVideo, true);

                // ask user if he wants to continue or stop the auto deletion, and maybe disable confirmations
                if(!ui->disableDeleteConfirmationCheckbox->isChecked()){
                    QMessageBox message;
                    message.setWindowTitle(QString("Auto trash by %1 confirmation").arg(autoDelConfig.getDeleteByText()));
                    message.setText(QString("Do you want to continue the auto deletion by %1, and maybe disable confirmations ?").arg(autoDelConfig.getDeleteByText()));
                    message.addButton(tr("Continue"), QMessageBox::AcceptRole);
                    QPushButton *stopButton = message.addButton(tr("Stop"), QMessageBox::RejectRole);
                    QPushButton *disableConfirmationsButton = message.addButton(tr("Disable confirmations"), QMessageBox::ActionRole);
                    message.exec();
                    if (message.clickedButton() == stopButton) {
                        userWantsToStop = true;
                        break;
                    } else if (message.clickedButton() == disableConfirmationsButton)
                        ui->disableDeleteConfirmationCheckbox->setCheckState(Qt::Checked);
                }

                // when left video was deleted, we need to break
                // out of the inner for loop to go to the next left/reference video
                if(vidToDeleteMetaPtr == &leftVidMeta)
                    break;
            }
        }
        ui->progressBar->setValue(comparisonsSoFar());
        _rightVideo = _leftVideo + 1;
        if(userWantsToStop)
            break;
    }

    if(!userWantsToStop) //finished going through all videos, check if there are still some matches from beginning
    {
        ui->tabWidget->setCurrentIndex(1); // switch back to auto tab
        _leftVideo = 0;
        _rightVideo = 0;
    }
    // display statistics of deletions
    QMessageBox::information(this, QString("Auto trash by %1 complete").arg(autoDelConfig.getDeleteByText()),
                             QString("%1 dupplicate files were moved to trash, saving %2 of disk space !")
                             .arg(_videosDeleted-initialDeletedNumber).arg(readableFileSize(_spaceSaved-initialSpaceSaved)));

    if(_someWereMovedInApplePhotosLibrary)
        displayApplePhotosAlbumDeletionMessage();
    on_nextVideo_clicked();
}

const VideoMetadata* Comparison::AutoDeleteConfig::videoToDelete(const VideoMetadata* meta1, const VideoMetadata* meta2, const AutoDeleteUserSettings userAutoDelConf) const {
    if(_autoDelConfig == AUTO_DELETE_ONLY_TIMES_DIFF){

        if(qAbs(meta1->size - meta2->size) > FILE_SIZE_BYTES_DIFF_STILL_EQUALS)
            return nullptr;
        if(qAbs(meta1->duration - meta2->duration) > VIDEO_DURATION_STILL_EQUALS_MS)
            return nullptr;
        if(meta1->height != meta2->height)
            return nullptr;
        if(meta1->width != meta2->width)
            return nullptr;
        if(qAbs(meta1->bitrate - meta2->bitrate)>BITRATE_DIFF_STILL_EQUAL_kbs) //leave some margin due to decoding error
            return nullptr;
        if(meta1->framerate != meta2->framerate)
            return nullptr;
        if(meta1->codec != meta2->codec)
            return nullptr;
        if(meta1->audio != meta2->audio)
            return nullptr;

        // check the dates and which is earlier
        const VideoMetadata** earlierVideo;
        if(meta1->_fileCreateDate < meta2->_fileCreateDate)
           earlierVideo = &meta1;
        else if(meta1->_fileCreateDate > meta2->_fileCreateDate)
            earlierVideo = &meta2;
        else if(meta1->modified < meta2->modified)
            earlierVideo = &meta1;
        else if(meta1->modified > meta2->modified)
            earlierVideo = &meta2;
        else
            return nullptr; // all dates are equal

        const VideoMetadata** laterVideo = &meta1;
        if(earlierVideo == &meta1)
            laterVideo = &meta2;

        //tell to delete depending on user setting
        if(userAutoDelConf.trashEarlierIsChecked)
            return *earlierVideo;
        else
            return *laterVideo;
    }
    else
        return nullptr;
}

QString Comparison::AutoDeleteConfig::getDeleteByText() const {
    switch (_autoDelConfig) {
    case AUTO_DELETE_ONLY_TIMES_DIFF:
        return "dates";
    default:
        return "";
    }
}

int Comparison::whichFilenameContainsTheOther(QString leftFileNamepath, QString rightFileNamepath) const {
    const QFileInfo leftVideoFile(leftFileNamepath);
    const QString leftFilename = leftVideoFile.fileName();
    const QString leftNoExtension = leftFilename.left(leftFilename.lastIndexOf("."));

    const QFileInfo rightVideoFile(rightFileNamepath);
    const QString rightFilename = rightVideoFile.fileName();
    const QString rightNoExtension = rightFilename.left(rightFilename.lastIndexOf("."));

    int containedStatus = NOT_CONTAINED;

    if(rightNoExtension.contains(leftNoExtension))
        containedStatus = RIGHT_CONTAINS_LEFT;
    else if(leftNoExtension.contains(rightNoExtension))
        containedStatus = LEFT_CONTAINS_RIGHT;

    return containedStatus;
}

// ------------------ End of : Automatic video deletion functions ------------------
// ---------------------------------------------------------------------------------

void Comparison::on_pushButton_importantFoldersAdd_clicked()
{
    const QString dir = QFileDialog::getExistingDirectory(ui->pushButton_importantFoldersAdd,
                                                              QByteArrayLiteral("Open folder"),
                                                              QStandardPaths::standardLocations(QStandardPaths::MoviesLocation).first() /*defines where the chooser opens at*/,
                                                              QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if(dir.isEmpty()){ //empty because error or none chosen in dialog
        return;
    }
    ui->importantFoldersListWidget->addItem(dir);
    ui->importantFoldersListWidget->setFocus();
}

void Comparison::on_lockedFolderButton_clicked()
{
    const QString dir = QFileDialog::getExistingDirectory(ui->lockedFolderButton,
                                                              QByteArrayLiteral("Open folder"),
                                                              QStandardPaths::standardLocations(QStandardPaths::MoviesLocation).first() /*defines where the chooser opens at*/,
                                                              QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if(dir.isEmpty()){ //empty because error or none chosen in dialog
        return;
    }
    ui->lockedFolderslistWidget->addItem(dir);
    ui->lockedFolderslistWidget->setFocus();

}

void Comparison::eraseImportantFolderItem(){
    // If multiple selection is on, we need to erase all selected items
    for (int i = 0; i < ui->importantFoldersListWidget->selectedItems().size(); ++i) {
        // Get curent item on selected row
        QListWidgetItem *item = ui->importantFoldersListWidget->takeItem(ui->importantFoldersListWidget->currentRow());
        // And remove it
        delete item;
    }
}

void Comparison::eraseLockedFolderItem(){
    // If multiple selection is on, we need to erase all selected items
    for (int i = 0; i < ui->lockedFolderslistWidget->selectedItems().size(); ++i) {
        // Get curent item on selected row
        QListWidgetItem *item = ui->lockedFolderslistWidget->takeItem(ui->lockedFolderslistWidget->currentRow());
        // And remove it
        delete item;
    }
}

void Comparison::clearImportantFolderList(){ ui->importantFoldersListWidget->clear(); }
void Comparison:: clearLockedFolderList() { ui->lockedFolderslistWidget->clear(); }

void Comparison::showImportantFolderContextMenu(const QPoint &pos){
    // Handle global position
    QPoint globalPos = ui->importantFoldersListWidget->mapToGlobal(pos);

    // Create menu and insert some actions
    QMenu myMenu;
    myMenu.addAction("Delete selection", this, SLOT(eraseImportantFolderItem()));
    myMenu.addAction("Add new",  this, SLOT(on_importantFolderButton_clicked()) );
    myMenu.addAction("Clear all",  this, SLOT(clearImportantFolderList()));

    // Show context menu at handling position
    myMenu.exec(globalPos);
}

void Comparison::showLockedFolderContextMenu(const QPoint &pos){
    // Handle global position
    QPoint globalPos = ui->lockedFolderslistWidget->mapToGlobal(pos);

    // Create menu and insert some actions
    QMenu myMenu;
    myMenu.addAction("Delete selection", this, SLOT(eraseLockedFolderItem()));
    myMenu.addAction("Add new",  this, SLOT(on_lockedFolderButton_clicked()));
    myMenu.addAction("Clear all",  this, SLOT(clearLockedFolderList()));

    // Show context menu at handling position
    myMenu.exec(globalPos);
}

void Comparison::displayApplePhotosAlbumDeletionMessage() {
    QMessageBox::information(this, "",
         QString("Notice: \n\nSome videos were not actually deleted"
                 " as they were from an Apple Photos Library.\n"
                 "They were added to the album 'Trash from %1'. "
                 "You must manually delete them from within "
                 "Apple Photos ! \n\n"
                 "From Apple Photos, select them and press 'cmd' and 'delete' "
                 " (or right click while pressing 'cmd', and select the option "
                 " 'Delete', ⚠️ but not 'Delete from album' !!!)\n\n"
                 "Then empty Apple Photos' trash").arg(APP_NAME));
}

void Comparison::on_settingNamesInAnotherCheckbox_stateChanged(int arg1)
{
    QString status;
    if(arg1==Qt::Checked) status="ENABLED";
    else status="DISABLED";

    ui->label_namesContainedInOneAnotherStatus_autoIdentFiles->setText(status);
    ui->label_namesContainedInOneAnotherStatus_autoOnlySizeDiff->setText(status);
}

void Comparison::on_ignoreDuplicatePairButton_clicked()
{
    Db cache(_prefs.cacheFilePathName); // opening connexion to database
    cache.writePairToIgnore(_videos[_leftVideo]->_filePathName, _videos[_rightVideo]->_filePathName);
    on_nextVideo_clicked();
}

