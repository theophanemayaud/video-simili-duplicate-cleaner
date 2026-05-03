#include "mainwindow.h"

int main(int argc, char *argv[])
{
    qSetMessagePattern("%{file}(%{line}) %{function}: %{message}");
    qDebug() << "Program start by Théophane with path :" << QDir::currentPath();

    QApplication a(argc, argv);
    
    // Set application properties for proper Qt StandardPaths resolution
    a.setApplicationName("Video simili duplicate cleaner");
    a.setOrganizationName("5743TheophaneMayaud");
    
    MainWindow w;
    w.show();
    return a.exec();
}
