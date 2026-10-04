#ifndef DRAW_HPP
#define DRAW_HPP

#include <cstdint>

class SMBEngine;

/**
 * Small drawing helpers for putting things on top of the game's picture.
 * The picture is 256x240 pixels; colors are 0xffRRGGBB.
 */
namespace Draw
{
    void setPixel(uint32_t* buffer, int x, int y, uint32_t color);

    /** Nothing is drawn above this row of the picture until it is set back to 0. */
    void setTop(int y);
    void fillRect(uint32_t* buffer, int x, int y, int width, int height, uint32_t color);

    /** The outline of a rectangle, one pixel thick. */
    void frame(uint32_t* buffer, int x, int y, int width, int height, uint32_t color);

    /** Text in the game's own font: letters, digits, space, - and !. Each character is 8 wide. */
    void text(SMBEngine& game, uint32_t* buffer, int x, int y, const char* text, uint32_t color);

    /** Text with a dark shadow, readable on any background. */
    void shadowText(SMBEngine& game, uint32_t* buffer, int x, int y, const char* text, uint32_t color);

    /** One 16x16 level block (a "metatile", see docs/smbdis.asm) in the level's current colors. */
    void metatile(SMBEngine& game, uint32_t* buffer, int x, int y, uint8_t metatile);

    /**
     * A ramp filling one 16x16 block, in the colors of the ground. The heights are of its
     * surface above the bottom of the block, at its left and right edges (0-16).
     */
    void slope(SMBEngine& game, uint32_t* buffer, int x, int y, int leftHeight, int rightHeight);

    /** One 8x8 sprite tile, using sprite palette 0-3. */
    void spriteTile(SMBEngine& game, uint32_t* buffer, int x, int y, uint8_t tile, int palette, bool flip);
}

#endif // DRAW_HPP
