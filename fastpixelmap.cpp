#include "fastpixelmap.hpp"
#include <algorithm>

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

int intClamp(int num, int low, int high) {
    if (num < low) return low;
    if (num > high) return high;
    return num;
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
uint8_t* FastPixelMap::convertImage(uint8_t *image) {

    uint8_t* pal8Image = new uint8_t[imageWidth * imageHeight];


    int padCount = (ALIGNMENT-(imageWidth%ALIGNMENT))%ALIGNMENT; // padCount in terms of pixels
    int offset = 0;

    /*
    Dithering: Spreading the error between the source color and chosen palette color to neighboring pixels.
    Using Sierra Lite algorithm. Half of the error is sent to the pixel to the right, and the other half is
    split equally to the pixels down and to the left and directly down.

    Diagram:
         X  1/2
    1/4 1/4
    */
    int diffSum = 0;
    for (int heightIndex = 0; heightIndex < imageHeight; heightIndex++) {

        for (int widthIndex = 0; widthIndex < imageWidth*PIXEL_SIZE_IN_BYTES; widthIndex+=PIXEL_SIZE_IN_BYTES) {

            int rawBlue = image[offset] + colorErrorRow1[widthIndex];
            int rawGreen = image[offset+1] + colorErrorRow1[widthIndex+1];
            int rawRed = image[offset+2] + colorErrorRow1[widthIndex+2];

            int blue = intClamp(rawBlue, 0, 255);
            int green = intClamp(rawGreen, 0, 255);
            int red = intClamp(rawRed, 0, 255);

//            blue = rawBlue;
//            green = rawGreen;
//            red = rawRed;




            /*
            Color Quantization - Fit the source color into the closest possible fit within the given palette.
            Hu, Yu-Chen & Su, B.-H. (2008). Accelerated pixel mapping scheme for colour image quantisation.
            Imaging Science Journal, The. 56. 68-78. 10.1179/174313107X214231.
            */

            int predIndex = indexLUT[intClamp((red + green + blue)/3,0,255)]; // Find the predicted index for the closest palette color using mean

            int sedMin = sed(blue, green, red, palette + predIndex*PIXEL_SIZE_IN_BYTES);
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
                    } else if ( (3 * sedMin) < ssd(blue, green, red, palette+downIndex*4) )  {
                        down = false;
                    } else if ( (4 * sedMin) < paletteDistanceLUT[indexMin*paletteSize + downIndex] ) {
                        // This color is rejected using the triangular inequality rule

                    } else {
//                        int testSed = sed(blue, green, red, palette+downIndex*4);
//                        if (testSed < sedMin) {
//                            sedMin = testSed;
//                            indexMin = downIndex;
//                        }
                        // Partial distance search technique
                        // Only testing after adding the blue and green channels, as there was not significant speed-up when checking for each channel.
                        int testSed = (blue - palette[downIndex*4]) * (blue - palette[downIndex*4]);
                        if (testSed < sedMin) {
                            testSed += (green - palette[downIndex*4+1]) * (green - palette[downIndex*4+1]);
                            if (testSed < sedMin) {
                                testSed += (red - palette[downIndex*4+2]) * (red - palette[downIndex*4+2]);
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
                    } else if ( (3 * sedMin) < ssd(blue, green, red, palette+upIndex*4) ) {
                        up = false;
                    } else  if ( (4 * sedMin) < paletteDistanceLUT[indexMin*paletteSize + upIndex] ) {

                        // This color is rejected using the triangular inequality rule

                    } else {
//                        int testSed = sed(blue, green, red, palette+upIndex*4);
//                        if (testSed < sedMin) {
//                            sedMin = testSed;
//                            indexMin = upIndex;
//                        }
                        int testSed = (blue - palette[upIndex*4]) * (blue - palette[upIndex*4]);
                        if (testSed < sedMin) {
                            testSed += (green - palette[upIndex*4+1]) * (green - palette[upIndex*4+1]);
                            if (testSed < sedMin) {
                                testSed += (red - palette[upIndex*4+2]) * (red - palette[upIndex*4+2]);
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
            offset+=PIXEL_SIZE_IN_BYTES;

            int diff = predIndex - indexMin;
            diffSum += diff;
            //if (abs(diff) < 10 || true) cout << "x: " << widthIndex << ", y: " << heightIndex <<  ", redErr: " << colorErrorRow1[widthIndex+2] << ", greenErr: " << colorErrorRow1[widthIndex+1] << ", blueErr: " << colorErrorRow1[widthIndex] << endl;
            //else cout << "x: " << widthIndex << ", y: " << heightIndex << "pred: " << predIndex << ", min: " << indexMin << ", diff: " << diff << " XXXXXX" << endl;

            calculateError(blue, green, red, widthIndex, indexMin);
            //calculateError(rawBlue, rawGreen, rawRed, widthIndex, indexMin);

        } // End pixel

        if (isPadded) {
            offset += padCount*PIXEL_SIZE_IN_BYTES;
        }

        swapArrays();
    } // End row

    //cout << "diffSum: " << diffSum << endl;

    fill(colorErrorRow1,colorErrorRow1+4*imageWidth+4, 0);
    fill(colorErrorRow2,colorErrorRow2+4*imageWidth+4, 0);

    return pal8Image;
}

void FastPixelMap::calculateError(int blue, int green, int red, int widthIndex, int indexMin) {
    // Calculate and add error to neighboring pixels.
    int blueError = (blue - palette[indexMin*4]);
    int greenError = (green - palette[indexMin*4+1]);
    int redError = (red - palette[indexMin*4+2]);

    // Half errors
    blueError >>= 1;
    greenError >>= 1;
    redError >>= 1;

    // Add half error to right pixel
    colorErrorRow1[widthIndex+4] += blueError;
    colorErrorRow1[widthIndex+5] += greenError;
    colorErrorRow1[widthIndex+6] += redError;
    // Half errors again
    blueError >>= 1;
    greenError >>= 1;
    redError >>= 1;
    // Add quarter error to bottom-left pixel
    if (widthIndex != 0) {
        colorErrorRow2[widthIndex-4] += blueError;
        colorErrorRow2[widthIndex-3] += greenError;
        colorErrorRow2[widthIndex-2] += redError;
    }
    // Add quarter error to bottom pixel
    colorErrorRow2[widthIndex] += blueError;
    colorErrorRow2[widthIndex+1] += greenError;
    colorErrorRow2[widthIndex+2] += redError;
}

void FastPixelMap::swapArrays() {
    fill(colorErrorRow1,colorErrorRow1+4*imageWidth+4, 0);
        //memset(colorErrorRow1,0,16*imageWidth+16);
    int* tempRow1 = colorErrorRow1;
    colorErrorRow1 = colorErrorRow2;
    colorErrorRow2 = tempRow1;
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
    return ((colorA[0] - colorB[0]) * (colorA[0] - colorB[0]) + (colorA[1] - colorB[1]) * (colorA[1] - colorB[1]) + (colorA[2] - colorB[2]) * (colorA[2] - colorB[2]));
}

int FastPixelMap::sed(int blue, int green, int red, uint8_t *colorB) {
    return ((blue - colorB[0]) * (blue - colorB[0]) + (green - colorB[1]) * (green - colorB[1]) + (red - colorB[2]) * (red - colorB[2]));
}

int FastPixelMap::ssd(uint8_t *colorA, uint8_t *colorB) {
    int result = (colorA[0] + colorA[1] + colorA[2] - colorB[0] - colorB[1] - colorB[2]);
    return result * result;
}

int FastPixelMap::ssd(int blue, int green, int red, uint8_t *colorB) {
    int result = (blue + green + red - colorB[0] - colorB[1] - colorB[2]);
    return result * result;
}

int FastPixelMap::meanValue(uint8_t *color) {
    return (color[0] + color[1] + color[2]) / 3;
}




