#ifndef ENCODER_H
#define ENCODER_H

#include <iostream>
extern "C" {
    #include <libavutil/samplefmt.h>
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavdevice/avdevice.h>
    #include <libswresample/swresample.h>
    #include <libavfilter/avfilter.h>
}

struct FilteringContext{
    AVFilterContext *bufferSink_ctx;
    AVFilterContext *bufferSrc_ctx;
    AVFilterGraph *filter_graph;
};

class Encoder
{
public:
    Encoder();
    void init();
    void init_filter(FilteringContext *fctx,AVCodecContext *dec_ctx,AVCodecContext *enc_ctx, const char *filter_spec);
    void close();
    void open_input_file(const char *file);
    void open_output_file(const char *file);
    AVStream* add_out_stream(AVFormatContext* outCtx,AVMediaType type);
    void conduct_ts();

private:
    AVFormatContext *inFormatCtx;
    AVFormatContext *outFormatCtx;

    FilteringContext *filter_ctx;

    AVBSFContext *absf_aac_adtstoasc;    //aac->adts to asc过滤器
    AVBSFContext *vbsf_h264_toannexb;    //h264->to annexb过滤器
    AVCodecContext *pAudioCodecCtx;
    AVCodecContext *pVideoCodecCtx;
    AVCodec *pAudioCodec;
    AVCodec *pVideoCodec;
    AVStream *audio_stream;
    AVStream *video_stream;
    int m_audioIndex=-1;
    int m_videoIndex=-1;

};

#endif // ENCODER_H
