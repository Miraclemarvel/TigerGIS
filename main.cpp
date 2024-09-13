#include "TigerGIS.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    TigerGIS w;
    w.show();
    return a.exec();
}
