#include "comparison.h"

#include <QAbstractSlider>
#include <QElapsedTimer>
#include <QMimeData>
#include <QProcess> // for opening a file in the platform file manager
#include <QProgressDialog>
#include <QSignalBlocker>
#include <QSlider>

#include "internal/backgroundmatchdiscovery.h"
#include "internal/videopairmatcher.h"
#include "internal/videopairspace.h"

#include "ui_comparison.h" // WARNING : don't include this in the header file, otherwise includes from other files will be broken

enum FILENAME_CONTAINED_WITHIN_ANOTHER : int {
    NOT_CONTAINED,
    LEFT_CONTAINS_RIGHT,
    RIGHT_CONTAINS_LEFT
};

const QString TEXT_STYLE_GREEN = QStringLiteral("QLabel { color : green; }");
const QString TEXT_STYLE_ORANGE = QStringLiteral("QLabel { color : peru; }");

const int64_t FILE_SIZE_BYTES_DIFF_STILL_EQUALS = 100 * 1024;
const int64_t VIDEO_DURATION_STILL_EQUALS_MS = 1000; //if this close in duration then it's considered equal
const int BITRATE_DIFF_STILL_EQUAL_kbs = 5;
// Modal progress updates process UI events and are expensive. Throttling by time
// keeps navigation responsive without making its overhead depend on library size.
constexpr qint64 PROGRESS_REFRESH_INTERVAL_MS = 100;

#ifdef Q_OS_MACOS
QString applePhotosNameFromPhotoKit(const QString& mediaId)
{
    const std::string name = Obj_C::obj_C_getMediaNameFromPhotoKit(mediaId.toStdString());
    return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
}
#endif

Comparison::Comparison(const QVector<Video*>& videosParam, Prefs& prefsParam, const QRect& mainWindowGeometry)
    : QDialog(prefsParam._mainwPtr, Qt::Window), ui(new Ui::Comparison), _videos(videosParam), _prefs(prefsParam),
      _maxComparisons(VideoPairSpace::comparisonCount(videosParam.size())),
      _backgroundDiscovery(std::make_unique<BackgroundMatchDiscovery>())
{
    ui->setupUi(this);

#ifdef Q_OS_MACOS
    _applePhotosNameLookup = applePhotosNameFromPhotoKit;
#endif

    this->setGeometry(mainWindowGeometry);

    connect(this, SIGNAL(sendStatusMessage(const QString&)), _prefs._mainwPtr, SLOT(addStatusMessage(const QString&)));
    connect(this, SIGNAL(switchComparisonMode(const int&)), _prefs._mainwPtr, SLOT(setComparisonMode(const int&)));
    connect(this, SIGNAL(adjustThresholdSlider(const int&)), _prefs._mainwPtr,
            SLOT(on_thresholdSlider_valueChanged(const int&)));
    connect(ui->progressBar, &QSlider::valueChanged,
            [this](int value) { ui->currentVideo->setText(QString::number(value)); });
    // The pair-space slider is retained as a read-only progress indicator for
    // automatic cleanup and background discovery. Manual review is set-driven;
    // allowing seeks here would desynchronise the selected set and displayed pair.
    ui->progressBar->setEnabled(false);
    connect(_backgroundDiscovery.get(), &BackgroundMatchDiscovery::preScannedEndChanged, this,
            &Comparison::updateDiscoveryProgress);
    connect(ui->duplicateSets, &QListWidget::currentRowChanged, this, &Comparison::on_duplicateSets_currentRowChanged);
    connect(ui->duplicateSetMembers, &QListWidget::currentRowChanged, this,
            &Comparison::on_duplicateSetMembers_currentRowChanged);
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this]() {
        if (ui->tabWidget->currentWidget() != ui->tabManual)
            return;
        if (_selectedDuplicateSet >= 0 && _selectedDuplicateSet < _duplicateSets.size())
            showSetMember(_selectedSetMember);
        else {
            clearManualComparisonDisplay();
            setManualComparisonActionsEnabled(false);
        }
    });

    initSortOrder();

    if (this->_prefs.comparisonMode() == Prefs::_SSIM)
        ui->selectSSIM->setChecked(true);
    on_thresholdSlider_valueChanged(this->_prefs.matchSimilarityThreshold());

    ui->progressBar->setMinimum(1);
    ui->progressBar->setMaximum(progressBarValue(_maxComparisons));
    ui->currentVideo->setNum(ui->progressBar->value());

    ui->trashedFiles->setVisible(false);                        // hide until at least one file is deleted
    ui->totalVideos->setText(QString::number(_maxComparisons)); // all possible combinations

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
    ui->pushButton_importantFoldersAdd->setIcon(
        ui->pushButton_importantFoldersAdd->style()->standardIcon(QStyle::SP_DirOpenIcon));
    setAcceptDrops(true); // drag and drop events for locked folders list
    loadLockedFolderFromPrefs();
    ui->lockedFolderButton->setIcon(ui->lockedFolderButton->style()->standardIcon(QStyle::SP_DirOpenIcon));
    connect(ui->importantFoldersListWidget, SIGNAL(customContextMenuRequested(QPoint)), this,
            SLOT(showImportantFolderContextMenu(QPoint)));
    connect(ui->lockedFolderslistWidget, SIGNAL(customContextMenuRequested(QPoint)), this,
            SLOT(showLockedFolderContextMenu(QPoint)));
    // (pressing DEL activates the slots only when list widget has focus)
    QShortcut* importantFoldersShortcut =
        new QShortcut(QKeySequence(Qt::Key_Delete), ui->importantFoldersListWidget); // doesn't seem to work...
    connect(importantFoldersShortcut, SIGNAL(activated()), this, SLOT(eraseImportantFolderItem()));
    QShortcut* lockedFoldersShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), ui->importantFoldersListWidget);
    connect(lockedFoldersShortcut, SIGNAL(activated()), this, SLOT(eraseLockedFolderItem()));

    //delete right, left, and go right, left shortcuts
    QShortcut* rightDelShortcut = new QShortcut(QKeySequence(QKeySequence::MoveToNextChar), ui->tabManual);
    connect(rightDelShortcut, &QShortcut::activated, this, [this]() {
        // Arrow keys must remain safe, ordinary navigation while either of the
        // new galleries has focus. Outside the galleries, preserve the existing
        // review shortcuts.
        if (ui->duplicateSetMembers->hasFocus()) {
            const int lastMember = ui->duplicateSetMembers->count() - 1;
            ui->duplicateSetMembers->setCurrentRow(qMin(ui->duplicateSetMembers->currentRow() + 1, lastMember));
            return;
        }
        if (!ui->duplicateSets->hasFocus())
            on_rightDelete_clicked();
    });
    QShortcut* leftDelShortcut = new QShortcut(QKeySequence(QKeySequence::MoveToPreviousChar), ui->tabManual);
    connect(leftDelShortcut, &QShortcut::activated, this, [this]() {
        if (ui->duplicateSetMembers->hasFocus()) {
            ui->duplicateSetMembers->setCurrentRow(qMax(ui->duplicateSetMembers->currentRow() - 1, 1));
            return;
        }
        if (!ui->duplicateSets->hasFocus())
            on_leftDelete_clicked();
    });

    QShortcut* downShortcut = new QShortcut(QKeySequence(QKeySequence::MoveToNextLine), ui->tabManual);
    connect(downShortcut, &QShortcut::activated, this, [this]() {
        if (ui->duplicateSets->hasFocus()) {
            const int lastSet = ui->duplicateSets->count() - 1;
            ui->duplicateSets->setCurrentRow(qMin(ui->duplicateSets->currentRow() + 1, lastSet));
            return;
        }
        on_nextVideo_clicked();
    });
    QShortcut* upShortcut = new QShortcut(QKeySequence(QKeySequence::MoveToPreviousLine), ui->tabManual);
    connect(upShortcut, &QShortcut::activated, this, [this]() {
        if (ui->duplicateSets->hasFocus()) {
            ui->duplicateSets->setCurrentRow(qMax(ui->duplicateSets->currentRow() - 1, 0));
            return;
        }
        on_prevVideo_clicked();
    });

    // we only show gps info if at least one of the files has it
    ui->labelLeftGps->setVisible(false);
    ui->leftGpsCoordinates->setVisible(false);
    ui->labelRightGps->setVisible(false);
    ui->rightGpsCoordinates->setVisible(false);

    // Add Cmd+W shortcut to close the comparison window (actually a dialog so closes with accept)
    connect(new QShortcut(QKeySequence::Close, this), &QShortcut::activated, this, &Comparison::accept);
    // Cmd+Q shortcut to quit the application from the comparison dialog, not handled by default
    connect(new QShortcut(QKeySequence::Quit, this), &QShortcut::activated, qApp, &QApplication::quit);

    applySortOrder();
}

// NB Sort order impacts auto deletion as they can assume sorting by size with left video being biggest
// All three auto delete modes are compatible with any sort order though:
// - Identical files: on_identicalFilesAutoTrash_clicked() keeps a random one which is ok as they're identical
// - Keep bigest: on_autoDelOnlySizeDiffersButton_clicked() keeps the biggest one which is ok as it's the point
// - Keep by date:
//     on_pushButton_onlyTimeDiffersAutoTrash_clicked/autoDeleteLoopthrough(AUTO_DELETE_ONLY_TIMES_DIFF)
//     keeps the earliest/latest one as selected by user
void Comparison::initSortOrder()
{
    switch (_prefs.sortCriterion()) {
    case Prefs::SortCriterion::BySizeDescending:
        ui->comboBox_sortBy->setCurrentIndex(0);
        break;
    case Prefs::SortCriterion::ByNameAscending:
        ui->comboBox_sortBy->setCurrentIndex(1);
        break;
    case Prefs::SortCriterion::ByCreationDateAscending:
        ui->comboBox_sortBy->setCurrentIndex(2);
        break;
    }
    // Connect signal after setting initial index to avoid premature trigger
    connect(ui->comboBox_sortBy, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &Comparison::onSortOrderChanged);
}

void Comparison::onSortOrderChanged(int index)
{
    switch (index) {
    case 0: // File size (largest first)
        _prefs.sortCriterion(Prefs::SortCriterion::BySizeDescending);
        break;
    case 1: // File name (A-Z)
        _prefs.sortCriterion(Prefs::SortCriterion::ByNameAscending);
        break;
    case 2: // Creation time (oldest first)
        _prefs.sortCriterion(Prefs::SortCriterion::ByCreationDateAscending);
        break;
    }
    applySortOrder();
}

void Comparison::applySortOrder()
{
    // Idea for later: we could re order in place if background scan is complete
    _backgroundDiscovery->stop();
    switch (_prefs.sortCriterion()) {
    case Prefs::SortCriterion::BySizeDescending:
        std::sort(_videos.begin(), _videos.end(), [](const Video* a, const Video* b) {
            return a->size > b->size; // Sort by size in descending order
        });
        break;
    case Prefs::SortCriterion::ByNameAscending:
        std::sort(this->_videos.begin(), this->_videos.end(), [](const Video* a, const Video* b) {
            return QString::localeAwareCompare(a->_filePathName, b->_filePathName) < 0;
        });
        break;
    case Prefs::SortCriterion::ByCreationDateAscending:
        std::sort(this->_videos.begin(), this->_videos.end(),
                  [](const Video* a, const Video* b) { return a->_fileCreateDate < b->_fileCreateDate; });
        break;
    }

    _leftVideo = 0;
    _rightVideo = 0;

    restartBackgroundDiscovery();
}

Comparison::~Comparison()
{
    _backgroundDiscovery->stop();
    delete ui;
}

// todo should be made const and run in background from mainwindow
int Comparison::reportMatchingVideos()
{
    int64_t combinedFilesize = 0;
    int foundMatches = 0;
    QProgressDialog progress("Estimating total pairs", QString(), 0, this->_prefs._numberOfVideos,
                             this->_prefs._mainwPtr);
    progress.setWindowModality(Qt::WindowModal);
    int numScanned = 0;

    QVector<Video*>::const_iterator left, right, end = _videos.cend();
    for (left = _videos.cbegin(); left < end; left++) {
        for (right = left + 1; right < end; right++) {
            if (bothVideosMatch(*left, *right)) { //smaller of two matching videos is likely the one to be deleted
                combinedFilesize += std::min((*left)->size, (*right)->size);
                foundMatches++;
                break;
            }
        }
        if (numScanned++ % 1000 == 999)
            progress.setValue(numScanned++);
    }

    if (foundMatches)
        emit sendStatusMessage(QStringLiteral("\n[%1] Found %2 video(s) (%3) with one or more matches")
                                   .arg(QTime::currentTime().toString())
                                   .arg(foundMatches)
                                   .arg(readableFileSize(combinedFilesize)));

    return foundMatches;
}

void Comparison::confirmToExit()
{
    int confirm = QMessageBox::Yes;
    if (!ui->leftFileName->text().isEmpty()) {
        QMessageBox msgBox;
        msgBox.setWindowTitle(QStringLiteral("Out of videos to compare"));
        msgBox.setText(QStringLiteral("Close window?                  "));
        msgBox.setIcon(QMessageBox::QMessageBox::Question);
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        confirm = msgBox.exec();
    }
    if (confirm == QMessageBox::Yes) {
        if (_someWereMovedInApplePhotosLibrary)
            displayApplePhotosAlbumDeletionMessage();
        if (_videosDeleted)
            emit sendStatusMessage(QStringLiteral("\n%1 file(s) removed, %2 freed")
                                       .arg(_videosDeleted)
                                       .arg(readableFileSize(_spaceSaved)));
        if (!ui->leftFileName->text().isEmpty())
            emit sendStatusMessage(QStringLiteral("\nPressing Find duplicates button opens comparison window "
                                                  "again if thumbnail mode and directories remain the same"));
        else
            emit sendStatusMessage(QStringLiteral("\nComparison window closed because no matching videos found "
                                                  "(a lower threshold may help to find more matches)"));

        QKeyEvent* closeEvent = new QKeyEvent(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::postEvent(this, closeEvent); //"pressing" ESC closes dialog
    }
    else {
        // A slider drag moves the handle before navigation confirms a pair.
        // Restore the position of the last pair when closing is cancelled.
        ui->progressBar->setValue(progressBarValue(comparisonsSoFar()));
    }
}

void Comparison::on_prevVideo_clicked()
{
    _seekForwards = false;
    if (ui->tabWidget->currentWidget() == ui->tabManual) {
        if (_selectedDuplicateSet >= 0 && _selectedDuplicateSet < _duplicateSets.size()) {
            const int lastMember = _duplicateSets[_selectedDuplicateSet].members.size() - 1;
            showSetMember(_selectedSetMember <= 1 ? lastMember : _selectedSetMember - 1);
        }
        return;
    }
    const int64_t currentPosition = comparisonsSoFar();
    if (currentPosition <= 1)
        return;

    const int64_t preScannedEnd = _backgroundDiscovery->preScannedEnd();
    const int64_t previousPosition = currentPosition - 1;
    if (previousPosition > preScannedEnd && navigateToPrevMatch(previousPosition, preScannedEnd + 1))
        return;

    int64_t cursor = qMin(currentPosition, preScannedEnd + 1);
    while (const auto candidate = _backgroundDiscovery->previousCandidateBefore(cursor)) {
        if (isPairStillDisplayable(*candidate)) {
            displayMatchedPair(*candidate);
            return;
        }
        cursor = candidate->position;
    }
}

void Comparison::on_nextVideo_clicked()
{
    _seekForwards = true;
    if (ui->tabWidget->currentWidget() == ui->tabManual) {
        if (_selectedDuplicateSet >= 0 && _selectedDuplicateSet < _duplicateSets.size()) {
            const int lastMember = _duplicateSets[_selectedDuplicateSet].members.size() - 1;
            showSetMember(_selectedSetMember >= lastMember ? 1 : _selectedSetMember + 1);
        }
        return;
    }
    if (!navigateForwardFrom(comparisonsSoFar()))
        confirmToExit();
}

bool Comparison::bothVideosMatch(const Video* left, const Video* right)
{
    if (left == nullptr || right == nullptr) {
        qCritical() << Q_FUNC_INFO << ": left or right video for comparison was null";
        return false;
    }

    const auto result = VideoPairMatcher::match(*left, *right, VideoPairMatcher::configFromPrefs(_prefs));
    _phashSimilarity = result.phashSimilarity;
    _ssimSimilarity = result.ssimSimilarity;
    if (!result.matches)
        return false;
    // check if pair is flagged as not dupplicate in DB. DB is very slow so only do this after all checks
    else if (Db(_prefs.cacheFilePathName()).isPairToIgnore(left->_filePathName, right->_filePathName))
        return false;

    return true;
}

void Comparison::restartBackgroundDiscovery()
{
    clearDuplicateSets();
    _backgroundDiscovery->start(_videos, VideoPairMatcher::configFromPrefs(_prefs));
}

void Comparison::updateDiscoveryProgress(int64_t preScannedEnd)
{
    ui->progressBar->setDiscoveredValue(progressBarValue(preScannedEnd));
    // Manual review navigates sets and their members, so the old pair position
    // is no longer meaningful here. Keep the read-only footer aligned with the
    // background scan instead of moving it whenever another member is shown.
    if (ui->tabWidget->currentWidget() == ui->tabManual)
        ui->progressBar->setValue(progressBarValue(preScannedEnd));
    const int percent = _maxComparisons > 0 ? int(100 * preScannedEnd / _maxComparisons) : 100;
    ui->progressBar->setToolTip(QStringLiteral("Background matching: %1% checked, %2 candidate pair(s) found")
                                    .arg(percent)
                                    .arg(_backgroundDiscovery->discoveredMatchCount()));
    rebuildDuplicateSets();
}

void Comparison::clearDuplicateSets()
{
    _duplicateSets.clear();
    _eligibleSetMatches.clear();
    _selectedDuplicateSet = -1;
    _selectedSetMember = -1;
    if (!ui)
        return;
    const QSignalBlocker blockSetSelection(ui->duplicateSets);
    const QSignalBlocker blockMemberSelection(ui->duplicateSetMembers);
    ui->duplicateSets->clear();
    ui->duplicateSetMembers->clear();
    ui->duplicateSetsStatus->setText(QStringLiteral("Scanning duplicate sets…"));
    ui->duplicateSetEvidence->clear();
    _currentComparisonIsDirectMatch = false;
    // Auto cleanup still uses the detailed pair display and its filename as
    // part of the existing completion/close flow. Only clear that surface when
    // the user is actually reviewing the set browser.
    if (ui->tabWidget->currentWidget() == ui->tabManual)
        clearManualComparisonDisplay();
    setManualComparisonActionsEnabled(false);
}

const MatchedVideoPair* Comparison::directEligiblePair(int left, int right) const
{
    for (const MatchedVideoPair& pair : _eligibleSetMatches)
        if ((pair.left == left && pair.right == right) || (pair.left == right && pair.right == left))
            return &pair;
    return nullptr;
}

void Comparison::rebuildDuplicateSets()
{
    const int previouslySelectedVideo =
        _selectedDuplicateSet >= 0 && _selectedDuplicateSet < _duplicateSets.size() && _selectedSetMember >= 0
            ? _duplicateSets[_selectedDuplicateSet].members[_selectedSetMember]
            : -1;
    _eligibleSetMatches.clear();
    for (const MatchedVideoPair& pair : _backgroundDiscovery->safeMatches())
        if (isPairStillDisplayable(pair))
            _eligibleSetMatches.append(pair);
    _duplicateSets = DuplicateSetBuilder::build(_videos.size(), _eligibleSetMatches);

    const QSignalBlocker blockSetSelection(ui->duplicateSets);
    ui->duplicateSets->clear();
    int videoCount = 0;
    for (int setIndex = 0; setIndex < _duplicateSets.size(); ++setIndex) {
        const DuplicateSet& set = _duplicateSets[setIndex];
        videoCount += set.members.size();
        qint64 size = 0;
        for (int member : set.members)
            size += _videos[member]->size;
        auto* item = new QListWidgetItem(
            QIcon(QPixmap::fromImage(QImage::fromData(_videos[set.members.first()]->thumbnail, "JPG"))),
            QStringLiteral("Set %1\n%2 videos · %3")
                .arg(setIndex + 1)
                .arg(set.members.size())
                .arg(readableFileSize(size)));
        item->setData(Qt::UserRole, setIndex);
        ui->duplicateSets->addItem(item);
    }

    if (_backgroundDiscovery->isComplete()) {
        ui->duplicateSetsStatus->setText(
            _duplicateSets.isEmpty()
                ? QStringLiteral("No duplicate sets found.")
                : QStringLiteral("%1 sets · %2 videos").arg(_duplicateSets.size()).arg(videoCount));
    }
    else if (_duplicateSets.isEmpty()) {
        ui->duplicateSetsStatus->setText(QStringLiteral("Scanning duplicate sets…"));
    }
    else {
        ui->duplicateSetsStatus->setText(
            QStringLiteral("%1 sets · %2 videos — collecting results…").arg(_duplicateSets.size()).arg(videoCount));
    }

    int selectedSet = -1;
    int selectedMember = 1;
    if (previouslySelectedVideo >= 0) {
        for (int setIndex = 0; setIndex < _duplicateSets.size() && selectedSet < 0; ++setIndex) {
            const int member = _duplicateSets[setIndex].members.indexOf(previouslySelectedVideo);
            if (member >= 0) {
                selectedSet = setIndex;
                selectedMember = member;
            }
        }
    }
    if (selectedSet < 0 && !_duplicateSets.isEmpty())
        selectedSet = 0;
    _selectedDuplicateSet = -1;
    _selectedSetMember = -1;
    if (selectedSet >= 0)
        selectDuplicateSet(selectedSet, selectedMember);
    else {
        const QSignalBlocker blockMemberSelection(ui->duplicateSetMembers);
        ui->duplicateSetMembers->clear();
        ui->duplicateSetEvidence->clear();
        _currentComparisonIsDirectMatch = false;
        if (ui->tabWidget->currentWidget() == ui->tabManual)
            clearManualComparisonDisplay();
        setManualComparisonActionsEnabled(false);
    }
}

void Comparison::selectDuplicateSet(int row, int preferredMember)
{
    if (row < 0 || row >= _duplicateSets.size())
        return;
    _selectedDuplicateSet = row;
    const DuplicateSet& set = _duplicateSets[row];
    const QSignalBlocker blockSetSelection(ui->duplicateSets);
    const QSignalBlocker blockMemberSelection(ui->duplicateSetMembers);
    ui->duplicateSets->setCurrentRow(row);
    ui->duplicateSetMembers->clear();
    for (int member = 0; member < set.members.size(); ++member) {
        const Video* video = _videos[set.members[member]];
        auto* item = new QListWidgetItem(
            QIcon(QPixmap::fromImage(QImage::fromData(video->thumbnail, "JPG"))),
            member == 0 ? QStringLiteral("Reference\n%1").arg(QFileInfo(video->_filePathName).fileName())
                        : QFileInfo(video->_filePathName).fileName());
        item->setData(Qt::UserRole, member);
        ui->duplicateSetMembers->addItem(item);
    }
    showSetMember(qBound(1, preferredMember, set.members.size() - 1));
}

void Comparison::showSetMember(int member)
{
    if (_selectedDuplicateSet < 0 || _selectedDuplicateSet >= _duplicateSets.size())
        return;
    const DuplicateSet& set = _duplicateSets[_selectedDuplicateSet];
    if (set.members.size() < 2)
        return;
    if (member <= 0)
        member = 1;
    member = qBound(1, member, set.members.size() - 1);
    _selectedSetMember = member;
    {
        const QSignalBlocker blockMemberSelection(ui->duplicateSetMembers);
        ui->duplicateSetMembers->setCurrentRow(member);
    }

    const int reference = set.members.first();
    const int candidate = set.members[member];
    if (const MatchedVideoPair* direct = directEligiblePair(reference, candidate)) {
        displayMatchedPair(*direct);
        _currentComparisonIsDirectMatch = true;
        ui->ignoreDuplicatePairButton->setEnabled(true);
        ui->ignoreDuplicatePairButton->setToolTip(
            QStringLiteral("Ignore this direct discovered match and rebuild duplicate sets."));
        ui->duplicateSetEvidence->setText(QStringLiteral("Direct discovered match"));
    }
    else {
        const VideoPairMatchResult result = VideoPairMatcher::match(*_videos[reference], *_videos[candidate],
                                                                    VideoPairMatcher::configFromPrefs(_prefs));
        displayMatchedPair({reference, candidate, 0, result.phashSimilarity, result.ssimSimilarity});
        _currentComparisonIsDirectMatch = false;
        ui->ignoreDuplicatePairButton->setEnabled(false);
        ui->ignoreDuplicatePairButton->setToolTip(QStringLiteral(
            "This is a linked-only comparison; these videos have no direct discovered match to ignore."));
        ui->duplicateSetEvidence->setText(QStringLiteral("Linked through other members — not a direct match"));
    }
}

void Comparison::on_duplicateSets_currentRowChanged(int row)
{
    selectDuplicateSet(row);
}

void Comparison::on_duplicateSetMembers_currentRowChanged(int row)
{
    showSetMember(row);
}

void Comparison::setManualComparisonActionsEnabled(bool enabled)
{
    ui->leftImage->setEnabled(enabled);
    ui->rightImage->setEnabled(enabled);
    ui->leftFileName->setEnabled(enabled);
    ui->rightFileName->setEnabled(enabled);
    ui->leftMove->setEnabled(enabled);
    ui->rightMove->setEnabled(enabled);
    ui->leftDelete->setEnabled(enabled);
    ui->rightDelete->setEnabled(enabled);
    ui->swapFilenames->setEnabled(enabled);
    ui->prevVideo->setEnabled(enabled);
    ui->nextVideo->setEnabled(enabled);
    ui->ignoreDuplicatePairButton->setEnabled(enabled && _currentComparisonIsDirectMatch);
}

void Comparison::clearManualComparisonDisplay()
{
    ui->leftImage->clear();
    ui->rightImage->clear();
    ui->leftFileName->clear();
    ui->rightFileName->clear();
    ui->leftPathName->clear();
    ui->rightPathName->clear();
    ui->leftFileSize->clear();
    ui->rightFileSize->clear();
    ui->leftDuration->clear();
    ui->rightDuration->clear();
    ui->leftModified->clear();
    ui->rightModified->clear();
    ui->leftResolution->clear();
    ui->rightResolution->clear();
    ui->leftFrameRate->clear();
    ui->rightFrameRate->clear();
    ui->leftBitRate->clear();
    ui->rightBitRate->clear();
    ui->leftCodec->clear();
    ui->rightCodec->clear();
    ui->leftAudio->clear();
    ui->rightAudio->clear();
    ui->leftFileCreated->clear();
    ui->rightFileCreated->clear();
    ui->leftGpsCoordinates->clear();
    ui->rightGpsCoordinates->clear();
    ui->textEdit_leftMetadata->clear();
    ui->textEdit_rightMetadata->clear();
    ui->identicalBits->clear();
}

bool Comparison::hasActiveManualComparison() const
{
    return ui->leftDelete->isEnabled();
}

bool Comparison::navigateForwardFrom(int64_t currentPosition)
{
    int64_t cursor = currentPosition;
    while (const auto candidate = _backgroundDiscovery->nextCandidateAfter(cursor)) {
        if (isPairStillDisplayable(*candidate)) {
            displayMatchedPair(*candidate);
            return true;
        }
        cursor = candidate->position;
    }

    const int64_t firstUncheckedPosition = qMax(currentPosition + 1, _backgroundDiscovery->preScannedEnd() + 1);
    return navigateToNextMatch(firstUncheckedPosition);
}

// Foreground fallback for navigating beyond the contiguous range already
// covered by background discovery. It may duplicate work that a worker is
// processing out of order, but avoids making manual navigation wait for or
// coordinate with the background scan.
bool Comparison::navigateToNextMatch(int64_t fromPosition)
{
    if (fromPosition < 1 || fromPosition > _maxComparisons)
        return false;

    QProgressDialog progress("Searching for next pair", QString(), progressBarValue(fromPosition),
                             progressBarValue(_maxComparisons), this);
    progress.setWindowModality(Qt::WindowModal);

    const auto config = VideoPairMatcher::configFromPrefs(_prefs);
    auto cursor = VideoPairSpace::pairAtPosition(_videos.size(), fromPosition);
    QElapsedTimer progressRefreshTimer;
    progressRefreshTimer.start();
    while (true) {
        const auto result = VideoPairMatcher::match(*_videos[cursor.left], *_videos[cursor.right], config);
        if (result.matches) {
            const MatchedVideoPair pair = {cursor.left, cursor.right, cursor.position, result.phashSimilarity,
                                           result.ssimSimilarity};
            if (isPairStillDisplayable(pair)) {
                displayMatchedPair(pair);
                return true;
            }
        }

        if (progressRefreshTimer.hasExpired(PROGRESS_REFRESH_INTERVAL_MS)) {
            progress.setValue(progressBarValue(cursor.position));
            progressRefreshTimer.restart();
        }

        if (cursor.position == _maxComparisons)
            break;
        VideoPairSpace::advancePair(_videos.size(), cursor);
    }
    return false;
}

// Foreground fallback for the gap between the current position and the
// contiguous range already covered by background discovery. As above, this
// deliberately stays independent from any background work on the same pairs.
bool Comparison::navigateToPrevMatch(int64_t fromPosition, int64_t throughPosition)
{
    if (fromPosition < throughPosition || fromPosition < 1)
        return false;

    QProgressDialog progress("Searching for previous pair", QString(), 0,
                             progressBarValue(fromPosition - throughPosition + 1), this);
    progress.setWindowModality(Qt::WindowModal);

    const auto config = VideoPairMatcher::configFromPrefs(_prefs);
    auto cursor = VideoPairSpace::pairAtPosition(_videos.size(), fromPosition);
    QElapsedTimer progressRefreshTimer;
    progressRefreshTimer.start();
    while (true) {
        const auto result = VideoPairMatcher::match(*_videos[cursor.left], *_videos[cursor.right], config);
        if (result.matches) {
            const MatchedVideoPair pair = {cursor.left, cursor.right, cursor.position, result.phashSimilarity,
                                           result.ssimSimilarity};
            if (isPairStillDisplayable(pair)) {
                displayMatchedPair(pair);
                return true;
            }
        }

        if (progressRefreshTimer.hasExpired(PROGRESS_REFRESH_INTERVAL_MS)) {
            progress.setValue(progressBarValue(fromPosition - cursor.position + 1));
            progressRefreshTimer.restart();
        }

        if (cursor.position == throughPosition)
            break;
        VideoPairSpace::retreatPair(_videos.size(), cursor);
    }
    return false;
}

// Discovery records visual matches only. Apply these cheaper, mutable filters
// when a sparse candidate is about to be shown: files can be removed and pairs
// ignored while discovery is running, and changing the name filter should not
// require rescanning the full pair space.
bool Comparison::isPairStillDisplayable(const MatchedVideoPair& pair) const
{
    const auto* left = _videos[pair.left];
    const auto* right = _videos[pair.right];
    if (!QFileInfo::exists(left->_filePathName) || left->trashed)
        return false;
    if (!QFileInfo::exists(right->_filePathName) || right->trashed)
        return false;
    if (Db(_prefs.cacheFilePathName()).isPairToIgnore(left->_filePathName, right->_filePathName))
        return false;
    if (ui->settingNamesInAnotherCheckbox->isChecked()
        && whichFilenameContainsTheOther(left->_filePathName, right->_filePathName) == NOT_CONTAINED)
        return false;
    return true;
}

void Comparison::displayMatchedPair(const MatchedVideoPair& pair)
{
    _leftVideo = pair.left;
    _rightVideo = pair.right;
    _phashSimilarity = pair.phashSimilarity;
    _ssimSimilarity = pair.ssimSimilarity;
    showVideo(QStringLiteral("left"));
    showVideo(QStringLiteral("right"));
    highlightBetterProperties();
    updateUI();
    _currentComparisonIsDirectMatch = true;
    ui->ignoreDuplicatePairButton->setEnabled(true);
    ui->ignoreDuplicatePairButton->setToolTip(
        QStringLiteral("Mark this direct pair as not duplicates and save it to the cache."));
}

void Comparison::showVideo(const QString& side)
{
    int thisVideo = _leftVideo;
    if (side == "right")
        thisVideo = _rightVideo;

    auto* Image = this->findChild<ClickableLabel*>(side + QStringLiteral("Image"));
    QBuffer pixels(&_videos[thisVideo]->thumbnail);
    QImage image;
    image.load(&pixels, QByteArrayLiteral("JPG"));
    Image->setPixmap(QPixmap::fromImage(image).scaled(Image->width(), Image->height(), Qt::KeepAspectRatio));

#ifdef Q_OS_MACOS
    if (_videos[thisVideo]->_filePathName.contains(".photoslibrary"))
        lookUpApplePhotosName(thisVideo);
#endif

    auto* FileName = this->findChild<ClickableLabel*>(side + QStringLiteral("FileName"));
    if (_videos[thisVideo]->nameInApplePhotos.isEmpty())
        FileName->setText(QFileInfo(_videos[thisVideo]->_filePathName).fileName());
    else
        FileName->setText(_videos[thisVideo]->nameInApplePhotos);
    FileName->setToolTip(
        QStringLiteral("%1\nOpen in file manager").arg(QDir::toNativeSeparators(_videos[thisVideo]->_filePathName)));

    QFileInfo videoFile(_videos[thisVideo]->_filePathName);
    auto* PathName = this->findChild<QLabel*>(side + QStringLiteral("PathName"));
    PathName->setText(QDir::toNativeSeparators(videoFile.absolutePath()));

    auto* FileSize = this->findChild<QLabel*>(side + QStringLiteral("FileSize"));
    FileSize->setText(readableFileSize(_videos[thisVideo]->size));

    auto* Duration = this->findChild<QLabel*>(side + QStringLiteral("Duration"));
    Duration->setText(readableDuration(_videos[thisVideo]->duration));

    auto* Modified = this->findChild<QLabel*>(side + QStringLiteral("Modified"));
    Modified->setText(_videos[thisVideo]->modified.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));

    // File create date
    if (side == "left") {
        this->ui->leftFileCreated->setText(
            _videos[thisVideo]->_fileCreateDate.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));
    }
    else {
        this->ui->rightFileCreated->setText(
            _videos[thisVideo]->_fileCreateDate.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));
    }

    const QString resolutionString =
        QStringLiteral("%1x%2").arg(_videos[thisVideo]->width).arg(_videos[thisVideo]->height);
    auto* Resolution = this->findChild<QLabel*>(side + QStringLiteral("Resolution"));
    Resolution->setText(resolutionString);

    auto* FrameRate = this->findChild<QLabel*>(side + QStringLiteral("FrameRate"));
    const double fps = _videos[thisVideo]->framerate;
    if (fps == 0.0)
        FrameRate->clear();
    else
        FrameRate->setText(QStringLiteral("%1 FPS").arg(fps));

    auto* BitRate = this->findChild<QLabel*>(side + QStringLiteral("BitRate"));
    BitRate->setText(readableBitRate(_videos[thisVideo]->bitrate));

    auto* Codec = this->findChild<QLabel*>(side + QStringLiteral("Codec"));
    Codec->setText(_videos[thisVideo]->codec);

    auto* Audio = this->findChild<QLabel*>(side + QStringLiteral("Audio"));
    Audio->setText(_videos[thisVideo]->audio);

    auto* GpsCoordinatesLabel = this->findChild<QLabel*>(side + QStringLiteral("GpsCoordinates"));
    GpsCoordinatesLabel->setText(
        _videos[thisVideo]->meta.gpsCoordinates); // set even when empty to clear previous comparison

    auto* metadata = this->findChild<QTextEdit*>(QStringLiteral("textEdit_%1Metadata").arg(side));
    if (metadata) {
        metadata->clear();
        for (auto it = _videos[thisVideo]->meta.additionalMetadata.cbegin();
             it != _videos[thisVideo]->meta.additionalMetadata.cend(); ++it)
            metadata->append(QStringLiteral("%1: %2").arg(it.key(), it.value()));
    }
}

#ifdef Q_OS_MACOS
// AppleScript was too slow to run during scanning, so names are still resolved
// only when a Photos video is shown. PhotoKit is milliseconds; if this stays
// reliable in use, move the lookup into Video's initial metadata extraction.
void Comparison::lookUpApplePhotosName(const int videoIndex)
{
    Video* video = _videos[videoIndex];
    const QString filePathname = video->_filePathName;
    const QString mediaId = QFileInfo(filePathname).completeBaseName();
    // Photos names an original after its asset UUID, except for the video part of
    // a Live Photo which gets an underscore suffix. Those have no asset of their
    // own to look up, and the delete path refuses them as not being real videos,
    // so skip quietly rather than reporting a lookup failure to the user.
    if (mediaId.contains("_"))
        return;

    if (!video->nameInApplePhotos.isEmpty())
        return;

    // A PhotoKit lookup of a single asset takes a few milliseconds, so it runs
    // inline: the caller displays the name right after, with no interim state.
    const QString name = _applePhotosNameLookup(mediaId);
    video->nameInApplePhotos.clear();

    if (name.contains(OBJ_C_NO_PHOTOS_ACCESS_STRING)) {
        emit sendStatusMessage(QString("Cannot read Apple Photos names: access to the library was refused. "
                                       "Grant it in System Settings > Privacy & Security > Photos, "
                                       "otherwise videos keep the internal name Photos gave them."));
        return;
    }

    if (name.isEmpty() || name.contains(OBJ_C_FAILURE_STRING)) {
        emit sendStatusMessage(QString("Could not get the name of %1 from Apple Photos. "
                                       "Names can only be read from the System Photo Library, "
                                       "so this is expected for videos kept in another library.")
                                   .arg(filePathname));
        return;
    }

    video->nameInApplePhotos = name;
}

#endif

QString Comparison::readableDuration(const int64_t& milliseconds) const
{
    if (milliseconds == 0)
        return QString("");

    const int hours = milliseconds / (1000 * 60 * 60) % 24;
    const int minutes = milliseconds / (1000 * 60) % 60;
    const int seconds = milliseconds / 1000 % 60;

    QString readableDuration;
    if (hours > 0)
        readableDuration = QStringLiteral("%1h").arg(hours);
    if (minutes > 0)
        readableDuration = QStringLiteral("%1%2m").arg(readableDuration).arg(minutes);
    if (seconds > 0)
        readableDuration = QStringLiteral("%1%2s").arg(readableDuration).arg(seconds);

    return readableDuration;
}

QString Comparison::readableFileSize(const int64_t& filesize) const
{
    //FileSizes are in bytes
    if (filesize < 1024 * 1024)
        return (QStringLiteral("%1 kB").arg(QString::number(filesize / 1024.0, 'i', 0))); //even kBs
    else if (filesize < 1024 * 1024 * 1024) //larger files have one decimal point
        return QStringLiteral("%1 MB").arg(QString::number(filesize / (1024.0 * 1024.0), 'f', 1));
    else
        return QStringLiteral("%1 GB").arg(QString::number(filesize / (1024.0 * 1024.0 * 1024.0), 'f', 1));
}

QString Comparison::readableBitRate(const double& kbps) const
{
    if (kbps == 0.0)
        return QString("");
    return QStringLiteral("%1 kb/s").arg(kbps);
}

void Comparison::highlightBetterProperties() const
{
    ui->leftFileSize->setStyleSheet(QString(""));
    ui->rightFileSize->setStyleSheet(QString("")); //both filesizes within 100 kB
    if (qAbs(_videos[_leftVideo]->size - _videos[_rightVideo]->size) <= FILE_SIZE_BYTES_DIFF_STILL_EQUALS) {
        ui->leftFileSize->setStyleSheet(TEXT_STYLE_ORANGE);
        ui->rightFileSize->setStyleSheet(TEXT_STYLE_ORANGE);
    }
    else if (_videos[_leftVideo]->size > _videos[_rightVideo]->size)
        ui->leftFileSize->setStyleSheet(TEXT_STYLE_GREEN);
    else if (_videos[_leftVideo]->size < _videos[_rightVideo]->size)
        ui->rightFileSize->setStyleSheet(TEXT_STYLE_GREEN);

    ui->leftDuration->setStyleSheet(QString(""));
    ui->rightDuration->setStyleSheet(QString("")); //both runtimes within 1 second
    if (qAbs(_videos[_leftVideo]->duration - _videos[_rightVideo]->duration) <= VIDEO_DURATION_STILL_EQUALS_MS) {
        ui->leftDuration->setStyleSheet(TEXT_STYLE_ORANGE);
        ui->rightDuration->setStyleSheet(TEXT_STYLE_ORANGE);
    }
    else if (_videos[_leftVideo]->duration > _videos[_rightVideo]->duration)
        ui->leftDuration->setStyleSheet(TEXT_STYLE_GREEN);
    else if (_videos[_leftVideo]->duration < _videos[_rightVideo]->duration)
        ui->rightDuration->setStyleSheet(TEXT_STYLE_GREEN);

    ui->leftBitRate->setStyleSheet(QString(""));
    ui->rightBitRate->setStyleSheet(QString(""));
    if (qAbs(_videos[_leftVideo]->bitrate - _videos[_rightVideo]->bitrate)
        <= BITRATE_DIFF_STILL_EQUAL_kbs) //leave some margin due to decoding error
    {
        ui->leftBitRate->setStyleSheet(TEXT_STYLE_ORANGE);
        ui->rightBitRate->setStyleSheet(TEXT_STYLE_ORANGE);
    }
    else if (_videos[_leftVideo]->bitrate > _videos[_rightVideo]->bitrate)
        ui->leftBitRate->setStyleSheet(TEXT_STYLE_GREEN);
    else if (_videos[_leftVideo]->bitrate < _videos[_rightVideo]->bitrate)
        ui->rightBitRate->setStyleSheet(TEXT_STYLE_GREEN);

    ui->leftFrameRate->setStyleSheet(QString(""));
    ui->rightFrameRate->setStyleSheet(QString("")); //both framerates within 0.1 fps
    if (qAbs(_videos[_leftVideo]->framerate - _videos[_rightVideo]->framerate) <= 0.1) {
        ui->leftFrameRate->setStyleSheet(TEXT_STYLE_ORANGE);
        ui->rightFrameRate->setStyleSheet(TEXT_STYLE_ORANGE);
    }
    else if (_videos[_leftVideo]->framerate > _videos[_rightVideo]->framerate)
        ui->leftFrameRate->setStyleSheet(TEXT_STYLE_GREEN);
    else if (_videos[_leftVideo]->framerate < _videos[_rightVideo]->framerate)
        ui->rightFrameRate->setStyleSheet(TEXT_STYLE_GREEN);

    // Set file modified date
    ui->leftModified->setStyleSheet(QString(""));
    ui->rightModified->setStyleSheet(QString(""));
    if (_videos[_leftVideo]->modified == _videos[_rightVideo]->modified) {
        ui->leftModified->setStyleSheet(TEXT_STYLE_ORANGE);
        ui->rightModified->setStyleSheet(TEXT_STYLE_ORANGE);
    }
    else if (_videos[_leftVideo]->modified < _videos[_rightVideo]->modified)
        ui->leftModified->setStyleSheet(TEXT_STYLE_GREEN);
    else if (_videos[_leftVideo]->modified > _videos[_rightVideo]->modified)
        ui->rightModified->setStyleSheet(TEXT_STYLE_GREEN);

    // Set file create date (earlier is better, ie green)
    ui->leftFileCreated->setStyleSheet(QString(""));
    ui->rightFileCreated->setStyleSheet(QString(""));
    if (_videos[_leftVideo]->_fileCreateDate == _videos[_rightVideo]->_fileCreateDate) {
        ui->leftFileCreated->setStyleSheet(TEXT_STYLE_ORANGE);
        ui->rightFileCreated->setStyleSheet(TEXT_STYLE_ORANGE);
    }
    else if (_videos[_leftVideo]->_fileCreateDate < _videos[_rightVideo]->_fileCreateDate)
        ui->leftFileCreated->setStyleSheet(TEXT_STYLE_GREEN);
    else if (_videos[_leftVideo]->_fileCreateDate > _videos[_rightVideo]->_fileCreateDate)
        ui->rightFileCreated->setStyleSheet(TEXT_STYLE_GREEN);

    // Set resolution
    ui->leftResolution->setStyleSheet(QString(""));
    ui->rightResolution->setStyleSheet(QString(""));

    if (_videos[_leftVideo]->width * _videos[_leftVideo]->height
        == _videos[_rightVideo]->width * _videos[_rightVideo]->height)
    {
        ui->leftResolution->setStyleSheet(TEXT_STYLE_ORANGE);
        ui->rightResolution->setStyleSheet(TEXT_STYLE_ORANGE);
    }
    else if (_videos[_leftVideo]->width * _videos[_leftVideo]->height
             > _videos[_rightVideo]->width * _videos[_rightVideo]->height)
        ui->leftResolution->setStyleSheet(TEXT_STYLE_GREEN);
    else if (_videos[_leftVideo]->width * _videos[_leftVideo]->height
             < _videos[_rightVideo]->width * _videos[_rightVideo]->height)
        ui->rightResolution->setStyleSheet(TEXT_STYLE_GREEN);

    // show if video codecs are the same
    ui->leftCodec->setStyleSheet("");
    ui->rightCodec->setStyleSheet("");
    if (_videos[_leftVideo]->codec.localeAwareCompare(_videos[_rightVideo]->codec) == 0) {
        ui->leftCodec->setStyleSheet(TEXT_STYLE_ORANGE);
        ui->rightCodec->setStyleSheet(TEXT_STYLE_ORANGE);
    }

    // show if audio codecs are the same
    ui->leftAudio->setStyleSheet("");
    ui->rightAudio->setStyleSheet("");
    if (_videos[_leftVideo]->audio.localeAwareCompare(_videos[_rightVideo]->audio) == 0) {
        ui->leftAudio->setStyleSheet(TEXT_STYLE_ORANGE);
        ui->rightAudio->setStyleSheet(TEXT_STYLE_ORANGE);
    }

    auto showGps = false;
    if (!this->ui->leftGpsCoordinates->text().isEmpty() || !this->ui->rightGpsCoordinates->text().isEmpty())
        showGps = true; // as soon as one has gps, show the data and labels for both
    this->ui->labelLeftGps->setVisible(showGps);
    this->ui->leftGpsCoordinates->setVisible(showGps);
    this->ui->labelRightGps->setVisible(showGps);
    this->ui->rightGpsCoordinates->setVisible(showGps);
    // now hightligh if only one has gps coordinates
    ui->leftGpsCoordinates->setStyleSheet(QString(""));
    ui->rightGpsCoordinates->setStyleSheet(QString(""));
    if (!ui->leftGpsCoordinates->text().isEmpty() && ui->rightGpsCoordinates->text().isEmpty())
        ui->leftGpsCoordinates->setStyleSheet(TEXT_STYLE_GREEN);
    else if (ui->leftGpsCoordinates->text().isEmpty() && !ui->rightGpsCoordinates->text().isEmpty())
        ui->rightGpsCoordinates->setStyleSheet(TEXT_STYLE_GREEN);
}

void Comparison::updateUI()
{
    setManualComparisonActionsEnabled(true);
    if (ui->leftPathName->text() == ui->rightPathName->text()) //gray out move button if both videos in same folder
    {
        ui->leftMove->setDisabled(true);
        ui->rightMove->setDisabled(true);
    }
    else {
        ui->leftMove->setDisabled(false);
        ui->rightMove->setDisabled(false);
    }

    if (this->_prefs.comparisonMode() == Prefs::_PHASH)
        ui->identicalBits->setText(QString("%1/64 same bits").arg(_phashSimilarity));
    if (this->_prefs.comparisonMode() == Prefs::_SSIM)
        ui->identicalBits->setText(QString("%1 SSIM index").arg(QString::number(qMin(_ssimSimilarity, 1.0), 'f', 3)));
    _zoomLevel = 0;
    if (ui->tabWidget->currentWidget() != ui->tabManual)
        ui->progressBar->setValue(progressBarValue(comparisonsSoFar()));
}

int64_t Comparison::comparisonsSoFar() const
{
    // Before the first match there is no current pair yet.
    if (_leftVideo == _rightVideo) {
        Q_ASSERT(_leftVideo == 0);
        return 0;
    }

    // Automatic cleanup increments the right index once beyond the list after
    // completing a row. Report the final pair in that row until the indexes are
    // moved to the next valid pair.
    if (_rightVideo == _videos.size()) {
        if (_leftVideo >= _videos.size() - 1)
            return _maxComparisons;
        return VideoPairSpace::positionForPair(_videos.size(), _leftVideo, _videos.size() - 1);
    }

    return VideoPairSpace::positionForPair(_videos.size(), _leftVideo, _rightVideo);
}

int Comparison::progressBarValue(int64_t comparisons) const
{
    // Qt progress bars use int for their range values (max INT_MAX ~2.1 billion)
    // For large file counts, scale down to fit within int range
    if (_maxComparisons <= INT_MAX)
        return (int)comparisons;
    // Scale down proportionally to fit in int range
    return int((double(INT_MAX) / _maxComparisons) * comparisons);
}

void Comparison::seekFromSliderPosition(int sliderValue)
{
    // we want to resume from the theoretical pair at "position",
    // video 1: compared to video 2, 3, 4, 5
    // video 2: compared to video 3, 4, 5
    // video 3: compared to video 4, 5
    // video 4: compared to video 5
    // Overal 5 videos means 5*(5-1)/2 = 5*4/2 = 10 possible pairs

    // total comparisons n * (n-1) / 2
    // remaining comparisons at vid a: a * (a-1) / 2, with other video being from 1 to total - a - 1
    // want to find a such that x within the range of [a * (a-1) / 2 to (a+1) * (a+1-1) / 2 [
    // can do with binary search
    if (_prefs._numberOfVideos < 2 || _videos.size() < 2)
        return;

    // Convert slider value back to actual target if we had to scale down
    int64_t target;
    if (_maxComparisons <= INT_MAX)
        target = sliderValue;
    else // Reverse the scaling
        target = (double(_maxComparisons) / INT_MAX) * sliderValue;

    target = qBound<int64_t>(1, target, _maxComparisons);
    _seekForwards = true;
    if (!navigateForwardFrom(target - 1))
        confirmToExit();
}

void Comparison::onProgressSliderReleased()
{
    seekFromSliderPosition(ui->progressBar->sliderPosition());
}

void Comparison::openFileManager(const QString& filename)
{
#ifdef Q_OS_WIN
    // TODO : for UWP, can't use process, so maybe change behavior to open folder, without file already selected :
    // https://stackoverflow.com/questions/48243245/qdesktopservicesopenurl-cannot-open-directory-in-mac-finder
    QProcess::startDetached("explorer", {"/select,", QDir::toNativeSeparators(filename)});
#elif defined(Q_OS_MACOS)
    if (!filename.contains(".photoslibrary")) {
        QProcess::startDetached("open", QStringList() << "-R" << filename);
    }
    else {
        const QString fileNameNoExt = QFileInfo(filename).completeBaseName();
        QString returnValue =
            QString::fromLocal8Bit(Obj_C::obj_C_revealMediaInPhotosApp(fileNameNoExt.toLocal8Bit().data()));
        if (!returnValue.contains(OBJ_C_SUCCESS_STRING)) {
            QMessageBox::information(this, "",
                                     QString("Unknown error revealing in Apple Photos Library album, sorry. "
                                             "\nInstead, will open in file manager. "
                                             "\n\nError:%1")
                                         .arg(returnValue));
            QProcess::startDetached("open", QStringList() << "-R" << filename);
        }
    }
#elif defined(Q_OS_X11)
    QProcess::startDetached(QStringLiteral("xdg-open \"%1\"").arg(filename.left(filename.lastIndexOf("/"))));
#endif
}

void Comparison::on_leftFileName_clicked()
{
    if (!hasActiveManualComparison())
        return;
    openFileManager(_videos[_leftVideo]->_filePathName);
}

void Comparison::on_rightFileName_clicked()
{
    if (!hasActiveManualComparison())
        return;
    openFileManager(_videos[_rightVideo]->_filePathName);
}

void Comparison::on_leftImage_clicked()
{
    if (!hasActiveManualComparison())
        return;
    openMedia(_videos[_leftVideo]->_filePathName);
}

void Comparison::on_rightImage_clicked()
{
    if (!hasActiveManualComparison())
        return;
    openMedia(_videos[_rightVideo]->_filePathName);
}

void Comparison::openMedia(const QString filename)
{
#ifdef Q_OS_MACOS
    if (!filename.contains(".photoslibrary")) {
#endif
        QDesktopServices::openUrl(QUrl::fromLocalFile(filename));
#ifdef Q_OS_MACOS
    }
    else {
        const QString fileNameNoExt = QFileInfo(filename).completeBaseName();
        QString returnValue =
            QString::fromLocal8Bit(Obj_C::obj_C_revealMediaInPhotosApp(fileNameNoExt.toLocal8Bit().data()));
        if (!returnValue.contains(OBJ_C_SUCCESS_STRING)) {
            QMessageBox::information(this, "",
                                     QString("Unknown error revealing in Apple Photos Library album, sorry. "
                                             "\nInstead, will open in default player manager. "
                                             "\n\nError:%1")
                                         .arg(returnValue));
            QDesktopServices::openUrl(QUrl::fromLocalFile(filename));
        }
    }
#endif
}

void Comparison::deleteVideo(const int& side, const bool auto_trash_mode)
{
    const QString filename = _videos[side]->_filePathName;
    const QString onlyFilename = filename.right(filename.length() - filename.lastIndexOf("/") - 1);

    // find if it is the elft or right video in ui to tell used in trash confirmation
    QString videoSide = "left";
    if (side == _rightVideo)
        videoSide = "right";

    if (_videos[side]->trashed || !QFileInfo::exists(filename)) //video was already manually deleted, skip to next
    {
        if (!auto_trash_mode)
            rebuildDuplicateSets();
        if (!auto_trash_mode && ui->tabWidget->currentWidget() == ui->tabManual && _selectedDuplicateSet >= 0)
            return;
        _seekForwards ? on_nextVideo_clicked() : on_prevVideo_clicked();
        return;
    }
    QString question;
    switch (_prefs.delMode) {
    case Prefs::STANDARD_TRASH:
        question =
            QString("Are you sure you want to move the %1 file to trash?\n\n%2")
                .arg(videoSide) //show if it is the left or right file
                .arg(_videos[side]->nameInApplePhotos.isEmpty() ? onlyFilename : _videos[side]->nameInApplePhotos);
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
    if (ui->disableDeleteConfirmationCheckbox->isChecked()
        || QMessageBox::question(this, "Delete file", question, QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
    {
        // check if file is in locked folder set by user
        if (isFileInProtectedFolder(filename)) {
            if (!auto_trash_mode)
                QMessageBox::information(this, "", "This file is locked, cannot delete !");
            else
                emit sendStatusMessage(QString("Skipped %1 as it is locked.").arg(QDir::toNativeSeparators(filename)));
            // no need to seek as in auto trash mode, the seeking is already handled, and manual will not want to seek
            return;
        }
#ifdef Q_OS_MACOS
        // we must never delete files from the Apple Photos Library, although we can detect them !
        else if (filename.contains(".photoslibrary")) {
            if (!filename.contains(".photoslibrary/originals/")) {
                if (!auto_trash_mode)
                    QMessageBox::information(this, "",
                                             QString("The file is a derivative media created by Apple Photos. "
                                                     "It shouldn't have been detected in the first place, sorry. "
                                                     "It must and will not be deleted."));
                emit sendStatusMessage(QString("Error, file %1 was an Apple Photos Library derivative not an original.")
                                           .arg(QDir::toNativeSeparators(filename)));
                return;
            }
            else { // only video in subfolder originals are true videos
                // We'll now tell Apple Photos via AppleScript to add videos to be deleted to a specific album so the user can manually delete them all at once
                const QString fileNameNoExt = QFileInfo(filename).completeBaseName();
                // TODO : if contains _ then video is probably a live photo media, so should not modify it ! -> should preferably discard at scan time... ?
                if (fileNameNoExt.contains("_")) {
                    if (!auto_trash_mode)
                        QMessageBox::information(this, "",
                                                 "This video is in an Apple Photos Libray, and seems to be from a Live "
                                                 "Photo, not a real video. \n"
                                                 "You should use duplicate photo scanners to deal with it.");
                    emit sendStatusMessage(QString("Did not add %1 into Apple Photos Library album : it seems to be a "
                                                   "live photo, so deal with it as a photo.")
                                               .arg(QDir::toNativeSeparators(filename)));
                    return;
                }
                else { // only videos that aren't live photos
                    QString returnValue = QString::fromLocal8Bit(Obj_C::obj_C_addMediaToAlbum(
                        QString(APP_NAME).toLocal8Bit().data(), fileNameNoExt.toLocal8Bit().data()));
                    if (!returnValue.contains(OBJ_C_SUCCESS_STRING)) {
                        if (!auto_trash_mode)
                            QMessageBox::information(
                                this, "",
                                QString("Unknown error adding into Apple Photos Library album, sorry. "
                                        "Video might be in Apple Photos trash. "
                                        "Make sure to empty Apple Photos trash."
                                        "\n\nError:%1")
                                    .arg(returnValue));
                        emit sendStatusMessage(QString("Unknown error adding %1 into Apple Photos Library album.")
                                                   .arg(QDir::toNativeSeparators(filename)));
                    }
                    // Finally if reached here: it is "deleted" so remove from DB
                    // NB : we only delete the file from the disk, and not from _videos, as we check
                    //      when going to the next/prev video that each exists, or skip it.
                    _someWereMovedInApplePhotosLibrary =
                        true; // used to check at the very end, to display reminder message to user
                    _videos[side]->trashed =
                        true; // could check simply if file still exists on disk but not in case of Apple Photos...
                    _videosDeleted++;
                    _spaceSaved = _spaceSaved + _videos[side]->size;

                    ui->trashedFiles->setVisible(true);
                    ui->trashedFiles->setText(QStringLiteral("Moved %1 to trash").arg(_videosDeleted));
                    emit sendStatusMessage(QString("Moved %1 to album 'Trash from %2' of Apple Photos Library")
                                               .arg(QDir::toNativeSeparators(filename), APP_NAME));

                    Db(_prefs.cacheFilePathName())
                        .removeVideo(filename); // remove it from the cache as it is not needed anymore !
                    if (!auto_trash_mode) {     // in auto trash mode, the seeking is already handled
                        rebuildDuplicateSets();
                        if (ui->tabWidget->currentWidget() != ui->tabManual)
                            _seekForwards ? on_nextVideo_clicked() : on_prevVideo_clicked();
                    }
                    return;
                }
            }
        }
#endif
        else {
            if (_prefs.delMode == Prefs::DIRECT_DELETION) {
                if (!QFile::remove(filename)) {
                    if (!auto_trash_mode)
                        QMessageBox::information(this, "", "Could not delete. Check file permissions");
                    else
                        emit sendStatusMessage(
                            QString("Error deleting video %1").arg(QDir::toNativeSeparators(filename)));
                    return;
                }
            }
            else if (_prefs.delMode == Prefs::CUSTOM_TRASH) { // otherwise we move to the custom folder selected by user
                const auto customTrashFolder = _prefs.customTrashFolder();
                if (!customTrashFolder.exists()) {
                    if (!auto_trash_mode)
                        QMessageBox::information(this, "", "Selected folder to move files into doesn't seem to exist");
                    else
                        emit sendStatusMessage(
                            QString("Error moving to selected folder, it doesn't seem to exist for video %1")
                                .arg(QDir::toNativeSeparators(filename)));
                    return;
                }
                else { // the destination directory does exist
                    QFileInfo newFileInfo(customTrashFolder, QFileInfo(filename).fileName());
                    if (newFileInfo.exists()) // create random name to make sure it doesn't exist
                        newFileInfo.setFile(customTrashFolder,
                                            QFileInfo(filename).completeBaseName() + "-"
                                                + QUuid::createUuid().toString().remove("{").remove("}") + "."
                                                + QFileInfo(filename).suffix());
                    // rename actually moves to new path !
                    if (!QFile(filename).rename(newFileInfo.absoluteFilePath())) {
                        if (!auto_trash_mode)
                            QMessageBox::information(this, "",
                                                     "Could not move file to selected folder. Check file permissions.");
                        else
                            emit sendStatusMessage(
                                QString("Error moving to selected folder, check file permissions of video %1")
                                    .arg(QDir::toNativeSeparators(filename)));
                        return;
                    }
                }
            }
            else { // meaning _prefs.delMode==Prefs::STANDARD_TRASH
                if (!QFile::moveToTrash(filename)) {
                    if (!auto_trash_mode)
                        QMessageBox::information(this, "",
                                                 "Could not move file to trash. Check file permissions, "
                                                 "and if a trash exists in your file system "
                                                 "(eg network locations do not have a trash).\n\n"
                                                 "You could try again with direct deletion enabled, or "
                                                 "with a custom trash folder.");
                    else
                        emit sendStatusMessage(
                            QString("Error moving to trash video %1").arg(QDir::toNativeSeparators(filename)));
                    return;
                }
            }

            // Reaches here if video was successfully handled (except apple photo case)
            // NB : we only delete the file from the disk, and not from _videos, as we check
            //      when going to the next/prev video that each exists, or skip it.
            _videos[side]->trashed =
                true; // could check simply if file still exists on disk but not in case of Apple Photos...
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

            Db(_prefs.cacheFilePathName())
                .removeVideo(filename); // remove it from the cache as it is not needed anymore !
            if (!auto_trash_mode) {     // in auto trash mode, the seeking is already handled
                rebuildDuplicateSets();
                if (ui->tabWidget->currentWidget() != ui->tabManual)
                    _seekForwards ? on_nextVideo_clicked() : on_prevVideo_clicked();
            }
        }
    }
}

void Comparison::on_leftDelete_clicked()
{
    if (hasActiveManualComparison())
        deleteVideo(_leftVideo);
}

void Comparison::on_rightDelete_clicked()
{
    if (hasActiveManualComparison())
        deleteVideo(_rightVideo);
}

void Comparison::on_leftMove_clicked()
{
    if (!hasActiveManualComparison())
        return;
    moveVideo(_videos[_leftVideo]->_filePathName, _videos[_rightVideo]->_filePathName);
}

void Comparison::on_rightMove_clicked()
{
    if (!hasActiveManualComparison())
        return;
    moveVideo(_videos[_rightVideo]->_filePathName, _videos[_leftVideo]->_filePathName);
}

void Comparison::moveVideo(const QString& from, const QString& to)
{
#ifdef Q_OS_MACOS
    if (from.contains(".photoslibrary")) {
        QMessageBox::information(this, "", "This file is in an Apple Photos Library, cannot move !");
        return;
    }
#endif
    if (QMessageBox::question(this, "Move", "This file is in a locked folder, are you sure you want to move it ?",
                              QMessageBox::Yes | QMessageBox::No)
        == QMessageBox::No)
        return;

    if (!QFileInfo::exists(from)) {
        _seekForwards ? on_nextVideo_clicked() : on_prevVideo_clicked();
        return;
    }

    const QString fromPath = from.left(from.lastIndexOf("/"));
    const QString toPath = to.left(to.lastIndexOf("/"));
    const QString question = QString("Are you sure you want to move this file?\n\nFrom: %1\nTo:     %2")
                                 .arg(QDir::toNativeSeparators(fromPath), QDir::toNativeSeparators(toPath));
    if (QMessageBox::question(this, "Move", question, QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        QFile moveThisFile(from);
        const QString destination = QString("%1/%2").arg(toPath, from.right(from.length() - from.lastIndexOf("/") - 1));
        if (!moveThisFile.rename(destination))
            QMessageBox::information(this, "", "Could not move file. Check file permissions and available disk space.");
        else {
            for (Video* video : _videos) {
                if (video->_filePathName == from) {
                    video->_filePathName = destination;
                    break;
                }
            }
            Db(_prefs.cacheFilePathName()).removeVideo(from);
            emit sendStatusMessage(QString("Moved %1 to %2").arg(QDir::toNativeSeparators(from), toPath));
            rebuildDuplicateSets();
            if (ui->tabWidget->currentWidget() != ui->tabManual)
                _seekForwards ? on_nextVideo_clicked() : on_prevVideo_clicked();
        }
    }
}

void Comparison::on_swapFilenames_clicked()
{
    if (!hasActiveManualComparison())
        return;
    const QFileInfo leftVideoFile(_videos[_leftVideo]->_filePathName);
    const QString leftPathname = leftVideoFile.absolutePath();
    const QString oldLeftFilename = leftVideoFile.fileName();
    const QString oldLeftNoExtension = oldLeftFilename.left(oldLeftFilename.lastIndexOf("."));
    const QString leftExtension = oldLeftFilename.right(oldLeftFilename.length() - oldLeftFilename.lastIndexOf("."));

    const QFileInfo rightVideoFile(_videos[_rightVideo]->_filePathName);
    const QString rightPathname = rightVideoFile.absolutePath();
    const QString oldRightFilename = rightVideoFile.fileName();
    const QString oldRightNoExtension = oldRightFilename.left(oldRightFilename.lastIndexOf("."));
    const QString rightExtension =
        oldRightFilename.right(oldRightFilename.length() - oldRightFilename.lastIndexOf("."));

    const QString newLeftFilename = QStringLiteral("%1%2").arg(oldRightNoExtension, leftExtension);
    const QString newLeftPathAndFilename = QStringLiteral("%1/%2").arg(leftPathname, newLeftFilename);

    const QString newRightFilename = QStringLiteral("%1%2").arg(oldLeftNoExtension, rightExtension);
    const QString newRightPathAndFilename = QStringLiteral("%1/%2").arg(rightPathname, newRightFilename);

    QFile leftFile(_videos[_leftVideo]->_filePathName); //rename files
    QFile rightFile(_videos[_rightVideo]->_filePathName);
    leftFile.rename(QStringLiteral("%1/DuplicateRenamedVideo.avi").arg(leftPathname));
    rightFile.rename(newRightPathAndFilename);
    leftFile.rename(newLeftPathAndFilename);

    _videos[_leftVideo]->_filePathName = newLeftPathAndFilename; //update filename in object
    _videos[_rightVideo]->_filePathName = newRightPathAndFilename;

    ui->leftFileName->setText(newLeftFilename); //update UI
    ui->rightFileName->setText(newRightFilename);

    // remove both from cache, otherwise they will be stored in the cache inverted from their full path names
    // TODO : could just rename them in the cache... ?
    Db cache(_prefs.cacheFilePathName()); // opening connexion to database
    cache.removeVideo(oldLeftFilename);
    cache.removeVideo(oldRightFilename);
    rebuildDuplicateSets();
}

void Comparison::on_selectPhash_clicked(const bool& checked)
{
    if (checked) {
        _prefs.comparisonMode(Prefs::_PHASH);
        if (_backgroundDiscovery->hasStarted())
            restartBackgroundDiscovery();
    }
    emit switchComparisonMode(_prefs.comparisonMode());
}

void Comparison::on_selectSSIM_clicked(const bool& checked)
{
    if (checked) {
        _prefs.comparisonMode(Prefs::_SSIM);
        if (_backgroundDiscovery->hasStarted())
            restartBackgroundDiscovery();
    }
    emit switchComparisonMode(_prefs.comparisonMode());
}

void Comparison::on_thresholdSlider_valueChanged(const int& value)
{
    this->_prefs.matchSimilarityThreshold(value);

    _prefs._thresholdSSIM = value / 100.0;
    const int matchingBitsOf64 = static_cast<int>(round(64 * _prefs._thresholdSSIM));
    _prefs._thresholdPhash = matchingBitsOf64;
    this->ui->percentSim->setNum(value);
    this->ui->thresholdSlider->setValue(value);

    const QString thresholdMessage =
        QStringLiteral("Threshold: %1% (%2/64 bits = match)   Default: %3%\n"
                       "Smaller: less strict, can match different videos (false positive)\n"
                       "Larger: more strict, can miss identical videos (false negative)")
            .arg(value)
            .arg(matchingBitsOf64)
            .arg((int)(100 * Prefs::DEFAULT_SSIM_THRESHOLD + 0.5));
    ui->thresholdSlider->setToolTip(thresholdMessage);

    if (_backgroundDiscovery->hasStarted())
        restartBackgroundDiscovery();
    emit adjustThresholdSlider(ui->thresholdSlider->value()); // sync with main window
}

void Comparison::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event)

    if (ui->leftFileName->text().isEmpty() || _leftVideo >= _prefs._numberOfVideos
        || _rightVideo >= _prefs._numberOfVideos)
        return; //automatic initial resize event can happen before closing when values went over limit

    QImage image;
    QBuffer leftPixels(&_videos[_leftVideo]->thumbnail);
    image.load(&leftPixels, QByteArrayLiteral("JPG"));
    ui->leftImage->setPixmap(
        QPixmap::fromImage(image).scaled(ui->leftImage->width(), ui->leftImage->height(), Qt::KeepAspectRatio));
    QBuffer rightPixels(&_videos[_rightVideo]->thumbnail);
    image.load(&rightPixels, QByteArrayLiteral("JPG"));
    ui->rightImage->setPixmap(
        QPixmap::fromImage(image).scaled(ui->rightImage->width(), ui->rightImage->height(), Qt::KeepAspectRatio));
}

void Comparison::wheelEvent(QWheelEvent* event)
{
    const QPoint pos = QCursor::pos();
    if (!QApplication::widgetAt(pos))
        return;
    ClickableLabel* imagePtr;
    if (QApplication::widgetAt(pos)->objectName() == "leftImage")
        imagePtr = ui->leftImage;
    else if (QApplication::widgetAt(pos)->objectName() == "rightImage")
        imagePtr = ui->rightImage;
    else
        return;

    // THEO : pixmap()->xxx didn't seem to work, as imagePtr is a pointer but imagePtr->pixmap() returns the object directly and not a pointer
    const int wmax = imagePtr->mapToGlobal(QPoint(imagePtr->pixmap().width(), 0)).x();          //image right edge
    const int hmax = imagePtr->mapToGlobal(QPoint(0, imagePtr->pixmap().height())).y();         //image bottom edge
    const double ratiox = 1 - static_cast<double>(wmax - pos.x()) / imagePtr->pixmap().width(); //mouse pos inside image
    const double ratioy = 1 - static_cast<double>(hmax - pos.y()) / imagePtr->pixmap().height();

    const int widescreenBlack = (imagePtr->height() - imagePtr->pixmap().height()) / 2;
    const int imgTop = imagePtr->mapToGlobal(QPoint(0, 0)).y() + widescreenBlack;
    const int imgBtm = imgTop + imagePtr->pixmap().height();
    if (pos.x() > wmax || pos.y() < imgTop || pos.y() > imgBtm) //image is smaller than label underneath
        return;

    if (_zoomLevel == 0) //first mouse wheel movement: retrieve actual screen captures in full resolution
    {
        QApplication::setOverrideCursor(Qt::WaitCursor);

        QImage image;
        image = _videos[_leftVideo]->ffmpegLib_captureAt(10);
        ui->leftImage->setPixmap(
            QPixmap::fromImage(image).scaled(ui->leftImage->width(), ui->leftImage->height(), Qt::KeepAspectRatio));
        _leftZoomed = QPixmap::fromImage(image); //keep it in memory
        _leftW = image.width();
        _leftH = image.height();

        image = _videos[_rightVideo]->ffmpegLib_captureAt(10);
        ui->rightImage->setPixmap(
            QPixmap::fromImage(image).scaled(ui->rightImage->width(), ui->rightImage->height(), Qt::KeepAspectRatio));
        _rightZoomed = QPixmap::fromImage(image);
        _rightW = image.width();
        _rightH = image.height();

        _zoomLevel = 1;
        QApplication::restoreOverrideCursor();
        return;
    }

    // THEO : event.delta() stopped working as of QT 5.15, need to use either pixel or angleDelta (check later if y is the correct logic to do here)
    if (event->angleDelta().y() > 0 && _zoomLevel < 10) //mouse wheel up
        _zoomLevel = _zoomLevel * 2;
    if (event->angleDelta().y() < 0 && _zoomLevel > 1) //mouse wheel down
        _zoomLevel = _zoomLevel / 2;

    QPixmap pix;
    pix = _leftZoomed.copy(static_cast<int>(_leftW * ratiox - _leftW * ratiox / _zoomLevel),
                           static_cast<int>(_leftH * ratioy - _leftH * ratioy / _zoomLevel), _leftW / _zoomLevel,
                           _leftH / _zoomLevel);
    ui->leftImage->setPixmap(
        pix.scaled(ui->leftImage->width(), ui->leftImage->height(), Qt::KeepAspectRatio, Qt::FastTransformation));

    pix = _rightZoomed.copy(static_cast<int>(_rightW * ratiox - _rightW * ratiox / _zoomLevel),
                            static_cast<int>(_rightH * ratioy - _rightH * ratioy / _zoomLevel), _rightW / _zoomLevel,
                            _rightH / _zoomLevel);
    ui->rightImage->setPixmap(
        pix.scaled(ui->rightImage->width(), ui->rightImage->height(), Qt::KeepAspectRatio, Qt::FastTransformation));
}

// ------------------------------------------------------------------------
// ------------------ Locked folders functions ----------------------------

void Comparison::loadLockedFolderFromPrefs()
{
    foreach (QString folderPath, this->_prefs.lockedFoldersList()) {
        this->ui->lockedFolderslistWidget->addItem(folderPath);
    }
}

void Comparison::on_lockedFolderButton_clicked()
{
    auto chooseLockedAt = this->_prefs.browseLockedFoldersLastPath();
    if (chooseLockedAt.isEmpty() || !QDir(chooseLockedAt).exists())
        chooseLockedAt = QStandardPaths::standardLocations(QStandardPaths::MoviesLocation)
                             .first(); /*defines where the chooser opens at*/
    const QString dir =
        QFileDialog::getExistingDirectory(ui->lockedFolderButton, QByteArrayLiteral("Open folder"), chooseLockedAt,
                                          QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty()) //empty because error or none chosen in dialog
        return;
    auto parentDir = QDir(dir);
    parentDir
        .cdUp(); // when a locked folder is selected, it's never children of it that will want to be selected next, rather those next to it
    this->_prefs.browseLockedFoldersLastPath(parentDir.absolutePath());
    addLockedFolderToList(dir);
    ui->lockedFolderslistWidget->setFocus();
}

void Comparison::addLockedFolderToList(QString folderPath)
{
    this->ui->lockedFolderslistWidget->addItem(folderPath);
    QStringList lockedList = this->_prefs.lockedFoldersList();
    lockedList.append(folderPath);
    this->_prefs.lockedFoldersList(lockedList);
}

void Comparison::dragEnterEvent(QDragEnterEvent* event)
{
    if (this->ui->tabWidget->currentIndex() == 2 // third tab, which is the locked folders list
        && event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void Comparison::dropEvent(QDropEvent* event)
{
    foreach (QUrl lockedFolder, event->mimeData()->urls()) {
        QString fileName = lockedFolder.toLocalFile();
        QFileInfo file(fileName);
        if (file.isDir())
            addLockedFolderToList(fileName);
    }
    ui->lockedFolderslistWidget->setFocus();
}

bool Comparison::isFileInProtectedFolder(const QString filePathName) const
{
    const QListWidget* list = ui->lockedFolderslistWidget;
    for (int i = 0; i < list->count(); ++i) {
        const QString folderPath = list->item(i)->text();
        if (filePathName.contains(folderPath))
            return true;
    }
    return false;
}

void Comparison::showLockedFolderContextMenu(const QPoint& pos)
{
    // Handle global position
    QPoint globalPos = ui->lockedFolderslistWidget->mapToGlobal(pos);

    // Create menu and insert some actions
    QMenu myMenu;
    myMenu.addAction("Delete selection", this, SLOT(eraseLockedFolderItem()));
    myMenu.addAction("Add new", this, SLOT(on_lockedFolderButton_clicked()));
    myMenu.addAction("Clear all", this, SLOT(clearLockedFolderList()));

    // Show context menu at handling position
    myMenu.exec(globalPos);
}

void Comparison::eraseLockedFolderItem()
{
    QStringList lockedFolders = this->_prefs.lockedFoldersList();
    // If multiple selection is on, we need to erase all selected items
    for (int i = 0; i < ui->lockedFolderslistWidget->selectedItems().size(); ++i) {
        // Get curent item on selected row
        QListWidgetItem* item = ui->lockedFolderslistWidget->takeItem(ui->lockedFolderslistWidget->currentRow());
        // And remove it
        lockedFolders.removeAll(item->text());
        delete item;
    }
    this->_prefs.lockedFoldersList(lockedFolders);
}

void Comparison::clearLockedFolderList()
{
    this->_prefs.lockedFoldersList(QStringList());
    ui->lockedFolderslistWidget->clear();
}

// ------------------ End of : Locked folders functions -------------------
// ------------------------------------------------------------------------

// ------------------------------------------------------------------------
// ------------------ Automatic video deletion functions ------------------

// Loop through all files
// If both files have all equal parameters, except name and path.
// Keep the left one (just a random choice but either could be kept).
// Compatible regardless of sort order since it's based on identical files and kinda random choice between the two anyway
void Comparison::on_identicalFilesAutoTrash_clicked()
{
    int initialDeletedNumber = _videosDeleted;
    int64_t initialSpaceSaved = _spaceSaved;
    bool userWantsToStop = false;

    // Automatic cleanup owns the pair indexes while it runs. Stop discovery so
    // a queued set selection cannot replace those indexes between loop steps.
    _backgroundDiscovery->stop();
    clearDuplicateSets();

    // Go over all videos from begin to end
    _leftVideo = 0; // reset to first video
    _rightVideo = 0;

    ui->tabWidget->setCurrentIndex(
        0); // switch to manual tab so that user can see progress and details if confirmation is still on

    QVector<Video*>::const_iterator left, right, begin = _videos.cbegin(), end = _videos.cend();
    for (left = begin + _leftVideo; left < end; left++, _leftVideo++) {
        for (_rightVideo++, right = begin + _rightVideo; right < end; right++, _rightVideo++) {
            if (bothVideosMatch(*left, *right) && QFileInfo::exists((*left)->_filePathName)
                && !(*left)->trashed // check trashed in case it is from Apple Photos
                && QFileInfo::exists((*right)->_filePathName) && !(*right)->trashed)
            {
                showVideo(QStringLiteral("left"));
                showVideo(QStringLiteral("right"));
                highlightBetterProperties();
                updateUI();

                // Check if params are equal and perform deletion, then go to next
                if (qAbs(_videos[_leftVideo]->size - _videos[_rightVideo]->size) > FILE_SIZE_BYTES_DIFF_STILL_EQUALS)
                    continue;
                // TODO mklemewmqwhoi13u18134tih2g
                if (_videos[_leftVideo]->modified != _videos[_rightVideo]->modified)
                    continue;
                if (_videos[_leftVideo]->duration != _videos[_rightVideo]->duration)
                    continue;
                if (_videos[_leftVideo]->height != _videos[_rightVideo]->height)
                    continue;
                if (_videos[_leftVideo]->width != _videos[_rightVideo]->width)
                    continue;
                if (qAbs(_videos[_leftVideo]->bitrate - _videos[_rightVideo]->bitrate)
                    > BITRATE_DIFF_STILL_EQUAL_kbs) //leave some margin due to decoding error
                    continue;
                if (_videos[_leftVideo]->framerate != _videos[_rightVideo]->framerate)
                    continue;
                if (_videos[_leftVideo]->codec != _videos[_rightVideo]->codec)
                    continue;
                if (_videos[_leftVideo]->audio != _videos[_rightVideo]->audio)
                    continue;
                if (_videos[_leftVideo]->meta.gpsCoordinates != _videos[_rightVideo]->meta.gpsCoordinates)
                    continue;

                int containedStatus = whichFilenameContainsTheOther((*left)->_filePathName, (*right)->_filePathName);

                if (ui->settingNamesInAnotherCheckbox->isChecked() && containedStatus == NOT_CONTAINED)
                    continue; // the file names were not contained in one another : we go to the next comparison

                if (containedStatus == LEFT_CONTAINS_RIGHT) {
                    deleteVideo(_leftVideo, true);
                    if (this->_prefs.isVerbose())
                        emit sendStatusMessage(QString("Auto remove kept %1\n")
                                                   .arg(QDir::toNativeSeparators(_videos[_rightVideo]->_filePathName)));
                }
                else { // by default and in specific name contained case : delete right video
                    deleteVideo(_rightVideo, true);
                    if (this->_prefs.isVerbose())
                        emit sendStatusMessage(QString("Auto remove kept %1\n")
                                                   .arg(QDir::toNativeSeparators(_videos[_leftVideo]->_filePathName)));
                }

                // ask user if he wants to continue or stop the auto deletion, and maybe disable confirmations
                if (!ui->disableDeleteConfirmationCheckbox->isChecked()) {
                    QMessageBox message;
                    message.setWindowTitle("Auto trash confirmation");
                    message.setText("Do you want to continue the auto deletion, and maybe disable confirmations ?");
                    message.addButton(tr("Continue"), QMessageBox::AcceptRole);
                    QPushButton* stopButton = message.addButton(tr("Stop"), QMessageBox::RejectRole);
                    QPushButton* disableConfirmationsButton = message.addButton(tr("Disable"), QMessageBox::ActionRole);
                    message.exec();
                    if (message.clickedButton() == stopButton) {
                        userWantsToStop = true;
                        break;
                    }
                    else if (message.clickedButton() == disableConfirmationsButton)
                        ui->disableDeleteConfirmationCheckbox->setCheckState(Qt::Checked);

                    // after prompting the user, if the left video was deleted we must break out of the
                    // inner for loop to go to the next left/reference video
                    if (containedStatus == LEFT_CONTAINS_RIGHT)
                        break;
                }
            }
        }
        ui->progressBar->setValue(progressBarValue(comparisonsSoFar()));
        _rightVideo = _leftVideo + 1;
        if (userWantsToStop)
            break;
    }

    if (!userWantsToStop) //finished going through all videos, check if there are still some matches from beginning
    {
        ui->tabWidget->setCurrentIndex(1); // switch back to auto tab
        _leftVideo = 0;
        _rightVideo = 0;
    }
    // display statistics of deletions
    QMessageBox::information(this, "Auto identical files deletion complete",
                             QString("%1 dupplicate files were moved to trash, saving %2 of disk space !")
                                 .arg(_videosDeleted - initialDeletedNumber)
                                 .arg(readableFileSize(_spaceSaved - initialSpaceSaved)));
    if (_someWereMovedInApplePhotosLibrary)
        displayApplePhotosAlbumDeletionMessage();
    restartBackgroundDiscovery();
    on_nextVideo_clicked();
}

// Loop through all files
// If both have :
// - same time duration
// - same resolution
// - same FPS
// - different file sizes
// Keeps either the bigger or smaller file of the two depending on user choice.
// Compatible regardless of sort order since it specifically chooses by file size regardless of left/right.
void Comparison::on_autoDelOnlySizeDiffersButton_clicked()
{
    int initialDeletedNumber = _videosDeleted;
    int64_t initialSpaceSaved = _spaceSaved;
    bool userWantsToStop = false;
    const bool keepBiggest = !ui->radioButton_onlySizeDiffers_keepSmallest->isChecked();
    const QString trashedSizeLabel = keepBiggest ? QStringLiteral("smaller") : QStringLiteral("bigger");

    // Automatic cleanup owns the pair indexes while it runs. Stop discovery so
    // a queued set selection cannot replace those indexes between loop steps.
    _backgroundDiscovery->stop();
    clearDuplicateSets();

    // Go over all videos from begin to end
    _leftVideo = 0; // reset to first video
    _rightVideo = 0;

    ui->tabWidget->setCurrentIndex(
        0); // switch to manual tab so that user can see progress and details if confirmation is on
    QCoreApplication::processEvents(); //next operations are blocking, might need to find a way to make it work nicer !

    QVector<Video*>::const_iterator left, right, begin = _videos.cbegin(), end = _videos.cend();
    for (left = begin + _leftVideo; left < end; left++, _leftVideo++) {
        for (_rightVideo++, right = begin + _rightVideo; right < end; right++, _rightVideo++) {
            if (bothVideosMatch(*left, *right) && QFileInfo::exists((*left)->_filePathName)
                && !(*left)->trashed // check trashed in case it is from Apple Photos
                && QFileInfo::exists((*right)->_filePathName) && !(*right)->trashed)
            {
                ui->progressBar->setValue(progressBarValue(comparisonsSoFar())); //update visible progress for user

                // Check if params are as required and perform deletion, then go to next
                if (qAbs(_videos[_leftVideo]->duration - _videos[_rightVideo]->duration)
                    > VIDEO_DURATION_STILL_EQUALS_MS) // video durations more than 1 second length difference
                    continue;
                if (!ui->autoOnlySizeDontCheckResFpsCheckbox->isChecked()) {
                    if (_videos[_leftVideo]->height != _videos[_rightVideo]->height)
                        continue;
                    if (_videos[_leftVideo]->width != _videos[_rightVideo]->width)
                        continue;
                    if (qAbs(_videos[_leftVideo]->framerate - _videos[_rightVideo]->framerate)
                        > 0.1) //both framerates more than 0.1 fps different
                        continue;
                }
                if (qAbs(_videos[_leftVideo]->size - _videos[_rightVideo]->size)
                    <= FILE_SIZE_BYTES_DIFF_STILL_EQUALS) // When sizes are identical, results are treated in specific other functionality
                    continue;
                if (ui->settingNamesInAnotherCheckbox->isChecked()
                    && whichFilenameContainsTheOther((*left)->_filePathName, (*right)->_filePathName) == NOT_CONTAINED)
                    continue; // the file names were not contained in one another : we go to the next comparison

                showVideo(QStringLiteral("left"));
                showVideo(QStringLiteral("right"));
                highlightBetterProperties();
                updateUI();

                const bool leftIsBigger = _videos[_leftVideo]->size > _videos[_rightVideo]->size;
                const bool deleteRightVideo = (keepBiggest && leftIsBigger) || (!keepBiggest && !leftIsBigger);
                const int videoToDelete = deleteRightVideo ? _rightVideo : _leftVideo;
                const int videoToKeep = (videoToDelete == _leftVideo) ? _rightVideo : _leftVideo;

                deleteVideo(videoToDelete, true);
                if (this->_prefs.isVerbose())
                    emit sendStatusMessage(QString("Auto remove kept %1\n")
                                               .arg(QDir::toNativeSeparators(_videos[videoToKeep]->_filePathName)));

                // ask user if he wants to continue or stop the auto deletion, and maybe disable confirmations
                if (!ui->disableDeleteConfirmationCheckbox->isChecked()) {
                    QMessageBox message;
                    message.setWindowTitle(QString("Auto trash %1 file sizes confirmation").arg(trashedSizeLabel));
                    message.setText(QString("Do you want to continue the auto deletion of %1 file sizes, and maybe "
                                            "disable confirmations ?")
                                        .arg(trashedSizeLabel));
                    message.addButton(tr("Continue"), QMessageBox::AcceptRole);
                    QPushButton* stopButton = message.addButton(tr("Stop"), QMessageBox::RejectRole);
                    QPushButton* disableConfirmationsButton =
                        message.addButton(tr("Disable confirmations"), QMessageBox::ActionRole);
                    message.exec();
                    if (message.clickedButton() == stopButton) {
                        userWantsToStop = true;
                        break;
                    }
                    else if (message.clickedButton() == disableConfirmationsButton)
                        ui->disableDeleteConfirmationCheckbox->setCheckState(Qt::Checked);
                }

                // when left video was deleted, we need to break
                // out of the inner for loop to go to the next left/reference video
                if (!deleteRightVideo)
                    break;
            }
        }
        ui->progressBar->setValue(progressBarValue(comparisonsSoFar()));
        _rightVideo = _leftVideo + 1;
        if (userWantsToStop)
            break;
    }

    if (!userWantsToStop) //finished going through all videos, check if there are still some matches from beginning
    {
        ui->tabWidget->setCurrentIndex(1); // switch back to auto tab
        _leftVideo = 0;
        _rightVideo = 0;
    }
    // display statistics of deletions
    QMessageBox::information(this, QString("Auto trash %1 file sizes complete").arg(trashedSizeLabel),
                             QString("%1 dupplicate files were moved to trash, saving %2 of disk space !")
                                 .arg(_videosDeleted - initialDeletedNumber)
                                 .arg(readableFileSize(_spaceSaved - initialSpaceSaved)));

    if (_someWereMovedInApplePhotosLibrary)
        displayApplePhotosAlbumDeletionMessage();
    restartBackgroundDiscovery();
    on_nextVideo_clicked();
}

// For now only used for auto delete AUTO_DELETE_ONLY_TIMES_DIFF
// TODO: refactor other auto delete modes to use this
// AUTO_DELETE_ONLY_TIMES_DIFF Compatible regardless of sort order since it keeps the earliest/latest one as selected by user
void Comparison::autoDeleteLoopthrough(const AutoDeleteConfig autoDelConfig)
{
    // loop through all files
    // and maybe trash one each time depending on config

    int initialDeletedNumber = _videosDeleted;
    int64_t initialSpaceSaved = _spaceSaved;
    bool userWantsToStop = false;

    // Automatic cleanup owns the pair indexes while it runs. Stop discovery so
    // a queued set selection cannot replace those indexes between loop steps.
    _backgroundDiscovery->stop();
    clearDuplicateSets();

    // Go over all videos from begin to end
    _leftVideo = 0; // reset to first video
    _rightVideo = 0;

    ui->tabWidget->setCurrentIndex(
        0); // switch to manual tab so that user can see progress and details if confirmation is on
    QCoreApplication::processEvents(); //next operations are blocking, might need to find a way to make it work nicer !

    QVector<Video*>::const_iterator left, right, begin = _videos.cbegin(), end = _videos.cend();
    for (left = begin + _leftVideo; left < end; left++, _leftVideo++) {
        for (_rightVideo++, right = begin + _rightVideo; right < end; right++, _rightVideo++) {
            if (bothVideosMatch(*left, *right) && QFileInfo::exists((*left)->_filePathName)
                && !(*left)->trashed // check trashed in case it is from Apple Photos
                && QFileInfo::exists((*right)->_filePathName) && !(*right)->trashed)
            {
                ui->progressBar->setValue(progressBarValue(comparisonsSoFar())); //update visible progress for user
                QCoreApplication::processEvents();

                // Check if params are as required or go to next
                if (ui->settingNamesInAnotherCheckbox->isChecked()
                    && whichFilenameContainsTheOther((*left)->_filePathName, (*right)->_filePathName) == NOT_CONTAINED)
                    continue; // the file names were not contained in one another : we go to the next comparison

                //find for the specific auto mode if one video needs to be deleted
                const VideoMetadata leftVidMeta = Video::videoToMetadata(*_videos[_leftVideo]);
                const VideoMetadata rightVidMeta = Video::videoToMetadata(*_videos[_rightVideo]);

                const VideoMetadata* vidToDeleteMetaPtr = autoDelConfig.videoToDelete(
                    &leftVidMeta, &rightVidMeta,
                    AutoDeleteUserSettings(ui->radioButton_onlyTimeDiffers_trashEarlier->isChecked()));
                if (vidToDeleteMetaPtr == nullptr) // null means the videos don't match in the auto mode
                    continue;

                // now we know videos are matched, we show them and the auto deletion goes through
                showVideo(QStringLiteral("left"));
                showVideo(QStringLiteral("right"));
                highlightBetterProperties();
                updateUI();

                if (vidToDeleteMetaPtr == &leftVidMeta) {
                    deleteVideo(_leftVideo, true);
                    if (this->_prefs.isVerbose())
                        emit sendStatusMessage(QString("Auto remove kept %1\n")
                                                   .arg(QDir::toNativeSeparators(_videos[_rightVideo]->_filePathName)));
                }
                else {
                    deleteVideo(_rightVideo, true);
                    if (this->_prefs.isVerbose())
                        emit sendStatusMessage(QString("Auto remove kept %1\n")
                                                   .arg(QDir::toNativeSeparators(_videos[_leftVideo]->_filePathName)));
                }

                // ask user if he wants to continue or stop the auto deletion, and maybe disable confirmations
                if (!ui->disableDeleteConfirmationCheckbox->isChecked()) {
                    QMessageBox message;
                    message.setWindowTitle(
                        QString("Auto trash by %1 confirmation").arg(autoDelConfig.getDeleteByText()));
                    message.setText(
                        QString("Do you want to continue the auto deletion by %1, and maybe disable confirmations ?")
                            .arg(autoDelConfig.getDeleteByText()));
                    message.addButton(tr("Continue"), QMessageBox::AcceptRole);
                    QPushButton* stopButton = message.addButton(tr("Stop"), QMessageBox::RejectRole);
                    QPushButton* disableConfirmationsButton =
                        message.addButton(tr("Disable confirmations"), QMessageBox::ActionRole);
                    message.exec();
                    if (message.clickedButton() == stopButton) {
                        userWantsToStop = true;
                        break;
                    }
                    else if (message.clickedButton() == disableConfirmationsButton)
                        ui->disableDeleteConfirmationCheckbox->setCheckState(Qt::Checked);
                }

                // when left video was deleted, we need to break
                // out of the inner for loop to go to the next left/reference video
                if (vidToDeleteMetaPtr == &leftVidMeta)
                    break;
            }
        }
        ui->progressBar->setValue(progressBarValue(comparisonsSoFar()));
        _rightVideo = _leftVideo + 1;
        if (userWantsToStop)
            break;
    }

    if (!userWantsToStop) //finished going through all videos, check if there are still some matches from beginning
    {
        ui->tabWidget->setCurrentIndex(1); // switch back to auto tab
        _leftVideo = 0;
        _rightVideo = 0;
    }
    // display statistics of deletions
    QMessageBox::information(this, QString("Auto trash by %1 complete").arg(autoDelConfig.getDeleteByText()),
                             QString("%1 dupplicate files were moved to trash, saving %2 of disk space !")
                                 .arg(_videosDeleted - initialDeletedNumber)
                                 .arg(readableFileSize(_spaceSaved - initialSpaceSaved)));

    if (_someWereMovedInApplePhotosLibrary)
        displayApplePhotosAlbumDeletionMessage();
    restartBackgroundDiscovery();
    on_nextVideo_clicked();
}

const VideoMetadata* Comparison::AutoDeleteConfig::videoToDelete(const VideoMetadata* meta1, const VideoMetadata* meta2,
                                                                 const AutoDeleteUserSettings userAutoDelConf) const
{
    if (_autoDelConfig == AUTO_DELETE_ONLY_TIMES_DIFF) {

        if (qAbs(meta1->size - meta2->size) > FILE_SIZE_BYTES_DIFF_STILL_EQUALS)
            return nullptr;
        if (qAbs(meta1->duration - meta2->duration) > VIDEO_DURATION_STILL_EQUALS_MS)
            return nullptr;
        if (meta1->height != meta2->height)
            return nullptr;
        if (meta1->width != meta2->width)
            return nullptr;
        if (qAbs(meta1->bitrate - meta2->bitrate)
            > BITRATE_DIFF_STILL_EQUAL_kbs) //leave some margin due to decoding error
            return nullptr;
        if (meta1->framerate != meta2->framerate)
            return nullptr;
        if (meta1->codec != meta2->codec)
            return nullptr;
        if (meta1->audio != meta2->audio)
            return nullptr;
        if (meta1->gpsCoordinates != meta2->gpsCoordinates)
            return nullptr;

        // check the dates and which is earlier
        const VideoMetadata** earlierVideo;
        if (meta1->_fileCreateDate < meta2->_fileCreateDate)
            earlierVideo = &meta1;
        else if (meta1->_fileCreateDate > meta2->_fileCreateDate)
            earlierVideo = &meta2;
        else if (meta1->modified < meta2->modified)
            earlierVideo = &meta1;
        else if (meta1->modified > meta2->modified)
            earlierVideo = &meta2;
        else
            return nullptr; // all dates are equal

        const VideoMetadata** laterVideo = &meta1;
        if (earlierVideo == &meta1)
            laterVideo = &meta2;

        //tell to delete depending on user setting
        if (userAutoDelConf.trashEarlierIsChecked)
            return *earlierVideo;
        else
            return *laterVideo;
    }
    else
        return nullptr;
}

QString Comparison::AutoDeleteConfig::getDeleteByText() const
{
    switch (_autoDelConfig) {
    case AUTO_DELETE_ONLY_TIMES_DIFF:
        return "dates";
    default:
        return "";
    }
}

int Comparison::whichFilenameContainsTheOther(QString leftFileNamepath, QString rightFileNamepath) const
{
    const QFileInfo leftVideoFile(leftFileNamepath);
    const QString leftFilename = leftVideoFile.fileName();
    const QString leftNoExtension = leftFilename.left(leftFilename.lastIndexOf("."));

    const QFileInfo rightVideoFile(rightFileNamepath);
    const QString rightFilename = rightVideoFile.fileName();
    const QString rightNoExtension = rightFilename.left(rightFilename.lastIndexOf("."));

    int containedStatus = NOT_CONTAINED;

    if (rightNoExtension.contains(leftNoExtension))
        containedStatus = RIGHT_CONTAINS_LEFT;
    else if (leftNoExtension.contains(rightNoExtension))
        containedStatus = LEFT_CONTAINS_RIGHT;

    return containedStatus;
}

// ------------------ End of : Automatic video deletion functions ------------------
// ---------------------------------------------------------------------------------

void Comparison::on_pushButton_importantFoldersAdd_clicked()
{
    const QString dir =
        QFileDialog::getExistingDirectory(ui->pushButton_importantFoldersAdd, QByteArrayLiteral("Open folder"),
                                          QStandardPaths::standardLocations(QStandardPaths::MoviesLocation)
                                              .first() /*defines where the chooser opens at*/,
                                          QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty()) //empty because error or none chosen in dialog
        return;
    ui->importantFoldersListWidget->addItem(dir);
    ui->importantFoldersListWidget->setFocus();
}

void Comparison::eraseImportantFolderItem()
{
    // If multiple selection is on, we need to erase all selected items
    for (int i = 0; i < ui->importantFoldersListWidget->selectedItems().size(); ++i) {
        // Get curent item on selected row
        QListWidgetItem* item = ui->importantFoldersListWidget->takeItem(ui->importantFoldersListWidget->currentRow());
        // And remove it
        delete item;
    }
}

void Comparison::clearImportantFolderList()
{
    ui->importantFoldersListWidget->clear();
}

void Comparison::showImportantFolderContextMenu(const QPoint& pos)
{
    // Handle global position
    QPoint globalPos = ui->importantFoldersListWidget->mapToGlobal(pos);

    // Create menu and insert some actions
    QMenu myMenu;
    myMenu.addAction("Delete selection", this, SLOT(eraseImportantFolderItem()));
    myMenu.addAction("Add new", this, SLOT(on_importantFolderButton_clicked()));
    myMenu.addAction("Clear all", this, SLOT(clearImportantFolderList()));

    // Show context menu at handling position
    myMenu.exec(globalPos);
}

void Comparison::displayApplePhotosAlbumDeletionMessage()
{
    QMessageBox::information(this, "",
                             QString("Notice: \n\nSome videos were not actually deleted"
                                     " as they were from an Apple Photos Library.\n"
                                     "They were added to the album 'Trash from %1'. "
                                     "You must manually delete them from within "
                                     "Apple Photos ! \n\n"
                                     "From Apple Photos, select them and press 'cmd' and 'delete' "
                                     " (or right click while pressing 'cmd', and select the option "
                                     " 'Delete', ⚠️ but not 'Delete from album' !!!)\n\n"
                                     "Then empty Apple Photos' trash")
                                 .arg(APP_NAME));
}

void Comparison::on_settingNamesInAnotherCheckbox_stateChanged(int arg1)
{
    QString status;
    if (arg1 == Qt::Checked)
        status = "ENABLED";
    else
        status = "DISABLED";

    ui->label_namesContainedInOneAnotherStatus_autoIdentFiles->setText(status);
    ui->label_namesContainedInOneAnotherStatus_autoOnlySizeDiff->setText(status);
    rebuildDuplicateSets();
}

void Comparison::on_ignoreDuplicatePairButton_clicked()
{
    if (!hasActiveManualComparison() || !_currentComparisonIsDirectMatch)
        return;
    Db cache(_prefs.cacheFilePathName()); // opening connexion to database
    cache.writePairToIgnore(_videos[_leftVideo]->_filePathName, _videos[_rightVideo]->_filePathName);
    rebuildDuplicateSets();
}
