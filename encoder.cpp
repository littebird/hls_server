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

void Encoder::VOD(std::string file)
{
    //根据url打开码流，并分析选择匹配的解复用器
    int ret=avformat_open_input(&inFormatCtx,file.c_str(),nullptr,nullptr);
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


//    //初始化打开解码器
//    avcodec_open2(pAudioCodecCtx,pAudioCodec,nullptr);
//    avcodec_open2(pVideoCodecCtx,pVideoCodec,nullptr);


    //文件信息
    std::cout<<"--------------- File Information ----------------\n";
    av_dump_format(inFormatCtx, 0, file.c_str(), 0);
    std::cout<<"-------------------------------------------------\n";


    if(strstr(inFormatCtx->iformat->name,"flv")!=nullptr||
            strstr(inFormatCtx->iformat->name,"mp4")!=nullptr||strstr(inFormatCtx->iformat->name,"mov")!=nullptr){
        if(pVideoCodec->id==AV_CODEC_ID_H264){
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

    //添加音频信息到输出ctx
    if(m_audioIndex!=-1){
        audio_stream=add_out_stream(outFormatCtx,AVMEDIA_TYPE_AUDIO);
        std::cout<<"add audio\n";
    }
    //添加视频信息到输出ctx
    if(m_videoIndex!=-1){
        video_stream=add_out_stream(outFormatCtx,AVMEDIA_TYPE_VIDEO);
        std::cout<<"add video\n";
    }


    ret=avformat_write_header(outFormatCtx,nullptr);
    if(ret!=0){
        std::cout<<"write header error\n";
        return ;
    }


    AVPacket *packet=av_packet_alloc();
    int res=0;
    while(!av_read_frame(inFormatCtx,packet)){
        std::cout<<"read pkt\n";
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

    out->id=outFormatCtx->nb_streams-1;
    out->time_base=in->time_base;
    outCodecPar=out->codecpar;

    //拷贝输入流相关参数到过滤器的AVBSFContext
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


