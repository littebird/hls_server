#ifndef ENCODER_H
#define ENCODER_H

#include <iostream>
extern "C" {
    #include <libavutil/samplefmt.h>
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavdevice/avdevice.h>
    #include <libswresample/swresample.h>
}


class Encoder
{
public:
    Encoder();
    void init();
    void VOD(std::string file);
private:
    AVFormatContext *pFormatCtx;
    AVCodecContext *pAudioCodecCtx;
    AVCodecContext *pVideoCodecCtx;
    AVCodec *pAudioCodec;
    AVCodec *pVideoCodec;
    int m_audioIndex=-1;
    int m_videoIndex=-1;
};

#endif // ENCODER_H
