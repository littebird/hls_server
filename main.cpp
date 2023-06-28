#include <QCoreApplication>
#include <iostream>
#include <encoder.h>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    std::cout<<"hls server \n";

    Encoder encoder;
    encoder.init();
    encoder.VOD("/root/hls_server/test.mp4");

    return a.exec();
}

