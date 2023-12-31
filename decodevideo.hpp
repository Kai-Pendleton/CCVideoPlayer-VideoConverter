#ifndef DECODEVIDEO_HPP_INCLUDED
#define DECODEVIDEO_HPP_INCLUDED

#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

//av_err2str
static const std::string av_make_error_string(int errnum)
{
char errbuf[AV_ERROR_MAX_STRING_SIZE];
av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
return (std::string)errbuf;
}
#undef av_err2str
#define av_err2str(errnum) av_make_error_string(errnum).c_str()

// Different architectures have different values that ffmpeg likes to round its linesizes to. This is handled manually here.
#ifndef ALIGNMENT
#define ALIGNMENT 64
#endif

void scaleImage(AVFrame * pFrame, int scaleX, int scaleY, AVFrame * pScaledFrame, AVPixelFormat pixfmt);

// Writes a simple .ppm image. Must be given BGRA pixels. Does not bounds check.
int writePPM(std::string outputFileName, int width, int height, uint8_t *data, bool isPadded);
int writePal8PPM(std::string outputFileName, int width, int height, uint8_t *data, uint8_t *palette);


class VideoDecoder {

public:
    VideoDecoder(int width, int height, int frameRate, std::string inputFileName) {

        framesProcessed = 0; // Total number of frames decoded
        framesReturned = 0; // Total number of frames returned to the caller of readFrame. Always lower than framesProcessed since outputFrameRate will (almost) always be lower
        padCount = (ALIGNMENT-(width%ALIGNMENT))%ALIGNMENT;
        frameSizeInBytes = (width+padCount) * height * 4; // BGRA

        this->width = width;
        this->height = height;
        this->outputFrameRate = frameRate;
        this->inputFileName = inputFileName;
        filterDescription = std::string("scale=w=") + std::to_string(width) + ":h=" + std::to_string(height) + ":flags=bicubic,format=pix_fmts=" + av_get_pix_fmt_name(AV_PIX_FMT_RGB24);



        openInputFile();
        initializeFilters();
        pAVPacket = av_packet_alloc();
        pFrame = av_frame_alloc();
        pRGBFrame = av_frame_alloc();
        RGBBuffer = new uint8_t[frameSizeInBytes];
        av_image_fill_arrays(pRGBFrame->data, pRGBFrame->linesize, RGBBuffer, AV_PIX_FMT_BGRA, width, height, ALIGNMENT);
        frameBuffer = new uint8_t[frameSizeInBytes];
        av_image_fill_arrays(pFrame->data, pFrame->linesize, frameBuffer, pCodecContext->pix_fmt, pCodecContext->width, pCodecContext->height, ALIGNMENT);

        inputFrameRate = av_q2d(pFormatContext->streams[videoStreamIndex]->avg_frame_rate);

    }

    ~VideoDecoder() {
        // Free up used resources

        av_frame_free(&pFrame);
        av_frame_free(&pRGBFrame);
        av_packet_free(&pAVPacket);
        delete[] frameBuffer;
        delete[] RGBBuffer;
        delete[] resultBuffer;
        avformat_close_input(&pFormatContext);
    }

    uint8_t *readFrame();
    bool seekFrame(int frameNumber);
    void printVideoInfo();
    double getFrameRate();
    int frameSizeInBytes;

private:

    int framesProcessed;
    int framesReturned;
    int width;
    int height;
    int outputFrameRate;
    std::string inputFileName;



    AVFormatContext * pFormatContext;
    const AVCodec * pVideoCodec;
    int videoStreamIndex;
    AVCodecContext * pCodecContext;
    int result;
    AVPacket * pAVPacket;
    AVFrame * pFrame;
    AVFrame * pRGBFrame;
    int scaledBufferByteCount;
    double inputFrameRate;
    uint8_t * resultBuffer;
    uint8_t * frameBuffer;
    uint8_t * RGBBuffer;


    AVFilterContext * pBufferSinkContext;
    AVFilterContext * pBufferSrcContext;
    AVFilterGraph * pFilterGraph;
    std::string filterDescription;

    int padCount;


    int openInputFile();
    int initializeFilters();

};







#endif // DECODEVIDEO_HPP_INCLUDED
