#include "encoder.h"

Encoder::Encoder()
{

}

void Encoder::init()
{
    //分配解复用器上下文内存
    pFormatCtx=avformat_alloc_context();
    if(!pFormatCtx){
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
    int ret=avformat_open_input(&pFormatCtx,file.c_str(),nullptr,nullptr);
    if(ret!=0){
        std::cout<<"error avformat_open_input: \n";
        return ;
    }

    //读取文件的数据获取流信息
    ret=avformat_find_stream_info(pFormatCtx,nullptr);
    if(ret<0){
        std::cout<<"error avformat_find_stream_info: \n";
        return ;
    }

    m_audioIndex=av_find_best_stream(pFormatCtx,AVMEDIA_TYPE_AUDIO,-1,-1,nullptr,0);
    if(m_audioIndex==-1){
        std::cout<<"error don't find a audio stream \n";
    }

    m_videoIndex=av_find_best_stream(pFormatCtx,AVMEDIA_TYPE_VIDEO,-1,-1,nullptr,0);
    if(m_audioIndex==-1){
        std::cout<<"error don't find a video stream \n";
    }

    //将流中解码器需要的信息AVcodecparameters拷贝到解码器上下文
    ret=avcodec_parameters_to_context(pAudioCodecCtx,pFormatCtx->streams[m_audioIndex]->codecpar);
    if(ret<0){
        std::cout<<"error avcodec_parameters_to_context: \n";
        return ;
    }

    ret=avcodec_parameters_to_context(pVideoCodecCtx,pFormatCtx->streams[m_videoIndex]->codecpar);
    if(ret<0){
        std::cout<<"error avcodec_parameters_to_context: \n";
        return ;
    }

    //通过解码器id找到解码器
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


    //初始化打开解码器
    avcodec_open2(pAudioCodecCtx,pAudioCodec,nullptr);

    //文件信息
    std::cout<<"--------------- File Information ----------------\n";
    av_dump_format(pFormatCtx, 0, file.c_str(), 0);
    std::cout<<"-------------------------------------------------\n";
}


