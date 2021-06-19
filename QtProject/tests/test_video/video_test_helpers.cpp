#include <QTest>
#include <QCoreApplication>
#include <QWidget>
#include <QLayout>
#include <QLabel>
#include <QCheckBox>

class VideoParam {
public:
    static const int nb_params = 13;
    static const char sep = '"';
    static const QString timeformat;

    QFileInfo videoInfo;
    QFileInfo thumbnailInfo;
    int64_t size;
    QDateTime modified;
    int64_t duration;
    int bitrate;
    double framerate;
    QString codec;
    QString audio;
    short width;
    short height;
    uint64_t hash1;
    uint64_t hash2;
};

const QString VideoParam::timeformat = "yyyy-MM-dd HH:mm:ss";

class TestHelpers {
public:
    static bool doThumbnailsLookSameWindow(const QByteArray ref_thumb, const QByteArray new_thumb, const QString title);
    // returns an empty list if either csv didn't exist, or something else happened.
    static QList<VideoParam> importCSVtoVideoParamQList(const QFileInfo csvInfo, const QDir videoDir, const QDir thumbDir); // CSV : sep  seperated values

    // will not overwrite it already exists
    static bool saveVideoParamQListToCSV(const QList<VideoParam> videoParamQList, const QFileInfo csvInfo); // false->failed to save
};

// -------------------------------------------------
// ------------START TestHelpers -----------------
bool TestHelpers::doThumbnailsLookSameWindow(const QByteArray ref_thumb, const QByteArray new_thumb, const QString title){
    QWidget *ui_image = new QWidget;
    ui_image->setWindowTitle(title);
    QVBoxLayout *layout = new QVBoxLayout(ui_image);

    // Create two image labels
    QLabel *label = new QLabel(ui_image);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QPixmap mPixmap;
    mPixmap.loadFromData(ref_thumb,"JPG");
    label->setPixmap(mPixmap);
    label->setScaledContents(true);
    QLabel *label2 = new QLabel(ui_image);
    label2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QPixmap mPixmap2;
    mPixmap2.loadFromData(new_thumb,"JPG");
    label2->setPixmap(mPixmap2);
    label2->setScaledContents(true);

    // Create buttons and texts
    QLabel *refText = new QLabel(ui_image);
    refText->setText("Ref thumbnail");
    QLabel *newText = new QLabel(ui_image);
    newText->setText("New thumbnail");
    QHBoxLayout *checkLayout = new QHBoxLayout();
    QCheckBox *accept = new QCheckBox(ui_image);
    accept->setText("Identical looking");
    QCheckBox *reject = new QCheckBox(ui_image);
    reject->setText("Different looking");
    checkLayout->addWidget(accept);
    checkLayout->addWidget(reject);

    //Put all together
    layout->addWidget(refText);
    layout->addWidget(label);
    layout->addWidget(newText);
    layout->addWidget(label2);
    layout->addLayout(checkLayout);

    ui_image->setLayout(layout);
    ui_image->showMaximized();
    while(ui_image->isVisible()){
        QTest::qWait(100);
        if(accept->isChecked()){
            ui_image->close();
            return true;
        }
        if(reject->isChecked()){
            ui_image->close();
            return false;
        }
    }

    ui_image->close();
    return false;
}

QList<VideoParam> TestHelpers::importCSVtoVideoParamQList(const QFileInfo csvInfo, const QDir videoDir, const QDir thumbDir){
    const char sep = VideoParam::sep;
    QList<VideoParam> videoParamList;

    if(!csvInfo.exists())
        return videoParamList;

    QFile csvFile(csvInfo.absoluteFilePath());
    if(!csvFile.open(QIODevice::ReadOnly))
        return videoParamList;

    // read first line, as header and skip (only use it to get width)
    QByteArray line = csvFile.readLine();
    if(line.count(sep) != VideoParam::nb_params){
        qDebug() << "File number of parameters didn't match class !";
        return videoParamList;
    }

    while (!csvFile.atEnd()) {
        VideoParam param;
        line = csvFile.readLine();
        if(line.count(sep) != VideoParam::nb_params){
            qDebug() << "File number of parameters didn't match class !";
            csvFile.close();
            return QList<VideoParam>();
        }
        QList<QByteArray> stringParams = line.split(sep);

        param.videoInfo = QFileInfo(videoDir.path()+"/"+stringParams[0]);
        param.thumbnailInfo = QFileInfo(thumbDir.path()+"/"+stringParams[1]);
        param.size = stringParams[2].toLongLong();
        param.modified = QDateTime::fromString(stringParams[3], VideoParam::timeformat);
        param.duration = stringParams[4].toLongLong();
        param.bitrate = stringParams[5].toInt();
        param.framerate = stringParams[6].toDouble();
        param.codec = stringParams[7];
        param.audio = stringParams[8];
        param.width = stringParams[9].toShort();
        param.height = stringParams[10].toShort();
        param.hash1 = stringParams[11].toULongLong();
        param.hash2 = stringParams[12].toULongLong();

        videoParamList.append(param);
    }

    csvFile.close();
    return videoParamList;
}

bool TestHelpers::saveVideoParamQListToCSV(const QList<VideoParam> videoParamQList, const QFileInfo csvInfo){
    const char sep = VideoParam::sep;

    if(csvInfo.exists()) // we don't want to overwrite
        return false;
    QFile csvFile(csvInfo.filePath());
    if(!csvFile.open(QIODevice::WriteOnly))
        return false;

    QTextStream output(&csvFile);
    // write headers, to mirror videoParam class
    output <<
              "videoInfo" << sep <<
              "thumbnailInfo" << sep <<
              "size" << sep <<
              "modified" << sep <<
              "duration" << sep <<
              "bitrate" << sep <<
              "framerate" << sep <<
              "codec" << sep <<
              "audio" << sep <<
              "width" << sep <<
              "height" << sep <<
              "hash1" << sep <<
              "hash2" << sep <<
              "\n";

    // write all parameters in the list
    foreach(VideoParam videoParam, videoParamQList){
        output << videoParam.videoInfo.absoluteFilePath() << sep <<
                  videoParam.thumbnailInfo.absoluteFilePath() << sep <<
                  QString::number(videoParam.size) << sep <<
                  videoParam.modified.toString(VideoParam::timeformat) << sep <<
                  QString::number(videoParam.duration) << sep <<
                  QString::number(videoParam.bitrate) << sep <<
                  QString::number(videoParam.framerate) << sep <<
                  videoParam.codec << sep <<
                  videoParam.audio << sep <<
                  QString::number(videoParam.width) << sep <<
                  QString::number(videoParam.height) << sep <<
                  QString::number(videoParam.hash1) << sep <<
                  QString::number(videoParam.hash2) << sep <<
                  "\n";
    }
    csvFile.close();
    return true;
}

// --------------END TestHelpers -------------------------
// -------------------------------------------------
