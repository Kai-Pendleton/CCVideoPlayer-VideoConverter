#ifndef FASTPIXELMAP_HPP_INCLUDED
#define FASTPIXELMAP_HPP_INCLUDED
#include <iostream>
#include <algorithm>

#ifndef ALIGNMENT
#define ALIGNMENT 64
#endif

struct BGRAPixel {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t alpha;
    friend std::ostream & operator << (std::ostream &out, const BGRAPixel &p);

};

bool BGRAcmp(const BGRAPixel &a, const BGRAPixel &b);
int displayPalette(uint8_t *palette, int paletteSize);



// Converts BGRA image into pal8 using accelerated pixel mapping algorithm by Yu-Chen Hu and B.-H Su
// Stores pal8 image in "image"
// Sorts palette by ascending mean value
// paletteSize is number of colors in palette, not number of bytes associated with *palette
class FastPixelMap {

public:
    FastPixelMap(uint8_t *palette, int paletteSize, int imageWidth, int imageHeight, bool isPadded) {
        this->palette = palette;
        this->paletteSize = paletteSize;
        // (1) Sort palette by mean value
        meanPaletteLUT = new uint8_t[paletteSize];
        if (!initializeMeanPaletteLUT()) std::cerr << "Failed to initialize Mean Palette LUT" << std::endl;
        if (!initializeIndexLUT()) std::cerr << "Failed to initialize Index LUT or your palette does not have white as a color!" << std::endl;
        paletteDistanceLUT = new int[paletteSize*paletteSize];
        if (!initializePaletteDistanceLUT()) std::cerr << "Failed to initialize Palette Distance LUT!" << std::endl;

        this->imageWidth = imageWidth;
        this->imageHeight = imageHeight;
        this->isPadded = isPadded;

        colorErrorRow1 = new int[4*imageWidth+4](); // Allocate extra pixel to avoid needing to check for overflow
        colorErrorRow2 = new int[4*imageWidth+4](); // Allocate extra pixel to avoid needing to check for overflow
    }
    uint8_t* convertImage(uint8_t *image);
    uint8_t* fullSearchConvertImage(uint8_t *image, int imageWidth, int imageHeight, bool isPadded);

    ~FastPixelMap() {

        delete[] meanPaletteLUT;
        delete[] paletteDistanceLUT;
        delete[] colorErrorRow1;
        delete[] colorErrorRow2;

    }

private:
    uint8_t *palette;
    int paletteSize;

    int imageWidth;
    int imageHeight;
    bool isPadded;

    void calculateError(int blue, int green, int red, int widthIndex, int indexMin);
    int * colorErrorRow1;
    int * colorErrorRow2;
    void swapArrays();

    uint8_t *meanPaletteLUT;
    bool initializeMeanPaletteLUT();

    uint8_t indexLUT[256];
    bool initializeIndexLUT();

    int *paletteDistanceLUT;
    bool initializePaletteDistanceLUT();

    int sed(uint8_t *colorA, uint8_t *colorB);
    int sed(int blue, int green, int red, uint8_t *colorB);
    int ssd(uint8_t *colorA, uint8_t *colorB);
    int ssd(int blue, int green, int red, uint8_t *colorB);
    int meanValue(uint8_t *color);

};

#endif // FASTPIXELMAP_HPP_INCLUDED
