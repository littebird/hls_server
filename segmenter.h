#ifndef SEGMENTER_H
#define SEGMENTER_H

#define OUTPUT_PREFIX  "seg_test"//切割文件前缀
#define URL_PREFIX     "../hls_server/"
#define NUM_SEGMENTS    30 //在磁盘上一共最多存储多少个分片
#define SEGMENT_DURATION    10//每一片切割多少秒

#include<stdio.h>
#include<fstream>
#include<string>
#include<vector>
#include<iostream>
extern "C" {

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}
#include<iostream>
class segmenter
{
public:
    segmenter(const char *inpath);
    ~segmenter();

    int init();//初始化输入AVFormatContext和AVBSFContext
    int init_mux();//初始化输出AVFmormatContext
    int uinit();//写输出文件尾部，并释放内存
    AVStream *add_out_stream(AVFormatContext* output_format_context,AVMediaType codec_type_t);//增加一个流
    void slice();//切片
    int write_index_file(const unsigned int first_segment, const unsigned int last_segment,
                         const int end,const int actual_segment_durations[]);//写m3u8文件

private:
    const char *input_filename;//输入文件名
    const char *output_filename;//输出文件名
    unsigned int output_index;//生成的切片文件顺序编号

    AVFormatContext *ifmt_ctx;
    AVFormatContext *ofmt_ctx;
    AVStream *video_st;
    AVStream *audio_st;
    int video_stream_idx;
    int audio_stream_idx;
    AVBSFContext *vbsf_h264_toannexb;


};

#endif // SEGMENTER_H
