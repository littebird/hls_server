#include "segmenter.h"

segmenter::segmenter(const char *inpath)
    :input_filename{inpath},
      output_index{1},
      ifmt_ctx{nullptr},
      ofmt_ctx{nullptr},
      video_st{nullptr},
      audio_st{nullptr},
      video_stream_idx{-1},
      audio_stream_idx{-1},
      vbsf_h264_toannexb{nullptr}

{


}
segmenter::~segmenter()
{

}

int segmenter::init()
{
    int ret;
    //打开一个输入流，并读取其文件头
    if ((ret=avformat_open_input(&ifmt_ctx, input_filename, 0, 0)) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Cannot open the input file %s\n", input_filename);
        return 0;
    }

    //读取文件的所有packets，获取流信息
    //当有些格式没有头文件，调用此函数来尝试读取和解码几个帧以查找丢失的信息
    if ((ret=avformat_find_stream_info(ifmt_ctx,0)) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to retrieve input stream information\n");
        return 0;
    }

    //打印输入格式的详细信息
    av_dump_format(ifmt_ctx, 0, input_filename, 0);

    //添加音频信息到输出context
    for(int i=0;i<ifmt_ctx->nb_streams;i++)
    {
        if(ifmt_ctx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO)
        {

            video_stream_idx = i;
        }
        else if(ifmt_ctx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO)
        {
            audio_stream_idx=i;
        }
    }

    if(ifmt_ctx->streams[video_stream_idx]->codecpar->codec_id==AV_CODEC_ID_H264)
    {
        const AVBitStreamFilter *p=av_bsf_get_by_name("h264_mp4toannexb");
        av_bsf_alloc(p,&vbsf_h264_toannexb);
    }
    else if(ifmt_ctx->streams[audio_stream_idx]->codecpar->codec_id==AV_CODEC_ID_AAC)
    {
        std::cout<<"to do"<<std::endl;
    }

    return 1;
}

int segmenter::init_mux()
{
    int ret;

    //为输出格式初始化AVFmormatContext指针
    if (avformat_alloc_output_context2(&ofmt_ctx, nullptr, nullptr, output_filename) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Cannot open the file %s\n", output_filename);
        return 0;
    }

    //打开输出文件
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        if(avio_open2(&ofmt_ctx->pb, output_filename, AVIO_FLAG_WRITE, &ofmt_ctx->interrupt_callback, nullptr) < 0) {
            av_log(nullptr, AV_LOG_ERROR, "cannot open the output file '%s'\n",output_filename);
            return 0;
        }
    }




    //添加音频信息到输出context
    if(audio_stream_idx != -1)
    {
        audio_st = add_out_stream(ofmt_ctx, AVMEDIA_TYPE_AUDIO);
    }
    //添加视频信息到输出context
    if (video_stream_idx != -1)//如果存在视频
    {
        video_st = add_out_stream(ofmt_ctx,AVMEDIA_TYPE_VIDEO);
    }


    //打印输出格式信息
    av_dump_format(ofmt_ctx, 0, output_filename, 1);

    //写文件头
    if ((ret = avformat_write_header(ofmt_ctx, nullptr)) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Cannot write the header for the file '%s'\n",output_filename);
        return 0;
    }

    return 1;
}

AVStream* segmenter::add_out_stream(AVFormatContext* output_format_context,AVMediaType codec_type_t)
{
    AVStream *in_stream=nullptr;
    AVStream *out_stream=nullptr;

    out_stream = avformat_new_stream(output_format_context,nullptr);
    if(!out_stream)
        return nullptr;

    switch(codec_type_t)
    {
    case AVMEDIA_TYPE_AUDIO:
    {
        in_stream = ifmt_ctx->streams[audio_stream_idx];
        break;
    }
    case AVMEDIA_TYPE_VIDEO:
    {
        in_stream=ifmt_ctx->streams[video_stream_idx];
        break;
    }
    defult:
        break;
    }
    AVCodecParameters *in_codecpar=in_stream->codecpar;
    out_stream->id = output_format_context->nb_streams-1;

    out_stream->time_base  = in_stream->time_base;
    //将输入的AVCodecParameters指针内存拷贝到输出的AVCodecParameters中
    int ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to copy codec parameters\n");
        return 0;
    }

    out_stream->codecpar->codec_tag = 0;

    if(AVFMT_GLOBALHEADER & output_format_context->oformat->flags)
    {
        output_format_context->flags |=AV_CODEC_FLAG_GLOBAL_HEADER;
        std::cout<<"to do"<<std::endl;
    }

    return out_stream;
}

int segmenter::write_index_file(const unsigned int first_segment, const unsigned int last_segment,
                                const int end, const int actual_segment_durations[])
{
    //文件写操作
    std::ofstream m_file;
    char *buf=nullptr;

    m_file.open("../hls_server/seg_test.m3u8",std::ios::out);
    if (!m_file.is_open()){
        std::cout << "cannot open file!"<<std::endl;
        return -1;
    }

    if(NUM_SEGMENTS)
    {
        m_file<<"#EXTM3U"<<"\n";
        m_file<<"#EXT-X-TARGETDURATION:"<<SEGMENT_DURATION<<"\n";
        m_file<<"#EXT-X-MEDIA-SEQUENCE:"<<first_segment<<"\n";
    }
    else
    {
        m_file<<"#EXTM3U"<<"\n";
        m_file<<"#EXT-X-TARGETDURATION:"<<SEGMENT_DURATION<<"\n";
    }

    for(int i=first_segment;i<=last_segment;i++)
    {
        m_file<<"#EXTINF:"<<actual_segment_durations[i-1]<<",\n";
        m_file<<OUTPUT_PREFIX<<i<<".ts"<<"\n";
    }

    if(end)
    {
        m_file<<"#EXT-X-ENDLIST"<<"\n";
    }

    m_file.close();//关闭文件

    return 0;
}

void segmenter::slice()
{
    int write_index = 1;
    unsigned int first_segment = 1;     //第一个分片的标号
    unsigned int last_segment = 0;      //最后一个分片标号
    int remove_file = 0;                //是否要移除文件
    int actual_segment_durations[1024]={0};//各个分片文件实际的长度
    const char* remove_filename;    //要从磁盘上删除的文件名称
    double prev_segment_time = 0;       //上一个分片时间
    int ret=0;

    std::string name="../hls_server/seg_test"+std::to_string(output_index)+".ts";
    output_index++;
    output_filename=name.c_str();
    init_mux();

    write_index=!write_index_file(first_segment,last_segment,0,actual_segment_durations);

    do{
        unsigned int current_segment_duration;
        double segment_time=prev_segment_time;
        AVPacket packet;


        ret = av_read_frame(ifmt_ctx, &packet);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "last frame\n");
            break;
        }

        if(packet.stream_index==video_stream_idx)
        {
            segment_time=packet.pts*av_q2d(ifmt_ctx->streams[video_stream_idx]->time_base);
        }
        else if(video_stream_idx<0)
        {
            segment_time=packet.pts*av_q2d(ifmt_ctx->streams[audio_stream_idx]->time_base);
        }
        else
        {
            segment_time=prev_segment_time;
        }

        if(packet.pts<packet.dts)
        {
            packet.pts=packet.dts;
        }

        //视频
        if(packet.stream_index==video_stream_idx)
        {
            if(vbsf_h264_toannexb!=nullptr)
            {
                AVPacket filteredpacket=packet;
                av_bsf_send_packet(vbsf_h264_toannexb, &filteredpacket);
                av_bsf_receive_packet(vbsf_h264_toannexb, &filteredpacket);

                packet.pts=filteredpacket.pts;
                packet.dts=filteredpacket.dts;
                packet.duration=filteredpacket.duration;
                packet.flags=filteredpacket.flags;
                packet.stream_index=filteredpacket.stream_index;
                packet.data=filteredpacket.data;
                packet.size=filteredpacket.size;
            }
            packet.pts=av_rescale_q_rnd(packet.pts, ifmt_ctx->streams[video_stream_idx]->time_base,
                                        video_st->time_base, AV_ROUND_NEAR_INF);
            packet.dts=av_rescale_q_rnd(packet.dts, ifmt_ctx->streams[video_stream_idx]->time_base,
                                        video_st->time_base, AV_ROUND_NEAR_INF);
            packet.duration=av_rescale_q(packet.duration,ifmt_ctx->streams[video_stream_idx]->time_base, video_st->time_base);

            packet.stream_index=1;
        }
        //音频
        else if(packet.stream_index==audio_stream_idx)
        {

            packet.pts=av_rescale_q_rnd(packet.pts, ifmt_ctx->streams[audio_stream_idx]->time_base,
                                        audio_st->time_base, AV_ROUND_NEAR_INF);
            packet.dts=av_rescale_q_rnd(packet.dts, ifmt_ctx->streams[audio_stream_idx]->time_base,
                                        audio_st->time_base, AV_ROUND_NEAR_INF);
            packet.duration=av_rescale_q(packet.duration,ifmt_ctx->streams[audio_stream_idx]->time_base, audio_st->time_base);

            packet.stream_index=0;

        }

        current_segment_duration=(int)(segment_time-prev_segment_time+0.5);
        actual_segment_durations[last_segment]=(current_segment_duration>0 ? current_segment_duration : 1);

        if(segment_time-prev_segment_time>=SEGMENT_DURATION)
        {
//            av_write_trailer(ofmt_ctx);//写文件尾，并关闭文件

            avio_flush(ofmt_ctx->pb);
            avio_close(ofmt_ctx->pb);

            if(NUM_SEGMENTS &&(int)(last_segment-first_segment)>=NUM_SEGMENTS-1)
            {

                remove_file=1;
                first_segment++;
            }
            else
            {
                remove_file=0;
            }

            if(write_index)
            {
                last_segment++;
                write_index=!write_index_file(first_segment,last_segment,0,actual_segment_durations);
            }

            if(remove_file)
            {
                std::string movename="../hls_server/seg_test"+std::to_string(first_segment-1)+".ts";
                remove_filename=movename.c_str();
                remove(remove_filename);

            }

            std::string name="../hls_server/seg_test"+std::to_string(output_index)+".ts";
            output_index++;
            output_filename=name.c_str();
            std::cout<<output_filename<<std::endl;
            if (avio_open2(&ofmt_ctx->pb, output_filename, AVIO_FLAG_WRITE, &ofmt_ctx->interrupt_callback, nullptr) < 0) {
                av_log(nullptr, AV_LOG_ERROR, "open file fault");
                break;
            }

            //写文件头
            if ((ret = avformat_write_header(ofmt_ctx, nullptr)) < 0) {
                av_log(nullptr, AV_LOG_ERROR, "write hrader fault");
            }

            prev_segment_time=segment_time;
        }

        //写packet
        ret = av_interleaved_write_frame(ofmt_ctx, &packet);

        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Muxing Error\n");
        }
        else if(ret>0)
        {
            av_packet_unref(&packet);
            av_log(nullptr, AV_LOG_ERROR, "end of stream requseted\n");
            break;
        }
        av_packet_unref(&packet);
    }while(1);

    uinit();

    if(NUM_SEGMENTS && (int)(last_segment-first_segment)>=NUM_SEGMENTS-1)
    {
        remove_file = 1;
        first_segment++;
    }
    else
    {
        remove_file=0;
    }

    if(write_index)
    {

       last_segment++;
       write_index_file(first_segment,last_segment,1,actual_segment_durations);
    }

    if(remove_file)
    {
        std::string movename="../hls_server/seg_test"+std::to_string(first_segment-1)+".ts";
        remove_filename=movename.c_str();
        remove(remove_filename);
    }

}

int segmenter::uinit()
{
    av_write_trailer(ofmt_ctx);//写文件尾，并关闭文件

    //释放AVBSFContext
    if(vbsf_h264_toannexb!=nullptr)
    {

        av_bsf_free(&vbsf_h264_toannexb);
        vbsf_h264_toannexb=nullptr;
    }

    //释放流
    for(int i=0;i<ofmt_ctx->nb_streams;i++)
    {
        av_freep(&ofmt_ctx->streams[i]->codecpar);
        av_freep(&ofmt_ctx->streams[i]);
    }

    if(!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        //关闭输出文件
        avio_close(ofmt_ctx->pb);
    }

    avformat_close_input(&ifmt_ctx);

    av_free(ofmt_ctx);

    return 1;
}

