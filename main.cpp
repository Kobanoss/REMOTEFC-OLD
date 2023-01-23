#include <QCoreApplication>
#include "src/resender.h"
 
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
 
    UdpResender server;
 
    return app.exec();
}