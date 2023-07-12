#ifndef FASTPIXELMAP_HPP_INCLUDED
#define FASTPIXELMAP_HPP_INCLUDED
#include <iostream>
#include <algorithm>

#ifndef ALIGNMENT
#define ALIGNMENT 16
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
    FastPixelMap(uint8_t *palette, int paletteSize) {
        this->palette = palette;
        this->paletteSize = paletteSize;
        // (1) Sort palette by mean value
        std::sort((BGRAPixel*) palette, (BGRAPixel*) palette+paletteSize-1, BGRAcmp);
        meanPaletteLUT = new uint8_t[paletteSize];
        if (!initializeMeanPaletteLUT()) std::cerr << "Failed to initialize Mean Palette LUT" << std::endl;
        if (!initializeIndexLUT()) std::cerr << "Failed to initialize Index LUT or your palette does not have white as a color!" << std::endl;
        for (int i = 0; i < 768; i++) { // initialize squaresLUT
            squaresLUT[i] = i * i;
        }
        paletteDistanceLUT = new int[paletteSize*paletteSize];
        if (!initializePaletteDistanceLUT()) std::cerr << "Failed to initialize Palette Distance LUT!" << std::endl;
    }
    uint8_t* convertImage(uint8_t *image, int imageWidth, int imageHeight, bool isPadded);
    uint8_t* fullSearchConvertImage(uint8_t *image, int imageWidth, int imageHeight, bool isPadded);

    ~FastPixelMap() {

        delete[] meanPaletteLUT;
        delete[] paletteDistanceLUT;

    }

private:
    uint8_t *palette;
    int paletteSize;

    uint8_t *meanPaletteLUT;
    bool initializeMeanPaletteLUT();

    uint8_t indexLUT[256];
    bool initializeIndexLUT();

    int squaresLUT[768];

    int *paletteDistanceLUT;
    bool initializePaletteDistanceLUT();

    int sed(uint8_t *colorA, uint8_t *colorB);
    int ssd(uint8_t *colorA, uint8_t *colorB);
    int meanValue(uint8_t *color);

};

#endif // FASTPIXELMAP_HPP_INCLUDED
