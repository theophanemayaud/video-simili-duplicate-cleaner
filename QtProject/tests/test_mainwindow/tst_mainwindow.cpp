#include <QtTest>
#include <QCoreApplication>

// add necessary includes here

class TestMainWindow : public QObject
{
    Q_OBJECT

public:
    TestMainWindow();
    ~TestMainWindow();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void test_case1();

};

TestMainWindow::TestMainWindow()
{

}

TestMainWindow::~TestMainWindow()
{

}

void TestMainWindow::initTestCase()
{

}

void TestMainWindow::cleanupTestCase()
{

}

void TestMainWindow::test_case1()
{

}

QTEST_MAIN(TestMainWindow)

#include "tst_mainwindow.moc"
