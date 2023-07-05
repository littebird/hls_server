#include <QCoreApplication>
#include <iostream>
#include <encoder.h>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    std::cout<<"hls server \n";

    Encoder encoder;
    encoder.init();
//    encoder.open_input_file("../hls_server/test.mp4");
//    encoder.conduct_ts();

    return a.exec();
}

