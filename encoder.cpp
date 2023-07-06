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
    AVFilterInOut *inputs=avfilter_inout_alloc();
    AVFilterInOut *outputs=avfilter_inout_alloc();
    AVFilterGraph *filter_graph=avfilter_graph_alloc();
    if(!inputs||!outputs||!filter_graph){
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
        ret = avfilter_graph_create_filter(&bufferSink_ctx, bufferSink, "out",nullptr, nullptr, filter_graph);//创建过滤器实例并将其添加到现有的图形中
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

    //设置inputs和outputs结构
    outputs->name=av_strdup("in");
    outputs->filter_ctx=bufferSrc_ctx;
    outputs->pad_idx=0;
    outputs->next=nullptr;
    inputs->name=av_strdup("out");
    inputs->filter_ctx=bufferSink_ctx;
    inputs->pad_idx=0;
    inputs->next=nullptr;
    if(!outputs->name||!inputs->name){
        std::cout<<"filter init error \n";
        return;
    }

    //将由字符串描述的图形添加到图形中
    if((ret=avfilter_graph_parse_ptr(filter_graph,filter_spec,&inputs,&outputs,nullptr))<0){
        return ;
    }

    //检查有效性并配置图中的所有链接和格式
    if((ret=avfilter_graph_config(filter_graph,nullptr))<0){
        return ;
    }

    //填充FilteringContext
    fctx->bufferSink_ctx=bufferSink_ctx;
    fctx->bufferSrc_ctx=bufferSrc_ctx;
    fctx->filter_graph=filter_graph;

    //释放资源
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

}

void Encoder::init_filters()
{
    const char* filter_spec;
    AVCodecContext *dec_ctx;
    AVCodecContext *enc_ctx;

    dec_ctx=avcodec_alloc_context3(nullptr);
    enc_ctx=avcodec_alloc_context3(nullptr);
    if(!dec_ctx||!enc_ctx){
        std::cout<<"error avcodec_alloc_context3: \n";
        return ;
    }

    filter_ctx=(FilteringContext *)av_malloc_array(inFormatCtx->nb_streams,sizeof(*filter_ctx));
    if(!filter_ctx){
        std::cout<<"filters init error \n";
        return;
    }

    for(int i=0;i<inFormatCtx->nb_streams;i++){
        filter_ctx[i].bufferSink_ctx=nullptr;
        filter_ctx[i].bufferSrc_ctx=nullptr;
        filter_ctx[i].filter_graph=nullptr;
        if(!(inFormatCtx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO||inFormatCtx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO))
            continue;
        if(inFormatCtx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
            filter_spec="null";//视频直通（虚拟）滤波器
        }else filter_spec="anull";//音频直通（虚拟）滤波器

        avcodec_parameters_to_context(dec_ctx,inFormatCtx->streams[i]->codecpar);
        avcodec_parameters_to_context(enc_ctx,outFormatCtx->streams[i]->codecpar);

        init_filter(filter_ctx,dec_ctx,enc_ctx,filter_spec);

    }
}

void Encoder::close()//释放资源
{
    av_free(filter_ctx);
    avformat_free_context(inFormatCtx);
    avformat_free_context(outFormatCtx);
//    av_bsf_free(&vbsf_h264_toannexb);
//    av_bsf_free(&absf_aac_adtstoasc);
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
        std::cout<<"Could not create output context\n";
        return;
    }

    decode_ctx=avcodec_alloc_context3(nullptr);
    encode_ctx=avcodec_alloc_context3(nullptr);
    if(!decode_ctx||!encode_ctx){
        std::cout<<"error avcodec_alloc_context3: \n";
        return ;
    }


    for(int i=0;i<inFormatCtx->nb_streams;i++){
        outStream=avformat_new_stream(outFormatCtx,nullptr);// 创建输出码流的AVStream
        if (!outStream) {
            std::cout<<"Failed allocating output stream\n";// 分配输出流失败
            return ;
        }

        inStream=inFormatCtx->streams[i];

        //将流中编解码器需要的信息AVcodecparameters添加到编解码器context
        ret=avcodec_parameters_to_context(decode_ctx,inStream->codecpar);
        if(ret<0){
            std::cout<<"error avcodec_parameters_to_context: \n";
            return ;
        }
        ret=avcodec_parameters_to_context(encode_ctx,outStream->codecpar);
        if(ret<0){
            std::cout<<"error avcodec_parameters_to_context: \n";
            return ;
        }


        if(decode_ctx->codec_type==AVMEDIA_TYPE_AUDIO||decode_ctx->codec_type==AVMEDIA_TYPE_VIDEO){

            encoder=const_cast<AVCodec *>(avcodec_find_encoder(decode_ctx->codec_id));//找到编码器



            if(decode_ctx->codec_type==AVMEDIA_TYPE_VIDEO){//视频

                encode_ctx->height=decode_ctx->height;//高
                encode_ctx->width=decode_ctx->width;//宽
                encode_ctx->sample_aspect_ratio=decode_ctx->sample_aspect_ratio;// 长宽比

                encode_ctx->pix_fmt=encoder->pix_fmts[0]; //从支持的像素格式列表中获取第一种格式

//                encode_ctx->time_base=decode_ctx->time_base;

            }else{//音频
                encode_ctx->sample_rate=decode_ctx->sample_rate;//每秒采样数
                encode_ctx->channel_layout=decode_ctx->channel_layout;//音频通道布局
                encode_ctx->channels=av_get_channel_layout_nb_channels(encode_ctx->channel_layout);//音频通道数
                encode_ctx->sample_fmt=encoder->sample_fmts[0];//从支持的采样格式列表中获取第一种格式
                encode_ctx->time_base={1,encode_ctx->sample_rate};
            }
std::cout<<encode_ctx->time_base.den<<" 1111111\n";
            ret=avcodec_open2(encode_ctx,encoder,nullptr);//打开编码器
            if (ret < 0) {
                std::cout<<"Cannot open  encoder for stream\n";// 无法打开流的编码器
                return ;
            }

        }

        if(outFormatCtx->flags&AVFMT_GLOBALHEADER){
            encode_ctx->flags|=AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        if (!(outFormatCtx->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open(&outFormatCtx->pb, file, AVIO_FLAG_WRITE); //打开FFmpeg输出文件
            if (ret < 0) {
                std::cout<<"Could not open output file \n";// 无法打开输出文件
                return ;
            }
        }
    }

    ret=avformat_write_header(outFormatCtx,nullptr); //写入输出文件头
    if(ret<0){
        std::cout<<"Error occurred when opening output file\n";// 打开输出文件时出错
        return ;
    }
}

int Encoder::encode_write_frame(AVFrame *filter_frame, int stream_idx, int got_pkt)
{
    int ret;
    AVPacket enc_pkt;
    AVCodecContext *enc_ctx;

    enc_pkt.data=nullptr;
    enc_pkt.size=0;
    av_init_packet(&enc_pkt);//初始化数据包

    enc_ctx=avcodec_alloc_context3(nullptr);
    if(!enc_ctx){
        std::cout<<"error avcodec_alloc_context3: \n";
        return -1;
    }

    avcodec_parameters_to_context(enc_ctx,outFormatCtx->streams[stream_idx]->codecpar);

    ret=avcodec_send_frame(enc_ctx,filter_frame);
    if(ret<0){
        std::cout<<"Error encoding send frame\n";
        return ret;
    }

    got_pkt=avcodec_receive_packet(enc_ctx,&enc_pkt);

    av_frame_free(&filter_frame);//释放file_frame
    if(!got_pkt){//0即成功收到packet

        //准备enc_pkt
        enc_pkt.stream_index=stream_idx;
        //将64位整数重缩放为2个具有指定舍入的有理数
        enc_pkt.dts=av_rescale_q_rnd(enc_pkt.dts,outFormatCtx->streams[stream_idx]->time_base,
                                     outFormatCtx->streams[stream_idx]->time_base,
                                     (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        enc_pkt.pts=av_rescale_q_rnd(enc_pkt.pts,outFormatCtx->streams[stream_idx]->time_base,
                                     outFormatCtx->streams[stream_idx]->time_base,
                                     (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        enc_pkt.duration=av_rescale_q(enc_pkt.duration,outFormatCtx->streams[stream_idx]->time_base,outFormatCtx->streams[stream_idx]->time_base);


        ret=av_interleaved_write_frame(outFormatCtx,&enc_pkt);//将数据包写入输出媒体文件

        return ret;
    }else {
        return ret;
    }

}

void Encoder::filter_encode_write_frame(AVFrame *frame, int stream_idx)//过滤编码写入帧
{
    int ret;
    AVFrame *filter_frame;

    ret=av_buffersrc_add_frame_flags(filter_ctx[stream_idx].bufferSrc_ctx,frame,-1); //将解码后的帧推入 filtergraph
    if(ret<0){
        std::cout<<"Error while feeding the filtergraph\n";
        return;
    }

    //从filtergraph 中拉出过滤的帧
    while(1){
        filter_frame=av_frame_alloc();
        if(!filter_frame){
            return;
        }

        // av_buffersink_get_frame 从接收器获取一个带有过滤数据的帧,并将其放入帧中
        ret=av_buffersink_get_frame(filter_ctx[stream_idx].bufferSink_ctx,filter_frame);
        if(ret<0){
            if(ret==AVERROR(EAGAIN)||ret==AVERROR_EOF){//如果没有更多的输出帧 - returns AVERROR(EAGAIN)||如果刷新并且没有更多的帧用于输出 - returns AVERROR_EOF
                ret=0;
            }
            av_frame_free(&filter_frame); //释放 filt_frame
            break;
        }

        filter_frame->pict_type=AV_PICTURE_TYPE_NONE;

        ret=encode_write_frame(filter_frame,stream_idx,-1);

        if(ret<0)break;
    }

}

void Encoder::flush_encoder(int stream_idx)
{
    int ret;
    while(1){
        ret=encode_write_frame(nullptr,stream_idx,0);
        if(ret<0)
            break;
    }
}

void Encoder::VOD(const char *inputfile)
{
    int ret;
    int stream_idx;
    AVPacket *packet;
    AVFrame *frame;
    AVCodecContext *dec_ctx;

    init();//初始化
    open_input_file(inputfile);//打开输入文件
    open_output_file("../hls_server/test.avi");//打开输出文件
    init_filters();//初始化AVFilter相关的结构体

    dec_ctx=avcodec_alloc_context3(nullptr);
    if(!dec_ctx){
        std::cout<<"error avcodec_alloc_context3: \n";
        return ;
    }

    //循环读取文件中所有数据包
    while(1){
        if((ret=av_read_frame(inFormatCtx,packet))<0)
            break;
        stream_idx=packet->stream_index;

        if(filter_ctx[stream_idx].filter_graph){
            //重新编码和过滤帧
            frame=av_frame_alloc();
            if(!frame){
                ret = AVERROR(ENOMEM);
                break;
            }
            //将64位整数重缩放为2个具有指定舍入的有理数。
            packet->dts=av_rescale_q_rnd(packet->dts,inFormatCtx->streams[stream_idx]->time_base,
                                        inFormatCtx->streams[stream_idx]->time_base,(AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
            packet->pts=av_rescale_q_rnd(packet->pts,inFormatCtx->streams[stream_idx]->time_base,
                                        inFormatCtx->streams[stream_idx]->time_base,(AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
            //视频解码或者音频解码

            avcodec_parameters_to_context(dec_ctx,inFormatCtx->streams[stream_idx]->codecpar);
            ret=avcodec_send_packet(dec_ctx,packet);
            if(ret<0){
                std::cout<<"Decoding failed\n";
                break;
            }

            if((avcodec_receive_frame(dec_ctx,frame)!=0)){
                av_frame_free(&frame);
            }else {
//                frame->pts=av_frame_get_best_effort_timestamp(frame);

                filter_encode_write_frame(frame,stream_idx);//编码一帧

            }

        }else{
            //重新复制此帧而不重新编码
            packet->dts=av_rescale_q_rnd(packet->dts,inFormatCtx->streams[stream_idx]->time_base,
                                        outFormatCtx->streams[stream_idx]->time_base,(AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
            packet->pts=av_rescale_q_rnd(packet->pts,inFormatCtx->streams[stream_idx]->time_base,
                                        outFormatCtx->streams[stream_idx]->time_base,(AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));

            ret=av_interleaved_write_frame(outFormatCtx,packet);// 将数据包写入输出媒体文件
            if(ret<0)
                return ;
        }
        av_packet_free(&packet);
    }

    //刷新过滤器和编码器
    for(int i=0;i<inFormatCtx->nb_streams;i++){
        //刷新过滤器
        if(!filter_ctx[i].filter_graph){
            continue;
        }
        filter_encode_write_frame(nullptr,i);

        //刷新编码器
        flush_encoder(i);
    }
    av_write_trailer(outFormatCtx);

    av_packet_free(&packet);
    av_frame_free(&frame);
    close();

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


