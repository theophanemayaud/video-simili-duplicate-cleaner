#include <QtTest>

// add necessary includes here

class test_video : public QObject
{
    Q_OBJECT

public:
    test_video();
    ~test_video();

private slots:
    void test_case1();

};

test_video::test_video()
{

}

test_video::~test_video()
{

}

void test_video::test_case1()
{

}

QTEST_APPLESS_MAIN(test_video)

#include "tst_test_video.moc"
