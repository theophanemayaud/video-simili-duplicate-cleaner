#include "video_test_helpers.h"
#include <QTest>
#include <QCoreApplication>
#include <QWidget>
#include <QLayout>
#include <QLabel>
#include <QCheckBox>

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

// --------------END TestHelpers -------------------------
// -------------------------------------------------
