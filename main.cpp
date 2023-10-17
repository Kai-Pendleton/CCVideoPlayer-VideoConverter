#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <queue>
#include <functional>
#include <atomic>
#include "decodevideo.hpp"
#include "fastpixelmap.hpp"

/*
*   Workshop 3
*   by Kai Pendleton
*
*   Problem: Mapping pixels in a source image to the closest color available within a predefined palette
*   Goal: An algorithm that can quickly convert RGB images to pal8
*
*/

using namespace std;

char colorCodes[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

struct Color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

struct GamePixel {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t backgroundIndex;
    uint8_t foregroundIndex;
    friend std::ostream & operator << (std::ostream &out, const GamePixel &p);
};

struct Pixel {
    int x;
    int y;
    int backgroundIndex;
    int textIndex;
};

struct ConvertJob {
    int frameNumber;
    uint8_t* frame;
};

struct WriteJob {
    int frameNumber;
    uint8_t* frame;
};

bool operator> (const WriteJob &lhs, const WriteJob &rhs) {
    return lhs.frameNumber > rhs.frameNumber;
}

ostream & operator << (ostream &out, const GamePixel &p) {
    out << "Red: " << (int)p.red << ", Green: " << (int)p.green << ", Blue: " << (int)p.blue << ", Back: " << (int)p.backgroundIndex << ", Fore: " << (int)p.foregroundIndex << ", Mean:" << (((int)p.red+p.green+p.blue)/3) << endl;
}

bool pixelCmp(const GamePixel &a, const GamePixel &b) {
    int meanA = ((int)a.red+a.green+a.blue)/3;
    int meanB = ((int)b.red+b.green+b.blue)/3;
    return (meanA < meanB) ? true : false;
}


// Color palette by John A. Watlington at alumni.media.mit.edu/~wad/color/palette.html
Color colorValues[16] = {
                                //Black
                                {0, 0, 0},
                                //Dark Gray
                                {87, 87, 87},
                                //Red
                                {173, 35, 35},
                                //Blue
                                {42, 75, 215},
                                //Green
                                {29, 105, 20},
                                //Brown
                                {129, 74, 25},
                                //Purple
                                {129, 38, 192},
                                //Light Gray
                                {160, 160, 160},
                                //Light Green
                                {129, 197, 122},
                                //Light Blue
                                {157, 175, 255},
                                //Cyan
                                {41, 208, 208},
                                //Orange
                                {255, 146, 51},
                                //Yellow
                                {255, 238, 51},
                                //Tan
                                {233, 222, 187},
                                //Pink
                                {255, 205, 243},
                                //White
                                {255, 255, 255}
};

BGRAPixel expandedPalette[256];
GamePixel gamePalette[256];

void initializeExpandedColors() {

    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            uint8_t red, green, blue;
            red = (int)(0.4 * colorValues[i].red + 0.6 * colorValues[j].red);
            green = (int)(0.4 * colorValues[i].green + 0.6 * colorValues[j].green);
            blue = (int)(0.4 * colorValues[i].blue + 0.6 * colorValues[j].blue);
            expandedPalette[16 * i + j] = {blue, green, red};
            gamePalette[16 * i + j] = {red, green, blue, (uint8_t) j, (uint8_t) i};
        }
    }
    return;
}

queue<ConvertJob> convertJobQueue;
mutex convertJobMutex;
priority_queue<WriteJob, vector<WriteJob>, greater<WriteJob> > writeJobQueue; // Min Priority queue.
mutex writeJobMutex;
int finalFrameNumber = -1;
atomic<bool> isFinished;

void runDecoderThread(int width, int height, int skipFrame, VideoDecoder & decoder) {


    decoder.seekFrame(0);
    uint8_t* image = nullptr; // Don't need to allocate, decoder has an internal buffer that is used.
    int frameSize = decoder.frameSizeInBytes;
    int frameNumber = 1;
    for (int i = 0; true; i++) {

        // Retrieve decoded BGRA image
        image = decoder.readFrame(); // decoder only returns nullptr when at EOF
        if (image == nullptr) {
                finalFrameNumber = frameNumber;
                return; //EOF
        }

        if ( i%skipFrame == 0 ) {

            // Add frame to queue as convertJob. Have to allocate new memory for frame, don't have to touch uint8_t* image.
            uint8_t *queueFrame = new uint8_t[frameSize];
            copy(image, image+frameSize-1, queueFrame);
            convertJobMutex.lock();
            convertJobQueue.push( {frameNumber, queueFrame} );
            convertJobMutex.unlock();

            frameNumber++;
        }

    }
}

void runConverterThread(int width, int height) {


    FastPixelMap pixelMapper((uint8_t*)expandedPalette, 256, width, height, true);

    // Grab frame from convertJobQueue, convert it, and DEALLOCATE ORIGINAL FRAME
    // Then add converted frame to writeJobQueue along with frameNumber
    for (int i = 0; true; i++) {

        if (isFinished == true) return; // Decoder has returned and writer has finished writing. Thus, converter must be done and busy waiting. return.

        ConvertJob job;
        convertJobMutex.lock();
        if (convertJobQueue.size() > 0) {
            job = convertJobQueue.front();
            convertJobQueue.pop();
        } else {
            convertJobMutex.unlock();
            continue;
        }
        convertJobMutex.unlock();


        uint8_t* pal8Image = pixelMapper.convertImage(job.frame); // pixelMapper allocates memory for us.

        if (job.frameNumber == 500) writePal8PPM("paletteTest.ppm", width, height, pal8Image, (uint8_t*) expandedPalette);
        if (job.frameNumber == 500) writePPM("test.ppm", width, height, job.frame, true);

        writeJobMutex.lock();
        writeJobQueue.push( {job.frameNumber, pal8Image} );
        cout << "Pushed to writeJobQueue, new size of " << writeJobQueue.size() << endl;
        writeJobMutex.unlock();

        delete [] job.frame;
    }

}

void writeGameImage(int width, int height, int frameRate, uint8_t * data, uint8_t * oldFrame, fstream &dstVideo) {

    int pixelCount = width * height;

    // Output every pixel if oldFrame does not exist. First frame of video.
    if (oldFrame == nullptr) {
        dstVideo.write( (char *) &pixelCount, 4);
        for (int i = 0; i < pixelCount; i++) {
            //cout << "x: " << i % width + 1 << ", y: " << i / width + 1 << ", back: " << (int)gamePalette[data[i]].backgroundIndex << ", fore: " << (int)gamePalette[data[i]].foregroundIndex << endl;
            uint16_t x = i % width + 1;
            uint16_t y = i / width + 1;
            dstVideo.write( (char *) &x, 2);
            dstVideo.write( (char *) &y, 2);
            dstVideo << colorCodes[gamePalette[data[i]].backgroundIndex] << colorCodes[gamePalette[data[i]].foregroundIndex];
            //dstVideo << (uint8_t) i % width + 1 << (uint8_t) i / width + 1 << colorCodes[gamePalette[data[i]].backgroundIndex] << colorCodes[gamePalette[data[i]].foregroundIndex];
        }
        return;
    }

    vector<Pixel> convertedFrame;
    for(int i = 0; i < width * height; i++) {
        uint8_t paletteIndex = data[i];
        if (oldFrame[i] == paletteIndex && i != 0) {
            continue;
        }
        GamePixel color = gamePalette[paletteIndex];
        Pixel pixel = { i % width + 1, i / width + 1, color.backgroundIndex, color.foregroundIndex };
        convertedFrame.push_back(pixel);
    }
    int frameSize = convertedFrame.size();
    dstVideo.write( (char *) &frameSize, 4);
    for (int i = 0; i < convertedFrame.size(); i++) {
        uint16_t xCoord = convertedFrame[i].x;
        uint16_t yCoord = convertedFrame[i].y;
        dstVideo.write( (char *) &xCoord, 2);
        dstVideo.write( (char *) &yCoord, 2);

        dstVideo << colorCodes[convertedFrame[i].backgroundIndex] << colorCodes[convertedFrame[i].textIndex];

    }
    return;
}

int main(int argc, char *argv[])
{
    int width = 164;
    int height = 81;
    char * srcFileName;
    if (argc < 2) {
        cout << "Please provide a movie file." << endl;
        return -1;
    } else if (argc == 2) {
        srcFileName = argv[1];
        cout << "Using provided movie file and default resolution of 164x81." << endl;
    } else if (argc == 4) {
        srcFileName = argv[1];
        width = stoi(argv[2]);
        height = stoi(argv[3]);
        cout << "Using provided movie file and resolution." << endl;
    } else {
        cerr << "Too many arguments. Exiting." << endl;
        return -1;
    }

    BGRAPixel palette[16];
    for (int i = 0; i < 16; i++) {
        palette[i].blue = colorValues[i].blue;
        palette[i].green = colorValues[i].green;
        palette[i].red = colorValues[i].red;
    }
    initializeExpandedColors();
    sort(gamePalette, gamePalette+255, pixelCmp);
    sort(expandedPalette, expandedPalette+255, BGRAcmp);
    //writePPM("palette", 4, 4, (uint8_t*) palette, false);
    //writePPM("expandedPalette", 16, 16, (uint8_t*) expandedPalette, false);


    string dstFileName = "outputVideo.ppm";
    fstream dstVideo(dstFileName, ios::out | ios::in | ios::trunc | ios::binary);
    if (!dstVideo.is_open()) {
        cout << "outputVideo.ppm: File could not be opened." << endl;
        return -1;
    }

    VideoDecoder decoder(width, height, srcFileName);
    //decoder.printVideoInfo();
    int frameRate = decoder.getFrameRate();
    cerr << "Frame rate: " << frameRate << endl;
    int skipFrame = 1;
    for (;skipFrame <= 12; skipFrame++) {
        if (frameRate / skipFrame <= 12 && frameRate % skipFrame == 0) break; // We want the end frame rate to be below 12, but we dont want uneven frame skipping. This breaks if frame rates with no factor below below 13 is given.
    }
    cerr << "skipFrame: " << skipFrame << endl;
    dstVideo << (uint8_t) (width>>8) << (uint8_t) (width&0x00ff) << (uint8_t) (height>>8) << (uint8_t) (height&0x00ff) << (uint8_t) (frameRate / skipFrame);



    // Create decoder thread, convert threads, go to write code
    vector<thread> threads;
    int hardwareThreads = thread::hardware_concurrency();
    int converterThreadCount;
    if (hardwareThreads < 3) converterThreadCount = 1;
    else {
        converterThreadCount = hardwareThreads - 2;
    }
    if (converterThreadCount > 6) converterThreadCount = 6;
    // In short, minimum of 1 converter, max of 6, limit number of total threads to number of hardware threads

    threads.emplace_back(runDecoderThread, width, height, skipFrame, ref(decoder));
    for (int i = 0; i < converterThreadCount; i++) {
        threads.emplace_back(runConverterThread, width, height);
    }


    uint8_t *oldPal8Image = nullptr;
    uint8_t * pal8Image = nullptr;
    int framesWritten = 0;
    while (true) {

        WriteJob job;


        writeJobMutex.lock();
        if (writeJobQueue.size() > 0) { // If job is available
            job = writeJobQueue.top(); // Access job
            if (job.frameNumber == framesWritten+1) { // If the job is for the next frame
                writeJobQueue.pop(); // Take the job
            } else {
                writeJobMutex.unlock(); // Else, give up the job
                continue; // Try again
            }
        } else {
            cout << "Waiting on convert: " << convertJobQueue.size() << endl;
            writeJobMutex.unlock();
            continue;
        }

        writeJobMutex.unlock();

        pal8Image = job.frame;


        writeGameImage(width, height, frameRate, pal8Image, oldPal8Image, dstVideo);
        framesWritten++;

        if (oldPal8Image != nullptr) delete[] oldPal8Image;

        oldPal8Image = pal8Image;
        pal8Image = nullptr;

        if (framesWritten == finalFrameNumber-1) {
            isFinished = true;
            cout << "Joining..." << endl;
            break;
        }
    }

    dstVideo.close();

    for (auto& thread : threads) {
        thread.join();
    }

    cout << "Frames written: " <<  framesWritten << endl;

    return 0;
}

/*
    Results:


    500 frames of 320x240 mp4 video

    Algorithm                                                      Time(s)         Time per Frame(s)
    Full-Search (naive)                                             68.744          .137
    MPS + No PDS                                                    5.980           .01196
    MPS + Partial PDS (Green+Blue, check, then red)                 5.996           .01199
    MPS + Full PDS                                                  5.926           .01185
    MPS + Full PDS + TIE                                            5.927           .01185
    No Pixel Mapping (Just decoding/scaling video with FFMPEG)      .643            .00129
    Performance Gain compared to Full-Search (Decoding/scaling time removed):       12.888x



    500 frames of 320x240 mkv video (Never Gonna Give You Up by Rick Astley)

    Algorithm                                                      Time(s)         Time per Frame(s)
    Full-Search (naive)                                            73.322          .1466
    MPS + Full PDS + TIE                                           11.089          .0222
    No Pixel Mapping (Just decoding/scaling video with FFMPEG)     3.200           .0064
    Performance Gain compared to Full Search (Decoding/scaling time removed):      8.889x


    Authors' reported speed-up: ~20x depending on image

    Possible Reasons for difference in speed: My best guess is that the additional operations used
    for calculating the index of the palette colors and image pixels significantly increases the overhead of my
    algorithm. It may be possible to rewrite the algorithm to continually track the offset of the image pointer
    and palette color without additional operations. This could potentially save multiple operations per pixel
    and per color checked.
    Another possible cause of the slowdown is the overhead of the class structure that I used. The authors'
    used C for their implementation, so they presumably had very little overhead.


    Sources:

    - Main source:
    Hu, Yu-Chen & Su, B.-H. (2008). Accelerated pixel mapping scheme for colour image quantisation.
    Imaging Science Journal, The. 56. 68-78. 10.1179/174313107X214231.

    - Original Paper describing the MPS algorithm
    Ra, & Kim, J.-K. (1993). A fast mean-distance-ordered partial codebook search algorithm for image vector quantization.
    IEEE Transactions on Circuits and Systems. 2, Analog and Digital Signal Processing, 40(9), 576–579.
    https://doi.org/10.1109/82.257335


    - Related Paper describing the trade-offs of the partial distance search technique
    L. Fissore, P. Laface, P. Massafra and F. Ravera, "Analysis and improvement of the partial distance search algorithm,"
    1993 IEEE International Conference on Acoustics, Speech, and Signal Processing, Minneapolis, MN, USA, 1993, pp. 315-318
    vol.2, doi: 10.1109/ICASSP.1993.319300.

*/




