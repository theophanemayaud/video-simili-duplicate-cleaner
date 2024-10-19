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
    static bool saveVideoParamQListToCSV(const QList<VideoParam> videoParamQList, const QFileInfo csvInfo, const QDir videoBaseDir); // false->failed to save
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

    // create and show image of only differences
    QImage img1, img2;
    img1.loadFromData(ref_thumb,"JPG");
    img2.loadFromData(new_thumb,"JPG");
    qDebug() << "Img format="<<(int)img1.format();

    QWidget *diff_imgWidg = new QWidget;
    diff_imgWidg->setWindowTitle("Diff of two images");

    QVBoxLayout *diff_layout = new QVBoxLayout(diff_imgWidg);

    QLabel *diffR_label = new QLabel(diff_imgWidg);
    QLabel *diffG_label = new QLabel(diff_imgWidg);
    QLabel *diffB_label = new QLabel(diff_imgWidg);
    diffR_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    diffG_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    diffB_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QImage r(img1.width(), img1.height(), QImage::Format_RGB32);
    QImage g(img1.width(), img1.height(), QImage::Format_RGB32);
    QImage b(img1.width(), img1.height(), QImage::Format_RGB32);
    for(int y=0; y<img1.height(); y++){
        for(int x=0; x<img1.width(); x++){
            QRgb p1 = img1.pixel(x,y);
            QRgb p2 = qRgb(0, 0, 0);
            if(x < img2.width() && y < img2.height())
                p2 = img2.pixel(x,y);
            r.setPixel(x,y,qRgb(255*sqrt(abs(qRed(p1)-qRed(p2))/255.00),0,0));
            g.setPixel(x,y,qRgb(0,255*sqrt(abs(qGreen(p1)-qGreen(p2))/255.00),0));
            b.setPixel(x,y,qRgb(0,0,255*sqrt(abs(qBlue(p1)-qBlue(p2))/255.00)));
        }
    }
    diffR_label->setPixmap(QPixmap::fromImage(r));
    diffR_label->setScaledContents(true);
    diffG_label->setPixmap(QPixmap::fromImage(g));
    diffG_label->setScaledContents(true);
    diffB_label->setPixmap(QPixmap::fromImage(b));
    diffB_label->setScaledContents(true);

    diff_layout->addWidget(diffR_label);
    diff_layout->addWidget(diffG_label);
    diff_layout->addWidget(diffB_label);
    diff_imgWidg->setLayout(diff_layout);
    diff_imgWidg->showNormal();

    bool accepted = false;
    while(ui_image->isVisible()){
        QTest::qWait(100);
        if(accept->isChecked()){
            accepted = true;
            break;
        }
        if(reject->isChecked()){
            accepted = false;
            break;
        }
    }

    ui_image->window()->close();
    diff_imgWidg->window()->close();
    delete ui_image;
    delete diff_imgWidg;
    QCoreApplication::processEvents();
    return accepted;
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

bool TestHelpers::saveVideoParamQListToCSV(const QList<VideoParam> videoParamQList, const QFileInfo csvInfo, const QDir videoBaseDir){
    const char sep = VideoParam::sep;

    if(csvInfo.exists()) // we don't want to overwrite
        return false;
    QFile csvFile(csvInfo.filePath());
    if(!csvFile.open(QIODevice::WriteOnly))
        return false;

    QTextStream output(&csvFile);
    // write headers, to mirror videoParam class
    output <<
              "videoFilename" << sep <<
              "thumbnailFilename" << sep <<
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
        output << videoParam.videoInfo.absoluteFilePath().remove(videoBaseDir.path() + "/") << sep <<
                  videoParam.thumbnailInfo.fileName() << sep <<
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
