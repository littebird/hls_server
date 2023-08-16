#include <QCoreApplication>
#include <iostream>
#include"server.h"
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    Server hls_server;
    hls_server.start_conn();

    return a.exec();
}
