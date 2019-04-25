#include "myLib.h"

// The start of the video memory
unsigned short *videoBuffer = (unsigned short *)0x6000000;

// The start of DMA registers
DMA *dma = (DMA *)0x40000B0;

// Set a pixel on the screen in Mode 3
void setPixel3(int row, int col, unsigned short color) {
	videoBuffer[OFFSET(row, col, SCREENWIDTH)] = color;
}

// Set a pixel on the screen in Mode 4
void setPixel4(int row, int col, unsigned char colorIndex) {

    unsigned short pixelData = videoBuffer[OFFSET(row, col, SCREENWIDTH)/2];
    if (col & 1) {
        pixelData &= 0x00FF;
        pixelData |= (colorIndex << 8);
    } else {
        pixelData &= 0xFF00;
        pixelData |= colorIndex;
    }
    videoBuffer[OFFSET(row, col, SCREENWIDTH)/2] = pixelData;
}

// Draw a rectangle at the specified location and size in Mode 3
void drawRect3(int row, int col, int height, int width, volatile unsigned short color) {

	for(int r = 0; r < height; r++) {
        DMANow(3, &color, &videoBuffer[OFFSET(row+r, col, SCREENWIDTH)], DMA_SOURCE_FIXED | width);
	}
}

// Draw a rectangle at the specified location and size in Mode 4
void drawRect4(int row, int col, int height, int width, volatile unsigned char colorIndex) {

    volatile unsigned short pixelData = colorIndex | (colorIndex<<8);
    for (int r = 0; r < height; r++) {
        if (col & 1) {
            if (width & 1) {  // Odd col, odd width
                setPixel4(row + r, col, colorIndex);
                if (width > 1) {
                    DMANow(3, &pixelData, &videoBuffer[OFFSET(r+row, col+1, SCREENWIDTH)/2], DMA_SOURCE_FIXED | width/2);
                }
            } else {  // Odd col, even width
                setPixel4(row + r, col, colorIndex);
                if (width > 2) {
                    DMANow(3, &pixelData, &videoBuffer[OFFSET(r+row, col+1, SCREENWIDTH)/2], DMA_SOURCE_FIXED | (width/2-1));
                }
                setPixel4(row + r, col+width-1, colorIndex);
            }
        } else {
            if (width & 1) {  // Even col, odd width
                if (width > 1) {
                    DMANow(3, &pixelData, &videoBuffer[OFFSET(r+row, col, SCREENWIDTH)/2], DMA_SOURCE_FIXED | width/2);
                }
                setPixel4(row + r, col+width-1, colorIndex);
            } else {  // Even col, even width
                DMANow(3, &pixelData, &videoBuffer[OFFSET(r+row, col, SCREENWIDTH)/2], DMA_SOURCE_FIXED | width/2);
            }
        }
    }
}

// Fill the entire screen with a single color in Mode 3
void fillScreen3(volatile unsigned short color) {

	DMANow(3, &color, videoBuffer, DMA_SOURCE_FIXED | (SCREENWIDTH * SCREENHEIGHT));
}

// Fill the entire screen with a single color in Mode 4
void fillScreen4(volatile unsigned char colorIndex) {

    volatile unsigned short pixelData = colorIndex | (colorIndex<<8);
    DMANow(3, &pixelData, videoBuffer, DMA_SOURCE_FIXED | (SCREENWIDTH * SCREENHEIGHT / 2));
}

// Draw an image at the specified location and size in Mode 3
void drawImage3(int row, int col, int height, int width, const unsigned short *image) {

    for(int r = 0; r < height; r++) {
        DMANow(3, &image[OFFSET(r, 0, width)], &videoBuffer[OFFSET(row+r, col, SCREENWIDTH)], width);
    }
}

// Draw an image at the specified location and size in Mode 4 (must be even col and width)
void drawImage4(int row, int col, int height, int width, const unsigned short *image) {

    for(int r = 0; r < height; r++) {
        DMANow(3, &image[OFFSET(r, 0, width)/2], &videoBuffer[OFFSET(row+r, col, SCREENWIDTH)/2], width/2);
    }
}

// Fill the entire screen with an image in Mode 3
void drawFullscreenImage3(const unsigned short *image) {

    DMANow(3, image, videoBuffer, SCREENWIDTH * SCREENHEIGHT);
}

// Fill the entire screen with an image in Mode 4
void drawFullscreenImage4(const unsigned short *image) {

    DMANow(3, image, videoBuffer, SCREENWIDTH * SCREENHEIGHT / 2);
}

// Pause code execution until vertical blank begins
void waitForVBlank() {

	while(SCANLINECOUNTER > 160);
	while(SCANLINECOUNTER < 160);
}

// Flips the page
void flipPage() {

    if (REG_DISPCTL & DISP_BACKBUFFER) {
        videoBuffer = BACKBUFFER;
    } else {
        videoBuffer = FRONTBUFFER;
    }
    REG_DISPCTL ^= DISP_BACKBUFFER;
}

// Set up and begin a DMA transfer
void DMANow(int channel, volatile const void *src, volatile void *dst, unsigned int cnt) {

    // Turn DMA off
    dma[channel].cnt = 0;

    // Set source and destination
    dma[channel].src = src;
    dma[channel].dst = dst;

    // Set control and begin
    dma[channel].cnt = cnt | DMA_ON;
}

// Return true if the two rectangular areas are overlapping
int collision(int rowA, int colA, int heightA, int widthA, int rowB, int colB, int heightB, int widthB) {

    return rowA < rowB + heightB - 1 && rowA + heightA - 1 > rowB
        && colA < colB + widthB - 1 && colA + widthA - 1 > colB;
}

int spriteCollision(ANISPRITE sprite1, ANISPRITE sprite2) {
    return collision(sprite1.screenRow + sprite1.rowOffset, sprite1.screenCol + sprite1.colOffset, sprite1.height, sprite1.width, sprite2.screenRow + sprite2.rowOffset, sprite2.screenCol + sprite2.colOffset, sprite2.height, sprite2.width);
}

// Hides all sprites in the shadowOAM
void hideSprites() {
    for (int i = 0; i < 125; i++) {
        shadowOAM[i].attr0 = ATTR0_HIDE;
    }
}

//gets a free OAMIndex
int getOAMIndex() {
    for (int i = 0; i < 125; i++) {
        if (oamIndexMask[i] == 0) {
            oamIndexMask[i] = 1;
            return i;
        }
    }
    return -1;
}

void freeOAMIndex(int i) {
    oamIndexMask[i] = 0;
}

void clearAllOAM() {
    for (int i = 0; i < 128; i++) {
        oamIndexMask[i] = 0;
    }
}

void printNum(int row, int col, int num) {
    int nums[3] = {-1,-1,-1};
    int i = 0;
    while (num > 0) {
        nums[i] = num % 10;
        num = num / 10;
        i++;
    }
    if (nums[2] != -1) {
        shadowOAM[125].attr0 = (row & ROWMASK) | ATTR0_REGULAR | ATTR0_TALL | ATTR0_4BPP;
        shadowOAM[125].attr1 = (col & COLMASK) | ATTR1_TINY;
        shadowOAM[125].attr2 = ATTR2_TILEID(24, nums[2]);
    }
    if (nums[1] != -1) {
        shadowOAM[126].attr0 = (row & ROWMASK) | ATTR0_REGULAR | ATTR0_TALL | ATTR0_4BPP;
        shadowOAM[126].attr1 = ((col+5) & COLMASK) | ATTR1_TINY;
        shadowOAM[126].attr2 = ATTR2_TILEID(24, nums[1]);
    }

    if (nums[0] != -1) {
        shadowOAM[127].attr0 = (row & ROWMASK) | ATTR0_REGULAR | ATTR0_TALL | ATTR0_4BPP;
        shadowOAM[127].attr1 = ((col+10) & COLMASK) | ATTR1_TINY;
        shadowOAM[127].attr2 = ATTR2_TILEID(24, nums[0]);
    }
}






