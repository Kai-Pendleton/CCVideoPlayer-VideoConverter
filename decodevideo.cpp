#include "decodevideo.hpp"


void scaleImage(AVFrame * pFrame, int scaleX, int scaleY, AVFrame * pScaledFrame, AVPixelFormat pixfmt) {
    // Use swscale to convert to rgb
    SwsContext * pSwsContext = sws_getContext(pFrame->width, pFrame->height, (AVPixelFormat)pFrame->format, scaleX, scaleY, pixfmt, SWS_BICUBIC, NULL, NULL, NULL);
//    const AVOption *testOption = av_opt_find(pSwsContext, "sws_dither", NULL, NULL, NULL);
//    av_opt_set(pSwsContext, "sws_dither", "bayer", NULL);
    //cout << testOption->help << endl;
    //cout << testOption->name << endl;
    // Allocating data for RGB frame
    pScaledFrame->linesize[0] = av_image_get_linesize(pixfmt, scaleX, 0);
    sws_scale(pSwsContext, pFrame->data, pFrame->linesize, 0, pFrame->height, pScaledFrame->data, pScaledFrame->linesize);
    sws_freeContext(pSwsContext);
    return;

}


// Writes a simple image file. Requires a uint8_t array with BGRA pixels. No bounds checking.
// Unless you are working with FFMPEG or similar libraries like OpenCV, your image is likely not padded.
int writePPM(std::string outputFileName, int width, int height, uint8_t *data, bool isPadded) {
    if ( !(outputFileName.substr(outputFileName.length()-4, 4) == ".ppm") ) outputFileName = outputFileName + ".ppm"; // Add .ppm if not already present

    int padCount = (ALIGNMENT-(width%ALIGNMENT))%ALIGNMENT;

    std::fstream dstImage(outputFileName, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!dstImage.is_open()) {
        std::cout << "writePPM: File could not be opened." << std::endl;
        return -1;
    }
    dstImage << "P6 " << width << " " << height << " " << 255 << " ";
    if (isPadded) {
        for (int heightIndex = 0; heightIndex < height; heightIndex++) {
            for (int widthIndex = 0; widthIndex < width*4; widthIndex+=4) {
                int offset = (width+padCount)*heightIndex*4 + widthIndex;
                dstImage << data[offset+2] << data[offset+1] << data[offset];
            }
        }
        dstImage.close();
        return 0;
    } else {
        for (int i = 0; i < width * height * 4; i+=4) {
            dstImage << data[i+2] << data[i+1] << data[i]; // BGRA, alpha ignored

        }
        dstImage.close();
        return 0;
    }
}

int writePal8PPM(std::string outputFileName, int width, int height, uint8_t *data, uint8_t *palette) {
    if ( !(outputFileName.substr(outputFileName.length()-4, 4) == ".ppm") ) outputFileName = outputFileName + ".ppm"; // Add .ppm if not already present

    std::fstream dstImage(outputFileName, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!dstImage.is_open()) {
        std::cout << "writePPM: File could not be opened." << std::endl;
        return -1;
    }
    dstImage << "P6 " << width << " " << height << " " << 255 << " ";

    for (int i = 0; i < width * height; i++) {
            int paletteIndex = data[i];
            //if (i < width) std::cout << paletteIndex << " ";
            uint8_t blue = palette[paletteIndex*4];
            uint8_t green = palette[paletteIndex*4+1];
            uint8_t red = palette[paletteIndex*4+2];
            dstImage << red << green << blue; // BGRA, alpha ignored

        }
    dstImage.close();
    return 0;
}




int VideoDecoder::openInputFile() {
    // Create format context (format is container)
    pFormatContext = avformat_alloc_context();
    if (avformat_open_input(&pFormatContext, inputFileName.c_str(), NULL, NULL) != 0) {
        std::cerr << "avformat_open_input failed!" << std::endl;
        return -1;
    }

    if (avformat_find_stream_info(pFormatContext, NULL) != 0) {
        std::cerr << "avformat_find_stream_info failed!" << std::endl;
        return -1;
    }

    // List info about video
    av_dump_format(pFormatContext, 0, inputFileName.c_str(), 0);

    // Find the video stream and its codec
    // av_find_best_stream(formatContext, Type of stream, Preferred stream index, Number of related stream, Codec associated with stream, flags)
    videoStreamIndex = av_find_best_stream(pFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &pVideoCodec, 0);

    // Open codec context to allow for decoding
    pCodecContext = avcodec_alloc_context3(pVideoCodec);
    if (!pCodecContext) {
        // Out of memory
        avformat_close_input(&pFormatContext);
    }

    result = avcodec_parameters_to_context(pCodecContext, pFormatContext->streams[videoStreamIndex]->codecpar);
    if (result < 0 ) {
        std::cerr << "Failed to set codec parameters!" << std::endl;
        avformat_close_input(&pFormatContext);
        avcodec_free_context(&pCodecContext);
        return -1;
    }

    pCodecContext->thread_count = 2; // This is bound to result in errors at some point. Changing it to >2 immediately causes errors. But it's faster! ~12s to ~8s total runtime
    std::cout << "CODEC THREAD COUNT: " << pCodecContext->thread_count << std::endl;

    // Ready to open stream based on previous parameters
    result = avcodec_open2(pCodecContext, pVideoCodec, NULL);

    if (result < 0) {
        std::cerr << "Cannot open the video codec!" << std::endl;
        avformat_close_input(&pFormatContext);
        avcodec_free_context(&pCodecContext);
        return -1;
    }
    return 0;
}


int VideoDecoder::initializeFilters() {

    av_log_set_level(32);

    char args[512];
    const AVFilter * pBufferSrc  = avfilter_get_by_name("buffer");
    //const AVFilter * pPaletteBufferSrc = avfilter_get_by_name("buffer");
    const AVFilter * pBufferSink = avfilter_get_by_name("buffersink");
    AVFilterInOut * pOutputs = avfilter_inout_alloc();
    AVFilterInOut * pInputs  = avfilter_inout_alloc();
    AVRational timeBase = {av_q2d(pFormatContext->streams[videoStreamIndex]->avg_frame_rate)*1000, 1000};
    //std::cout << "Good: " << pFormatContext->streams[videoStreamIndex]->time_base.num << "/" << pFormatContext->streams[videoStreamIndex]->time_base.den << std::endl; // + std::to_string(pFormatContext->streams[videoStreamIndex]->time_base.num) + "/" + std::to_string(pFormatContext->streams[videoStreamIndex]->time_base.den) +
    //std::cout << "Bad: " << std::to_string((int)(av_q2d(pFormatContext->streams[videoStreamIndex]->avg_frame_rate)*1000)) << std::endl; //  + std::to_string((int)(av_q2d(pFormatContext->streams[videoStreamIndex]->avg_frame_rate)*1000)) +

    pFilterGraph = avfilter_graph_alloc();
    if (!pOutputs || !pInputs || !pFilterGraph) {
        std::cout << "Input, output, or graph failed" << std::endl;
    }

    std::string parseArgs = "buffer=video_size=" + std::to_string(pCodecContext->width) + "x" + std::to_string(pCodecContext->height) + ":pix_fmt=" + std::to_string((int)pCodecContext->pix_fmt) + ":time_base=" + std::to_string(pFormatContext->streams[videoStreamIndex]->time_base.num) + "/" + std::to_string(pFormatContext->streams[videoStreamIndex]->time_base.den) + ":pixel_aspect=1/1 [in_1];"
                        /*"buffer=video_size=16x16:pix_fmt=" + std::to_string((int)AV_PIX_FMT_RGB32) + ":time_base=" + std::to_string((int)(av_q2d(pFormatContext->streams[videoStreamIndex]->avg_frame_rate)*1000)) + ":pixel_aspect=1/1 [in_2];"*/
                        //"[in_1] fps=fps=20 [in_1];"
                        //"[in_1]minterpolate=fps=20:mi_mode=dup[in_1];"
                        "[in_1] scale=" + std::to_string(width) + ":" + std::to_string(height) + " [in_1];"
                        "[in_1] format=28 [in_1];"
                        /*"[in_1] [in_2] paletteuse [result_1];"*/
                        /*"[result_1] buffersink";*/
                        "[in_1] buffersink";
    //std::cout << parseArgs << std::endl;

    //std::cout << timeBase.num << "/" << timeBase.den << std::endl;

    if (avfilter_graph_parse2(pFilterGraph, parseArgs.c_str(), &pInputs, &pOutputs) < 0) {
        std::cout << "avfilter_graph_parse_ptr failed" << std::endl;
    }

    if (avfilter_graph_config(pFilterGraph, NULL) < 0) {
        std::cout << "avfilter_graph_config failed" << std::endl;
    }

    pBufferSrcContext = avfilter_graph_get_filter(pFilterGraph, "Parsed_buffer_0");
    pBufferSrcContext->name = "in";
    pBufferSinkContext = avfilter_graph_get_filter(pFilterGraph, "Parsed_buffersink_3");
    pBufferSinkContext->name = "out";

    //std::cout << avfilter_graph_dump(pFilterGraph, NULL) << std::endl;

    avfilter_inout_free(&pInputs);
    avfilter_inout_free(&pOutputs);
    return 0;
}



uint8_t* VideoDecoder::readFrame() {




    while (av_read_frame(pFormatContext, pAVPacket) <= 0) {
        if (pAVPacket->stream_index != videoStreamIndex) {
            av_packet_unref(pAVPacket);
            continue;
        }

        // Send the data packet to the decoder
        int sendPacketResult = avcodec_send_packet(pCodecContext, pAVPacket);
        if (sendPacketResult == AVERROR(EAGAIN)){
            std::cerr << "Decoder can't take packets right now. Make sure you are draining it." << std::endl;
        }else if (sendPacketResult < AVERROR(ENOMEM)){
            std::cerr << "Failed to send the packet to the decoder!" << std::endl;
        }
        // Receive decoded frame
        result = avcodec_receive_frame(pCodecContext, pFrame);
        if (result == AVERROR(EAGAIN)) {
            av_packet_unref(pAVPacket);
            continue;
        } else if (result == AVERROR_EOF) {
            std::cout << "Finished reading file" << std::endl;
            return nullptr;
        } else if (result < 0) {
            std::cout << "No frame was received from decoder!" << std::endl;
            printf("avcodec_receive_frame error: %s\n", av_err2str(result));
        }
        av_packet_unref(pAVPacket);
        double relTime = framesProcessed++ / inputFrameRate; // Use framesProcessed and inputFrameRate to get an approx timestamp
        if (relTime < (double) framesReturned / outputFrameRate) { // If timestamp of framesProcessed is before the timestamp of framesDisplayed, then DON'T display another frame.
            //std::cout << "Skipping frame: " << framesProcessed << std::endl;
            av_frame_unref(pFrame);
            continue;
        }
        framesReturned++;
        break;
    }

    // Decoded frame is in pFrame. Now, the decoded, likely YUV, frame must be sent to the filtergraph to be scaled and converted to BGRA (RGB basically)
    if (av_buffersrc_add_frame_flags(pBufferSrcContext, pFrame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) std::cout << "Pushing to pBufferSrc failed" << std::endl;
    while (true) {
        int ret = av_buffersink_get_frame(pBufferSinkContext, pRGBFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        if (ret < 0)
            std::cout << "Receive from pBufferSink failed" << std::endl;
    }
    av_frame_unref(pFrame);
    //for (int i = 0; i < frameSizeInBytes/4096; i+=4) std::cout << (int)pRGBFrame->data[0][i+2];
    resultBuffer = pRGBFrame->data[0];
    av_frame_unref(pRGBFrame);
    return resultBuffer;

}


// TODO: Make accurate frame seeking, not just by closest keyframe. Also, use the seek frame function with flags
bool VideoDecoder::seekFrame(int frameNumber) {

    int result = av_seek_frame(pFormatContext, videoStreamIndex, frameNumber, NULL);
    //std::cout << "seekFrame: " << result << std::endl;
    return true;

}

void VideoDecoder::printVideoInfo() {
    av_dump_format(pFormatContext, 0, inputFileName.c_str(), 0);
}

double VideoDecoder::getFrameRate() {
    return inputFrameRate;
}

