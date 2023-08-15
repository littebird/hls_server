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
    #include <libavfilter/buffersrc.h>
    #include <libavfilter/buffersink.h>
}

typedef struct FilteringContext {
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;

    AVPacket *enc_pkt;//编码数据包
    AVFrame *filtered_frame;//过滤后的帧
} FilteringContext;

typedef struct StreamContext {
    AVCodecContext *dec_ctx;
    AVCodecContext *enc_ctx;

    AVFrame *dec_frame;//解码帧
} StreamContext;

class Encoder
{
public:
    Encoder();
    int init_filter(FilteringContext *fctx,AVCodecContext *dec_ctx,AVCodecContext *enc_ctx, const char *filter_spec);
    int init_filters();
    void close();
    int open_input_file(const char *filename);
    int open_output_file(const char *filename);
    int encode_write_frame(unsigned int stream_idx,int flush);
    int filter_encode_write_frame(AVFrame *frame,unsigned int stream_idx);
    int flush_encoder(unsigned int stream_idx);
    void VOD(const char *inputfile);

private:
    AVFormatContext *ifmt_ctx;
    AVFormatContext *ofmt_ctx;

    FilteringContext *filter_ctx;   //过滤器上下文
    StreamContext *stream_ctx;      //流上下文

//    AVBSFContext *absf_aac_adtstoasc;    //aac->adts to asc过滤器
//    AVBSFContext *vbsf_h264_toannexb;    //h264->to annexb过滤器


};

#endif // ENCODER_H
