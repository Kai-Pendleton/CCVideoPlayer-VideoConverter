#include "fastpixelmap.hpp"

using namespace std;

int PIXEL_SIZE_IN_BYTES = 4;

bool BGRAcmp(const BGRAPixel &a, const BGRAPixel &b) {
    int meanA = ((int)a.red+a.green+a.blue)/3;
    int meanB = ((int)b.red+b.green+b.blue)/3;
    return (meanA < meanB) ? true : false;
}

ostream & operator << (ostream &out, const BGRAPixel &p) {
    out << "Red: " << (int)p.red << ", Green: " << (int)p.green << ", Blue: " << (int)p.blue << ", Mean:" << (((int)p.red+p.green+p.blue)/3) << endl;
}

int displayPalette(uint8_t *palette, int paletteSize) {
    cout << "Size: " << paletteSize << endl;
    for (int i = 0; i < paletteSize*4; i+=4) {
        cout << "Red: " << (int)palette[i+2] << ", Green: " << (int)palette[i+1] << ", Blue: " << (int)palette[i] << ", Mean: " << ((int)palette[i] + palette[i+1] + palette[i+2])/3 << endl;
    }
    return 0;
}


uint8_t* FastPixelMap::fullSearchConvertImage(uint8_t *image, int imageWidth, int imageHeight, bool isPadded) {

    uint8_t* pal8Image = new uint8_t[imageWidth * imageHeight];


    int padCount = (ALIGNMENT-(imageWidth%ALIGNMENT))%ALIGNMENT; // padCount in terms of pixels
    for (int heightIndex = 0; heightIndex < imageHeight; heightIndex++) {
        for (int widthIndex = 0; widthIndex < imageWidth*PIXEL_SIZE_IN_BYTES; widthIndex+=PIXEL_SIZE_IN_BYTES) {

            int offset;
            if (isPadded) {
                offset = (imageWidth+padCount)*heightIndex*PIXEL_SIZE_IN_BYTES + widthIndex;
            } else {
                offset = imageWidth*heightIndex*PIXEL_SIZE_IN_BYTES + widthIndex;
            }

            int sedMin = 10000000; // Impossible to reach for 8-bit color channels
            int indexMin = -1;

            for (int k = 0; k < paletteSize; k++) {
                int testSed = sed(image+offset, palette+k*PIXEL_SIZE_IN_BYTES);
                if (testSed < sedMin) {
                    sedMin = testSed;
                    indexMin = k;
                }
            }
            pal8Image[heightIndex*imageWidth+widthIndex/PIXEL_SIZE_IN_BYTES] = indexMin;
        }
    }
    //displayPalette(palette, paletteSize);


    return pal8Image;
}

// imageWidth is number of pixels per row. FFMPEG pads rows with excess space in order to make sure
// the linesize is divisible by 32.
uint8_t* FastPixelMap::convertImage(uint8_t *image, int imageWidth, int imageHeight, bool isPadded) {

    uint8_t* pal8Image = new uint8_t[imageWidth * imageHeight];


    int padCount = (ALIGNMENT-(imageWidth%ALIGNMENT))%ALIGNMENT; // padCount in terms of pixels
    for (int heightIndex = 0; heightIndex < imageHeight; heightIndex++) {
        for (int widthIndex = 0; widthIndex < imageWidth*PIXEL_SIZE_IN_BYTES; widthIndex+=PIXEL_SIZE_IN_BYTES) {

            int offset;
            if (isPadded) {
                offset = (imageWidth+padCount)*heightIndex*PIXEL_SIZE_IN_BYTES + widthIndex;
            } else {
                offset = imageWidth*heightIndex*PIXEL_SIZE_IN_BYTES + widthIndex;
            }

            // Find the predicted index for the closest palette color using mean
            int predIndex = indexLUT[meanValue(image + offset)];

            int sedMin = sed(image + offset, palette + predIndex*PIXEL_SIZE_IN_BYTES);
            int indexMin = predIndex;

            int downIndex = indexMin;
            int upIndex = indexMin;


            bool down = (indexMin >= paletteSize-1) ? false : true;
            bool up = (indexMin <= 0) ? false : true;
            while (up || down) {

                if (down) { // check below predicted index (below = further in array)
                        downIndex++;
                    if ( downIndex >= paletteSize ) {
                        down = false;
                    } else if ( (3 * sedMin) < ssd(image + offset, palette+downIndex*4) )  {
                        down = false;
                    } else if ( (4 * sedMin) < paletteDistanceLUT[indexMin*paletteSize + downIndex] ) {
                        // This color is rejected using the triangular inequality rule

                    } else {
//                        int testSed = sed(image + offset, palette+downIndex*4);
//                        if (testSed < sedMin) {
//                            sedMin = testSed;
//                            indexMin = downIndex;
//                        }
                        // Partial distance search technique
                        // Only testing after adding the blue and green channels, as there was not significant speed-up when checking for each channel.
                        int testSed = squaresLUT[abs(image[offset] - palette[downIndex*4])];
                        if (testSed < sedMin) {
                            testSed += squaresLUT[abs(image[offset+1] - palette[downIndex*4+1])];
                            if (testSed < sedMin) {
                                testSed += squaresLUT[abs(image[offset+2] - palette[downIndex*4+2])];
                                if (testSed < sedMin) {
                                    sedMin = testSed;
                                    indexMin = downIndex;
                                }
                            }
                        }
                    }
                }

                if (up) { // check above predicted index (above = before in array)
                    upIndex--;
                    if ( upIndex < 0 ) {
                        up = false;
                    } else if ( (3 * sedMin) < ssd(image + offset, palette+upIndex*4) ) {
                        up = false;
                    } else  if ( (4 * sedMin) < paletteDistanceLUT[indexMin*paletteSize + upIndex] ) {

                        // This color is rejected using the triangular inequality rule

                    } else {
//                        int testSed = sed(image + offset, palette+upIndex*4);
//                        if (testSed < sedMin) {
//                            sedMin = testSed;
//                            indexMin = upIndex;
//                        }
                        int testSed = squaresLUT[abs(image[offset] - palette[upIndex*4])];
                        if (testSed < sedMin) {
                            testSed +=squaresLUT[abs(image[offset+1] - palette[upIndex*4+1])];
                            if (testSed < sedMin) {
                                testSed += squaresLUT[abs(image[offset+2] - palette[upIndex*4+2])];
                                if (testSed < sedMin) {
                                    sedMin = testSed;
                                    indexMin = upIndex;
                                }
                            }
                        }
                    }

                } // End up/down if-blocks
            } // End while (up or down) - Done checking every eligible color
            pal8Image[heightIndex*imageWidth+widthIndex/PIXEL_SIZE_IN_BYTES] = indexMin;
        }
    }
    //displayPalette(palette, paletteSize);

    return pal8Image;
}

bool FastPixelMap::initializeMeanPaletteLUT() {

    for (int i = 0; i < paletteSize*PIXEL_SIZE_IN_BYTES; i+=PIXEL_SIZE_IN_BYTES) {
        meanPaletteLUT[i/PIXEL_SIZE_IN_BYTES] = ((int)palette[i] + palette[i+1] + palette[i+2]) / 3;
    }
    return true;
}

bool FastPixelMap::initializeIndexLUT() {

    int zeroCheck = ((int)meanPaletteLUT[0] + meanPaletteLUT[1]) / 2;
    int kCheck = ((int)meanPaletteLUT[paletteSize-2] + meanPaletteLUT[paletteSize-1]) / 2;
    for(int i = 0; i < 256; i++) {

        if ( i < zeroCheck ) {
            indexLUT[i] = 0;
        } else for (int j = 1; j < paletteSize-1; j++) {
            if ( i >= ((int)meanPaletteLUT[j-1] + meanPaletteLUT[j])/2 && i < ((int)meanPaletteLUT[j] + meanPaletteLUT[j+1])/2 ) {
                indexLUT[i] = j;
            }
        }

        if ( i >= kCheck ) {
            for (int k = i; k < 256; k++) {
                indexLUT[k] = paletteSize-1;
            }
            return true;
        }
    }

//    for (int i = 0; i < 256; i++) {
//        cout << "indexLUT[" << i << "]: " << (int)indexLUT[i] << endl;
//    }
    return false;
}

bool FastPixelMap::initializePaletteDistanceLUT() {
    for (int i = 0; i < paletteSize; i++) {
        for (int j = 0; j < paletteSize; j++) {
            paletteDistanceLUT[paletteSize*i+j] = sed(palette+i*4,palette+j*4);
        }
    }
    return true;
}

int FastPixelMap::sed(uint8_t *colorA, uint8_t *colorB) {
    return (squaresLUT[abs(colorA[0] - colorB[0])] + squaresLUT[abs(colorA[1] - colorB[1])] + squaresLUT[abs(colorA[2] - colorB[2])]);
}

int FastPixelMap::ssd(uint8_t *colorA, uint8_t *colorB) {
    return squaresLUT[abs(colorA[0] + colorA[1] + colorA[2] - colorB[0] - colorB[1] - colorB[2])];
}

int FastPixelMap::meanValue(uint8_t *color) {
    return (color[0] + color[1] + color[2]) / 3;
}




