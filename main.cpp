#include <QCoreApplication>
#include <iostream>
#include "encoder.h"
#include"segmenter.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    Encoder encoder;
    encoder.VOD("../hls_server/test.mp4");//转码播放
    //to do 切片生成m3u8文件


    segmenter seg{"../hls_server/tests.ts"};
    seg.init();
    seg.slice();

    return a.exec();
}
