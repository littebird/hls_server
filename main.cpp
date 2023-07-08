#include <QCoreApplication>
#include <iostream>
#include <encoder.h>
#include"segmenter.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    std::cout<<"hls server \n";

//    segmenter seg{"../test.mp4"};
//    seg.init();
//    seg.slice();
//    Encoder encoder;
//    encoder.VOD("../hls_server/test.mp4");//转码播放
    //to do 切片生成m3u8文件

    return a.exec();
}
