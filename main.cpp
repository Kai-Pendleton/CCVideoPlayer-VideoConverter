#include <iostream>
#include <vector>
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
            gamePalette[16 * i + j] = {red, green, blue, j, i};
        }
    }
    return;
}


void writeGameImage(int width, int height, int frameRate, uint8_t * data, uint8_t * oldFrame, fstream &dstVideo) {

    uint16_t pixelCount = width * height;

    // Output every pixel if oldFrame does not exist. First frame of video.
    if (oldFrame == nullptr) {
        dstVideo.write( (char *) &pixelCount, 2);
        for (int i = 0; i < pixelCount; i++) {
            //cout << "x: " << i % width + 1 << ", y: " << i / width + 1 << ", back: " << (int)gamePalette[data[i]].backgroundIndex << ", fore: " << (int)gamePalette[data[i]].foregroundIndex << endl;
            uint8_t x = i % width + 1;
            uint8_t y = i / width + 1;
            dstVideo.write( (char *) &x, 1);
            dstVideo.write( (char *) &y, 1);
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
    uint16_t frameSize = (uint16_t) convertedFrame.size();
    dstVideo.write( (char *) &frameSize, 2);
    for (int i = 0; i < convertedFrame.size(); i++) {
        dstVideo.write( (char *) &convertedFrame[i].x, 1);
        dstVideo.write( (char *) &convertedFrame[i].y, 1);

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
    dstVideo << (uint8_t) width << (uint8_t) height << (uint8_t) (frameRate / skipFrame);

    decoder.seekFrame(0);
    uint8_t* image = nullptr;
    uint8_t *pal8Image = nullptr;
    uint8_t *oldPal8Image = nullptr;
    FastPixelMap pixelMapper((uint8_t*)expandedPalette, 256, width, height, true);
    for (int i = 0; i < 500000; i++) {

        // Retrieve decoded BGRA image
        image = decoder.readFrame();
        if (image == nullptr) break; //EOF

        if ( i%skipFrame == 0 ) {
//        if (true) {
            //cout << i << endl;
            //int snapshotFrame = 500;
            //if (i == snapshotFrame) writePPM("test.ppm", width, height, image, true);

            // Convert image to pal8
            pal8Image = pixelMapper.convertImage(image);

            writeGameImage(width, height, frameRate, pal8Image, oldPal8Image, dstVideo);

            if (!(i == 0)) delete[] oldPal8Image;
            oldPal8Image = pal8Image;

            //if (i == snapshotFrame) writePal8PPM("paletteTest.ppm", width, height, pal8Image, (uint8_t*) expandedPalette);

        }

    }
    delete[] pal8Image;
    dstVideo.close();

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




