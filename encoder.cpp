#include "encoder.h"

Encoder::Encoder()
{

}


int Encoder::init_filter(FilteringContext *fctx, AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, const char *filter_spec)
{
    char args[512];
    int ret = 0;
    const AVFilter *buffersrc = nullptr;
    const AVFilter *buffersink = nullptr;
    AVFilterContext *buffersrc_ctx = nullptr;
    AVFilterContext *buffersink_ctx = nullptr;
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterGraph *filter_graph = avfilter_graph_alloc();

    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        buffersrc = avfilter_get_by_name("buffer");
        buffersink = avfilter_get_by_name("buffersink");
        if (!buffersrc || !buffersink) {
            av_log(nullptr, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        snprintf(args, sizeof(args),
                "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
                dec_ctx->pkt_timebase.num, dec_ctx->pkt_timebase.den,
                dec_ctx->sample_aspect_ratio.num,
                dec_ctx->sample_aspect_ratio.den);

        //创建过滤器实例并将其添加到现有的图形中
        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                args, nullptr, filter_graph);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Cannot create buffer source\n");
            goto end;
        }

        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                nullptr, nullptr, filter_graph);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Cannot create buffer sink\n");
            goto end;
        }

        //设置AVOption
        ret = av_opt_set_bin(buffersink_ctx, "pix_fmts",
                (uint8_t*)&enc_ctx->pix_fmt, sizeof(enc_ctx->pix_fmt),
                AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Cannot set output pixel format\n");
            goto end;
        }
    } else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        buffersrc = avfilter_get_by_name("abuffer");
        buffersink = avfilter_get_by_name("abuffersink");
        if (!buffersrc || !buffersink) {
            av_log(nullptr, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        if(!dec_ctx->channel_layout)
            dec_ctx->channel_layout=av_get_default_channel_layout(dec_ctx->channels);//默认音频通道布局

        std::string trans=std::to_string(dec_ctx->channel_layout);

        snprintf(args, sizeof(args),
                "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
                dec_ctx->pkt_timebase.num, dec_ctx->pkt_timebase.den, dec_ctx->sample_rate,
                av_get_sample_fmt_name(dec_ctx->sample_fmt),
                trans.c_str());

        //创建过滤器实例并将其添加到现有的图形中
        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                args, nullptr, filter_graph);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Cannot create audio buffer source\n");
            goto end;
        }

        //创建过滤器实例并将其添加到现有的图形中
        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                nullptr, nullptr, filter_graph);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
            goto end;
        }

        //设置AVOption
        ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
                (uint8_t*)&enc_ctx->sample_fmt, sizeof(enc_ctx->sample_fmt),
                AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Cannot set output sample format\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "channel_layouts",
                             (uint8_t*)&enc_ctx->channel_layout,sizeof(enc_ctx->channel_layout), AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Cannot set output channel layout\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
                (uint8_t*)&enc_ctx->sample_rate, sizeof(enc_ctx->sample_rate),
                AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Cannot set output sample rate\n");
            goto end;
        }
    } else {
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    //设置inputs和outputs结构
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = nullptr;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = nullptr;

    if (!outputs->name || !inputs->name) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    //将由字符串描述的图形添加到图形中
    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec,
                    &inputs, &outputs, nullptr)) < 0)
        goto end;

    //检查有效性并配置图中的所有链接和格式
    if ((ret = avfilter_graph_config(filter_graph, nullptr)) < 0)
        goto end;

    //填充FilteringContext
    fctx->buffersrc_ctx = buffersrc_ctx;
    fctx->buffersink_ctx = buffersink_ctx;
    fctx->filter_graph = filter_graph;

end:
    //释放资源
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;

}

int Encoder::init_filters()
{    
    const char *filter_spec;
    unsigned int i;
    int ret;
    filter_ctx = (FilteringContext *)av_malloc_array(ifmt_ctx->nb_streams, sizeof(*filter_ctx));
    if (!filter_ctx)
        return AVERROR(ENOMEM);

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        filter_ctx[i].buffersrc_ctx  = nullptr;
        filter_ctx[i].buffersink_ctx = nullptr;
        filter_ctx[i].filter_graph   = nullptr;
        if (!(ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO
                || ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO))
            continue;


        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            filter_spec = "null"; //视频直通（虚拟）滤波器
        else
            filter_spec = "anull"; //音频直通（虚拟）滤波器
        ret = init_filter(&filter_ctx[i], stream_ctx[i].dec_ctx,
                stream_ctx[i].enc_ctx, filter_spec);
        if (ret)
            return ret;

        filter_ctx[i].enc_pkt = av_packet_alloc();
        if (!filter_ctx[i].enc_pkt)
            return AVERROR(ENOMEM);

        filter_ctx[i].filtered_frame = av_frame_alloc();
        if (!filter_ctx[i].filtered_frame)
            return AVERROR(ENOMEM);
    }
    return 0;
}

void Encoder::close()//释放资源
{
    av_free(filter_ctx);
    av_free(stream_ctx);
    avformat_free_context(ifmt_ctx);
    avformat_free_context(ofmt_ctx);
//    av_bsf_free(&vbsf_h264_toannexb);
//    av_bsf_free(&absf_aac_adtstoasc);

}


int Encoder::open_input_file(const char *filename)
{
    int ret;
    unsigned int i;

    ifmt_ctx = nullptr;
    //根据url打开码流，并分析选择匹配的解复用器
    if ((ret = avformat_open_input(&ifmt_ctx, filename, nullptr, nullptr)) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

    //读取文件的数据获取流信息
    if ((ret = avformat_find_stream_info(ifmt_ctx, nullptr)) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    //分配流上下文内存
    stream_ctx = (StreamContext *)av_calloc(ifmt_ctx->nb_streams, sizeof(*stream_ctx));
    if (!stream_ctx)
        return AVERROR(ENOMEM);

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *stream = ifmt_ctx->streams[i];
        //根据解码器id找到解码器
        const AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
        AVCodecContext *codec_ctx;
        if (!dec) {
            av_log(nullptr, AV_LOG_ERROR, "Failed to find decoder for stream #%u\n", i);
            return AVERROR_DECODER_NOT_FOUND;
        }
        //根据解码器分配解码器上下文内存
        codec_ctx = avcodec_alloc_context3(dec);
        if (!codec_ctx) {
            av_log(nullptr, AV_LOG_ERROR, "Failed to allocate the decoder context for stream #%u\n", i);
            return AVERROR(ENOMEM);
        }
        //将流中解码器需要的信息AVcodecparameters添加到解码器context
        ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Failed to copy decoder parameters to input decoder context "
                   "for stream #%u\n", i);
            return ret;
        }

        //解码器的时间戳的时间基
        codec_ctx->pkt_timebase = stream->time_base;

        //重新编码音视频
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO|| codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {

            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)//视频
                codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, nullptr);
            //打开解码器
            ret = avcodec_open2(codec_ctx, dec, nullptr);
            if (ret < 0) {
                av_log(nullptr, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return ret;
            }
        }
        stream_ctx[i].dec_ctx = codec_ctx;

        stream_ctx[i].dec_frame = av_frame_alloc();
        if (!stream_ctx[i].dec_frame)
            return AVERROR(ENOMEM);
    }

    //文件信息
    std::cout<<"--------------- File Information ----------------\n";
    av_dump_format(ifmt_ctx, 0, filename, 0);
    std::cout<<"-------------------------------------------------\n";
    return 0;

}

int Encoder::open_output_file(const char *filename)
{   
    AVStream *out_stream;//out流
    AVStream *in_stream;//in流
    AVCodecContext *dec_ctx, *enc_ctx;
    const AVCodec *encoder;
    int ret;
    unsigned int i;

    ofmt_ctx = nullptr;
    //使用 avformat_alloc_output_context2 函数就可以根据文件名分配合适的AVFormatContext管理结构
    avformat_alloc_output_context2(&ofmt_ctx, nullptr, nullptr, filename);
    if (!ofmt_ctx) {
        av_log(nullptr, AV_LOG_ERROR, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }


    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        out_stream = avformat_new_stream(ofmt_ctx, nullptr);//创建输出码流的AVStream
        if (!out_stream) {
            av_log(nullptr, AV_LOG_ERROR, "Failed allocating output stream\n");// 分配输出流失败
            return AVERROR_UNKNOWN;
        }

        in_stream = ifmt_ctx->streams[i];
        dec_ctx = stream_ctx[i].dec_ctx;

        if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
                || dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            //编码器选择与解码器相同
            encoder = avcodec_find_encoder(dec_ctx->codec_id);
            if (!encoder) {
                av_log(nullptr, AV_LOG_FATAL, "Necessary encoder not found\n");
                return AVERROR_INVALIDDATA;
            }
            //根据编码器分配编码器上下文内存
            enc_ctx = avcodec_alloc_context3(encoder);
            if (!enc_ctx) {
                av_log(nullptr, AV_LOG_FATAL, "Failed to allocate the encoder context\n");
                return AVERROR(ENOMEM);
            }


            if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                enc_ctx->height = dec_ctx->height;//高
                enc_ctx->width = dec_ctx->width;//宽
                enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;//长宽比
               //从支持的像素格式列表中获取第一种格式
                if (encoder->pix_fmts)
                    enc_ctx->pix_fmt = encoder->pix_fmts[0];
                else
                    enc_ctx->pix_fmt = dec_ctx->pix_fmt;
                //设置编码器的时间基
                enc_ctx->time_base = av_inv_q(dec_ctx->framerate);
//std::cout<<enc_ctx->time_base.den<<" 1111111\n";
            } else {//音频
                enc_ctx->sample_rate = dec_ctx->sample_rate;//采样率

                enc_ctx->channel_layout=dec_ctx->channel_layout;//音频通道布局

                enc_ctx->channels = av_get_channel_layout_nb_channels(enc_ctx->channel_layout);//音频通道数

                //选择第一个采样格式
                enc_ctx->sample_fmt = encoder->sample_fmts[0];//采样格式
                enc_ctx->time_base = (AVRational){1, enc_ctx->sample_rate};
            }

            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            //打开编码器
            ret = avcodec_open2(enc_ctx, encoder, nullptr);
            if (ret < 0) {
                av_log(nullptr, AV_LOG_ERROR, "Cannot open video encoder for stream #%u\n", i);
                return ret;
            }
            ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
            if (ret < 0) {
                av_log(nullptr, AV_LOG_ERROR, "Failed to copy encoder parameters to output stream #%u\n", i);
                return ret;
            }

            out_stream->time_base = enc_ctx->time_base;
            stream_ctx[i].enc_ctx = enc_ctx;
        } else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN) {//未知类型
            av_log(nullptr, AV_LOG_FATAL, "Elementary stream #%d is of unknown type, cannot proceed\n", i);
            return AVERROR_INVALIDDATA;
        } else {
            /* if this stream must be remuxed */
            ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
            if (ret < 0) {
                av_log(nullptr, AV_LOG_ERROR, "Copying parameters for stream #%u failed\n", i);
                return ret;
            }
            out_stream->time_base = in_stream->time_base;
        }

    }
    av_dump_format(ofmt_ctx, 0, filename, 1);

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);//打开FFmpeg输出文件
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Could not open output file '%s'", filename);
            return ret;
        }
    }

    //写入输出文件头
    ret = avformat_write_header(ofmt_ctx, nullptr);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Error occurred when opening output file\n");// 打开输出文件时出错
        return ret;
    }

    return 0;
}

int Encoder::encode_write_frame(unsigned int stream_idx, int flush)
{
    StreamContext *stream = &stream_ctx[stream_idx];
    FilteringContext *filter = &filter_ctx[stream_idx];
    AVFrame *filt_frame = flush ? nullptr : filter->filtered_frame;//是否刷新过滤帧
    AVPacket *enc_pkt = filter->enc_pkt;
    int ret;

    av_log(nullptr, AV_LOG_INFO, "Encoding frame\n");
    //编码过滤后的帧

    //擦包
    av_packet_unref(enc_pkt);

    if (filt_frame && filt_frame->pts != AV_NOPTS_VALUE)
        filt_frame->pts = av_rescale_q(filt_frame->pts, filt_frame->time_base,
                                       stream->enc_ctx->time_base);

    //发送帧
    ret = avcodec_send_frame(stream->enc_ctx, filt_frame);

    if (ret < 0)
        return ret;

    while (ret >= 0) {
        ret = avcodec_receive_packet(stream->enc_ctx, enc_pkt);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;

        //准备编码数据包
        enc_pkt->stream_index = stream_idx;
        //将数据包中的有效计时字段从一个时间基转换为另一个时间基
        av_packet_rescale_ts(enc_pkt,
                             stream->enc_ctx->time_base,
                             ofmt_ctx->streams[stream_idx]->time_base);

        av_log(nullptr, AV_LOG_DEBUG, "Muxing frame\n");
        //将数据包编码写入输出媒体文件
        ret = av_interleaved_write_frame(ofmt_ctx, enc_pkt);
    }

    return ret;

}

int Encoder::filter_encode_write_frame(AVFrame *frame, unsigned int stream_idx)//过滤编码写入帧
{    
    FilteringContext *filter = &filter_ctx[stream_idx];
    int ret;

    av_log(nullptr, AV_LOG_INFO, "Pushing decoded frame to filters\n");

    //将解码后的帧推入 filtergraph
    ret = av_buffersrc_add_frame_flags(filter->buffersrc_ctx,frame, 0);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
        return ret;
    }

    //从filtergraph 中拉出过滤的帧
    while (1) {
        av_log(nullptr, AV_LOG_INFO, "Pulling filtered frame from filters\n");

        // av_buffersink_get_frame 从接收器获取一个带有过滤数据的帧,并将其放入帧中
        ret = av_buffersink_get_frame(filter->buffersink_ctx,
                                      filter->filtered_frame);
        if (ret < 0) {
            //如果没有更多的输出帧 - returns AVERROR(EAGAIN)||如果刷新并且没有更多的帧用于输出 - returns AVERROR_EOF
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                ret = 0;
            break;
        }


        filter->filtered_frame->time_base = av_buffersink_get_time_base(filter->buffersink_ctx);;
        filter->filtered_frame->pict_type = AV_PICTURE_TYPE_NONE;
        ret = encode_write_frame(stream_idx, 0);
        av_frame_unref(filter->filtered_frame);//擦包
        if (ret < 0)
            break;
    }

    return ret;

}

int Encoder::flush_encoder(unsigned int stream_idx)
{
    if (!(stream_ctx[stream_idx].enc_ctx->codec->capabilities &
                AV_CODEC_CAP_DELAY))
        return 0;

    av_log(nullptr, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_idx);
    return encode_write_frame(stream_idx, 1);
}

void Encoder::VOD(const char *inputfile)
{   
    int ret;
    AVPacket *packet = nullptr;
    unsigned int stream_index;
    unsigned int i;

    //打开输入文件
    if ((ret = open_input_file(inputfile)) < 0)
        goto end;
    //打开输出文件
    if ((ret = open_output_file("../hls_server/tests.ts")) < 0)
        goto end;
    //初始化AVFilter相关的结构体
    if ((ret = init_filters()) < 0)
        goto end;
    //分配数据包内存
    if (!(packet = av_packet_alloc()))
        goto end;

    //循环读取文件中所有数据包
    while (1) {
        if ((ret = av_read_frame(ifmt_ctx, packet)) < 0)
            break;
        stream_index = packet->stream_index;
        av_log(nullptr, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n",
                stream_index);

        if (filter_ctx[stream_index].filter_graph) {
            //重新编码和过滤帧

            StreamContext *stream = &stream_ctx[stream_index];

            av_log(nullptr, AV_LOG_DEBUG, "Going to reencode&filter the frame\n");

             //视频解码或者音频解码

            ret = avcodec_send_packet(stream->dec_ctx, packet);
            if (ret < 0) {
                av_log(nullptr, AV_LOG_ERROR, "Decoding failed\n");
                break;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(stream->dec_ctx, stream->dec_frame);
                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                    break;
                else if (ret < 0)
                    goto end;

                stream->dec_frame->pts = stream->dec_frame->best_effort_timestamp;
                ret = filter_encode_write_frame(stream->dec_frame, stream_index);//过滤编码一帧
                if (ret < 0)
                    goto end;
            }
        } else {
            //重新复制此帧而不重新编码

            //将数据包中的有效计时字段从一个时间基转换为另一个时间基
            av_packet_rescale_ts(packet,
                                 ifmt_ctx->streams[stream_index]->time_base,
                                 ofmt_ctx->streams[stream_index]->time_base);

            ret = av_interleaved_write_frame(ofmt_ctx, packet);//将数据包写入输出媒体文件
            if (ret < 0)
                goto end;
        }
        av_packet_unref(packet);
    }

    //刷新过滤器和编码器
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        //刷新过滤器
        if (!filter_ctx[i].filter_graph)
            continue;
        ret = filter_encode_write_frame(nullptr, i);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Flushing filter failed\n");
            goto end;
        }

        //刷新编码器
        ret = flush_encoder(i);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Flushing encoder failed\n");
            goto end;
        }
    }

    av_write_trailer(ofmt_ctx);//写到输出文件尾

end:
    //回收资源
    av_packet_free(&packet);
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        avcodec_free_context(&stream_ctx[i].dec_ctx);
        if (ofmt_ctx && ofmt_ctx->nb_streams > i && ofmt_ctx->streams[i] && stream_ctx[i].enc_ctx)
            avcodec_free_context(&stream_ctx[i].enc_ctx);
        if (filter_ctx && filter_ctx[i].filter_graph) {
            avfilter_graph_free(&filter_ctx[i].filter_graph);
            av_packet_free(&filter_ctx[i].enc_pkt);
            av_frame_free(&filter_ctx[i].filtered_frame);
        }

        av_frame_free(&stream_ctx[i].dec_frame);
    }
    close();

}






