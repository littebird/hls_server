#include "encoder.h"

Encoder::Encoder()
{

}

void Encoder::init()
{
    //分配解复用器上下文内存
    inFormatCtx=avformat_alloc_context();
    if(!inFormatCtx){
        std::cout<<"error can't create allocate context \n";
        return ;
    }

    //分配音频解码器上下文内存
    pAudioCodecCtx=avcodec_alloc_context3(nullptr);
    if(!pAudioCodecCtx){
        std::cout<<"error avcodec_alloc_context3: \n";
        return ;
    }

    pVideoCodecCtx=avcodec_alloc_context3(nullptr);
    if(!pVideoCodecCtx){
        std::cout<<"error avcodec_alloc_context3: \n";
        return ;
    }

}

void Encoder::init_filter(FilteringContext *fctx, AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, const char *filter_spec)
{
    char args[512];
    int ret=0;
    AVFilter *bufferSrc;
    AVFilter *bufferSink;
    AVFilterContext *bufferSrc_ctx;
    AVFilterContext *bufferSink_ctx;
    AVFilterInOut *intputs=avfilter_inout_alloc();
    AVFilterInOut *outputs=avfilter_inout_alloc();
    AVFilterGraph *filter_graph=avfilter_graph_alloc();
    if(!intputs||!outputs||!filter_graph){
        std::cout<<"filter init error \n";
        return;
    }

    if(dec_ctx->codec_type==AVMEDIA_TYPE_VIDEO){
        bufferSrc=const_cast<AVFilter *>(avfilter_get_by_name("buffer"));
        bufferSink=const_cast<AVFilter *>(avfilter_get_by_name("buffersink"));
        if(!bufferSink||!bufferSrc){
            std::cout<<"filtering source or sink element not found\n";
            return ;
        }

        snprintf(args, sizeof(args),
                        "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                        dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
                        dec_ctx->time_base.num, dec_ctx->time_base.den,
                        dec_ctx->sample_aspect_ratio.num,
                        dec_ctx->sample_aspect_ratio.den);

        ret=avfilter_graph_create_filter(&bufferSrc_ctx,bufferSrc,"in",args,nullptr,filter_graph);//创建过滤器实例并将其添加到现有的图形中
        if(ret<0){
            std::cout<<"Cannot create buffer source\n";
            return;
        }
        ret = avfilter_graph_create_filter(&bufferSink_ctx, bufferSink, "out",nullptr, nullptr, filter_graph);
        if(ret<0){
            std::cout<<"Cannot create buffer sink\n";
            return;
        }

        //设置AVOption
        ret=av_opt_set_bin(bufferSink_ctx,"pix_fmts",(uint8_t*) &enc_ctx->pix_fmt,sizeof(enc_ctx->pix_fmt),AV_OPT_SEARCH_CHILDREN);
        if(ret<0){
            std::cout<<"cannot set output pixel format\n";
            return ;
        }
    }else if(dec_ctx->codec_type==AVMEDIA_TYPE_AUDIO){
        bufferSrc = const_cast<AVFilter *>(avfilter_get_by_name("abuffer"));
        bufferSink = const_cast<AVFilter *>(avfilter_get_by_name("abuffersink"));
        if(!bufferSink||!bufferSrc){
            std::cout<<"filtering source or sink element not found\n";
            return ;
        }

        if(!dec_ctx->channel_layout){
            dec_ctx->channel_layout=av_get_default_channel_layout(dec_ctx->channels);//默认音频通道布局
        }

        snprintf(args, sizeof(args),
                 "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%I64x",
                 dec_ctx->time_base.num, dec_ctx->time_base.den, dec_ctx->sample_rate,
                 av_get_sample_fmt_name(dec_ctx->sample_fmt),
                 dec_ctx->channel_layout);

        ret=avfilter_graph_create_filter(&bufferSrc_ctx,bufferSrc,"in",args,nullptr,filter_graph);//创建过滤器实例并将其添加到现有的图形中
        if(ret<0){
            std::cout<<"Cannot create buffer source\n";
            return;
        }
        ret = avfilter_graph_create_filter(&bufferSink_ctx, bufferSink, "out",nullptr, nullptr, filter_graph);
        if(ret<0){
            std::cout<<"Cannot create buffer sink\n";
            return;
        }

        //设置AVOption
        ret = av_opt_set_bin(bufferSink_ctx, "sample_fmts",(uint8_t*)&enc_ctx->sample_fmt, sizeof(enc_ctx->sample_fmt),AV_OPT_SEARCH_CHILDREN);
        if(ret<0){
            std::cout<<"cannot set output sample format\n";
            return ;
        }

        ret = av_opt_set_bin(bufferSink_ctx, "channel_layouts",(uint8_t*)&enc_ctx->channel_layout,
                             sizeof(enc_ctx->channel_layout),AV_OPT_SEARCH_CHILDREN);
        if(ret<0){
            std::cout<<"cannot set output channel layouts\n";
            return ;
        }

        ret = av_opt_set_bin(bufferSink_ctx, "sample_rates",
                        (uint8_t*)&enc_ctx->sample_rate, sizeof(enc_ctx->sample_rate),
                        AV_OPT_SEARCH_CHILDREN);
        if(ret<0){
            std::cout<<"cannot set output sample rates\n";
            return ;
        }

    }
}

void Encoder::close()//释放资源
{
    avformat_free_context(inFormatCtx);
    avformat_free_context(outFormatCtx);
    av_bsf_free(&vbsf_h264_toannexb);
    av_bsf_free(&absf_aac_adtstoasc);
    avcodec_free_context(&pAudioCodecCtx);
    avcodec_free_context(&pVideoCodecCtx);
}


void Encoder::open_input_file(const char *file)
{
    //根据url打开码流，并分析选择匹配的解复用器
    int ret=avformat_open_input(&inFormatCtx,file,nullptr,nullptr);
    if(ret!=0){
        std::cout<<"error avformat_open_input: \n";
        return ;
    }

    //读取文件的数据获取流信息
    ret=avformat_find_stream_info(inFormatCtx,nullptr);
    if(ret<0){
        std::cout<<"error avformat_find_stream_info: \n";
        return ;
    }

    m_audioIndex=av_find_best_stream(inFormatCtx,AVMEDIA_TYPE_AUDIO,-1,-1,nullptr,0);
    if(m_audioIndex==-1){
        std::cout<<"error don't find a audio stream \n";
    }

    m_videoIndex=av_find_best_stream(inFormatCtx,AVMEDIA_TYPE_VIDEO,-1,-1,nullptr,0);
    if(m_audioIndex==-1){
        std::cout<<"error don't find a video stream \n";
    }

    //将流中解码器需要的信息AVcodecparameters添加到解码器context
    ret=avcodec_parameters_to_context(pAudioCodecCtx,inFormatCtx->streams[m_audioIndex]->codecpar);
    if(ret<0){
        std::cout<<"error avcodec_parameters_to_context: \n";
        return ;
    }

    ret=avcodec_parameters_to_context(pVideoCodecCtx,inFormatCtx->streams[m_videoIndex]->codecpar);
    if(ret<0){
        std::cout<<"error avcodec_parameters_to_context: \n";
        return ;
    }

    //通过解码器id找到对应解码器
    pAudioCodec=const_cast<AVCodec *>(avcodec_find_decoder(pAudioCodecCtx->codec_id));
    if(pAudioCodec==nullptr){
        std::cout<<"error audio codec not found \n";
        return ;
    }

    pVideoCodec=const_cast<AVCodec *>(avcodec_find_decoder(pVideoCodecCtx->codec_id));
    if(pVideoCodec==nullptr){
        std::cout<<"error video codec not found \n";
        return ;
    }

    avcodec_open2(pVideoCodecCtx,pVideoCodec,nullptr);//打开对应编解码器
    avcodec_open2(pAudioCodecCtx,pAudioCodec,nullptr);

    //文件信息
    std::cout<<"--------------- File Information ----------------\n";
    av_dump_format(inFormatCtx, 0, file, 0);
    std::cout<<"-------------------------------------------------\n";


}

void Encoder::open_output_file(const char *file)
{
    AVStream *outStream;//out流
    AVStream *inStream;//in流
    AVCodecContext *decode_ctx, *encode_ctx;
    AVCodec *encoder;
    int ret;

    //使用 avformat_alloc_output_context2 函数就可以根据文件名分配合适的 AVFormatContext 管理结构。
    avformat_alloc_output_context2(&outFormatCtx,nullptr,nullptr,file);
    if(!outFormatCtx){
        av_log(nullptr, AV_LOG_ERROR, "Could not create output context\n");
        return;
    }
    for(int i=0;i<inFormatCtx->nb_streams;i++){
        outStream=avformat_new_stream(outFormatCtx,nullptr);// 创建输出码流的AVStream
        if (!outStream) {
            av_log(nullptr, AV_LOG_ERROR, "Failed allocating output stream\n"); // 分配输出流失败
            return ;
        }

        inStream=inFormatCtx->streams[i];

        //将流中编解码器需要的信息AVcodecparameters添加到编解码器context
        avcodec_parameters_to_context(decode_ctx,inStream->codecpar);
        avcodec_parameters_to_context(encode_ctx,outStream->codecpar);

        if(decode_ctx->codec_type==AVMEDIA_TYPE_AUDIO||decode_ctx->codec_type==AVMEDIA_TYPE_VIDEO){
            encoder=const_cast<AVCodec *>(avcodec_find_decoder(decode_ctx->codec_id));//找到编码器

            if(decode_ctx->codec_type==AVMEDIA_TYPE_VIDEO){//视频
                encode_ctx->height=decode_ctx->height;//高
                encode_ctx->width=decode_ctx->width;//宽
                encode_ctx->sample_aspect_ratio=decode_ctx->sample_aspect_ratio;// 长宽比
                encode_ctx->pix_fmt=encoder->pix_fmts[0]; //从支持的像素格式列表中获取第一种格式
                encode_ctx->time_base=decode_ctx->time_base;
            }else{//音频
                encode_ctx->sample_rate=decode_ctx->sample_rate;//每秒采样数
                encode_ctx->channel_layout=decode_ctx->channel_layout;//音频通道布局
                encode_ctx->channels=av_get_channel_layout_nb_channels(encode_ctx->channel_layout);//音频通道数
                encode_ctx->sample_fmt=encoder->sample_fmts[0];//从支持的采样格式列表中获取第一种格式
                encode_ctx->time_base={1,encode_ctx->sample_rate};
            }

            ret=avcodec_open2(encode_ctx,encoder,nullptr);//打开编码器
            if (ret < 0) {
                av_log(nullptr, AV_LOG_ERROR, "Cannot open video encoder for stream #%u\n", i); // 无法打开流的视频编码器
                return ;
            }

        }

        if(outFormatCtx->flags&AVFMT_GLOBALHEADER){
            encode_ctx->flags|=AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        if (!(outFormatCtx->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open(&outFormatCtx->pb, file, AVIO_FLAG_WRITE); //打开FFmpeg输出文件
            if (ret < 0) {
                av_log(nullptr, AV_LOG_ERROR, "Could not open output file '%s'", file); // 无法打开输出文件
                return ;
            }
        }
    }

    ret=avformat_write_header(outFormatCtx,nullptr); //写入输出文件头
    if(ret<0){
        av_log(nullptr, AV_LOG_ERROR, "Error occurred when opening output file\n"); // 打开输出文件时出错
        return ;
    }
}

AVStream *Encoder::add_out_stream(AVFormatContext *outCtx, AVMediaType type)
{
    AVStream* in=nullptr;
    AVStream* out=nullptr;
    AVCodecParameters* outCodecPar=nullptr;

    out=avformat_new_stream(outCtx,nullptr);
    if(!out){
        return nullptr;
    }

    switch (type) {
        case AVMEDIA_TYPE_AUDIO:
            in=inFormatCtx->streams[m_audioIndex];
            break;
        case AVMEDIA_TYPE_VIDEO:
            in=inFormatCtx->streams[m_videoIndex];
            break;
        default:
            break;
    }

    out->time_base=in->time_base;
    outCodecPar=out->codecpar;

    //拷贝输入流相关参数到out
    int ret=avcodec_parameters_copy(out->codecpar,in->codecpar);
    if(ret<0){
        std::cout<<"copy codec params to filter failed\n";
        return nullptr;
    }

    outCodecPar->codec_tag=0;

    if(AVFMT_GLOBALHEADER & outCtx->oformat->flags){

    }


    return out;
}

void Encoder::conduct_ts()
{

    if(strstr(inFormatCtx->iformat->name,"flv")!=nullptr||
            strstr(inFormatCtx->iformat->name,"mp4")!=nullptr||strstr(inFormatCtx->iformat->name,"mov")!=nullptr){
        if(pVideoCodec->id==AV_CODEC_ID_H264){//初始化h264,aac过滤器
            const AVBitStreamFilter *p=av_bsf_get_by_name("h264_mp4toannexb");
            av_bsf_alloc(p,&vbsf_h264_toannexb);

            std::cout<<"video bsf alloc success\n";
        }
        if(pAudioCodec->id==AV_CODEC_ID_AAC){
            const AVBitStreamFilter *p=av_bsf_get_by_name("aac_adtstoasc");
            av_bsf_alloc(p,&absf_aac_adtstoasc);

            std::cout<<"audio bsf alloc success\n";
        }

    }

    const char* output_name="../hls_server/test.ts";

    avformat_alloc_output_context2(&outFormatCtx,nullptr,nullptr,output_name);
    const AVOutputFormat* oformat=outFormatCtx->oformat;

    if(!(oformat->flags&AVFMT_NOFILE)){
        if(avio_open(&outFormatCtx->pb,output_name,AVIO_FLAG_WRITE)<0){
            std::cout<<"could't open file\n";
            return ;
        }
    }


    //添加视频信息到输出ctx,须先添加视频流
    if(m_videoIndex!=-1){
        video_stream=add_out_stream(outFormatCtx,AVMEDIA_TYPE_VIDEO);
        std::cout<<"add video stream \n";
    }
    //添加音频信息到输出ctx
    if(m_audioIndex!=-1){
        audio_stream=add_out_stream(outFormatCtx,AVMEDIA_TYPE_AUDIO);
        std::cout<<"add audio stream \n";
    }

    int ret=avformat_write_header(outFormatCtx,nullptr);
    if(ret!=0){
        std::cout<<"write header error\n";
        return ;
    }


    AVPacket *packet=av_packet_alloc();

    while(!av_read_frame(inFormatCtx,packet)){

//        std::cout<<"read pkt \n";

        if(packet->stream_index==m_videoIndex){//视频

            packet->pts=av_rescale_q_rnd(packet->pts,inFormatCtx->streams[m_videoIndex]->time_base,video_stream->time_base,AV_ROUND_INF);
            packet->dts=av_rescale_q_rnd(packet->dts,inFormatCtx->streams[m_videoIndex]->time_base,video_stream->time_base,AV_ROUND_INF);
            packet->duration=av_rescale_q(packet->duration,inFormatCtx->streams[m_videoIndex]->time_base,video_stream->time_base);
            packet->pos=-1;

            if(vbsf_h264_toannexb!=nullptr){
                AVPacket bsfPkt=*packet;

                av_bsf_send_packet(vbsf_h264_toannexb,&bsfPkt);
                av_bsf_receive_packet(vbsf_h264_toannexb,&bsfPkt);
                packet->data=bsfPkt.data;
                packet->size=bsfPkt.size;

//                std::cout<<"video success\n";
            }
        }else if(packet->stream_index==m_audioIndex){//音频

            packet->pts=av_rescale_q_rnd(packet->pts,inFormatCtx->streams[m_audioIndex]->time_base,audio_stream->time_base,AV_ROUND_INF);
            packet->dts=av_rescale_q_rnd(packet->dts,inFormatCtx->streams[m_audioIndex]->time_base,audio_stream->time_base,AV_ROUND_INF);
            packet->duration=av_rescale_q(packet->duration,inFormatCtx->streams[m_audioIndex]->time_base,audio_stream->time_base);
            packet->pos=-1;

//            std::cout<<"audio success\n";
        }

        int res=av_write_frame(outFormatCtx,packet);
        if(res<0){
            std::cout<<"Could not write frame of stream\n";
            break;
        }else if(res>0){
            std::cout<<"end of stream \n";
            break;
        }

        av_packet_unref(packet);
    }


    int res=av_write_trailer(outFormatCtx);//写文件尾
    if(res<0){
        std::cout<<"Could not av_write_trailer of stream\n";
        return ;

    }
    avio_flush(outFormatCtx->pb);
    avio_close(outFormatCtx->pb);
}


