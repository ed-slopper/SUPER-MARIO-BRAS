#include "Emulation/PPU.hpp"
#include "SMB/SMBEngine.hpp"

#include "Draw.hpp"

extern const uint32_t* paletteRGB; // the NES's 64 colors as RGB (see Emulation/PPU.cpp)

static int topRow = 0;

void Draw::setTop(int y)
{
    topRow = y;
}

void Draw::setPixel(uint32_t* buffer, int x, int y, uint32_t color)
{
    if (x >= 0 && x < 256 && y >= topRow && y < 240)
    {
        buffer[y * 256 + x] = color;
    }
}

void Draw::fillRect(uint32_t* buffer, int x, int y, int width, int height, uint32_t color)
{
    for (int row = 0; row < height; row++)
    {
        for (int column = 0; column < width; column++)
        {
            setPixel(buffer, x + column, y + row, color);
        }
    }
}

void Draw::frame(uint32_t* buffer, int x, int y, int width, int height, uint32_t color)
{
    fillRect(buffer, x, y, width, 1, color);
    fillRect(buffer, x, y + height - 1, width, 1, color);
    fillRect(buffer, x, y, 1, height, color);
    fillRect(buffer, x + width - 1, y, 1, height, color);
}

void Draw::text(SMBEngine& game, uint32_t* buffer, int x, int y, const char* text, uint32_t color)
{
    for (; *text != 0; text++, x += 8)
    {
        char c = *text;
        int tile;
        if (c >= '0' && c <= '9') tile = c - '0';
        else if (c >= 'A' && c <= 'Z') tile = 10 + (c - 'A');
        else if (c >= 'a' && c <= 'z') tile = 10 + (c - 'a');
        else if (c == '-') tile = 0x28;
        else if (c == '!') tile = 0x2b;
        else continue; // space or unknown

        // background tiles are in the second half of the graphics data, 16 bytes per tile
        const uint8_t* data = game.getGraphics() + 0x1000 + tile * 16;
        for (int row = 0; row < 8; row++)
        {
            uint8_t bits = data[row] | data[row + 8];
            for (int column = 0; column < 8; column++)
            {
                if (bits & (0x80 >> column))
                {
                    setPixel(buffer, x + column, y + row, color);
                }
            }
        }
    }
}

void Draw::shadowText(SMBEngine& game, uint32_t* buffer, int x, int y, const char* text, uint32_t color)
{
    Draw::text(game, buffer, x + 1, y + 1, text, 0xff000000);
    Draw::text(game, buffer, x, y, text, color);
}

// Draw one 8x8 tile. Each pixel is a number 0-3 (0 = see-through) that picks a color
// from a palette of four.
static void drawTile(uint32_t* buffer, int x, int y, const uint8_t* data, const uint8_t* palette, bool flip)
{
    for (int row = 0; row < 8; row++)
    {
        for (int column = 0; column < 8; column++)
        {
            int bit = 0x80 >> column;
            int index = ((data[row] & bit) ? 1 : 0) + ((data[row + 8] & bit) ? 2 : 0);
            if (index != 0)
            {
                Draw::setPixel(buffer, x + (flip ? 7 - column : column), y + row, 0xff000000 | paletteRGB[palette[index] & 63]);
            }
        }
    }
}

void Draw::metatile(SMBEngine& game, uint32_t* buffer, int x, int y, uint8_t metatile)
{
    // The top two bits of a metatile number say which of the 4 background palettes it uses
    const uint8_t* tiles = game.getMetatileTiles(metatile);
    const uint8_t* palette = game.getPPU().getPalette() + (metatile >> 6) * 4;
    const uint8_t* graphics = game.getGraphics() + 0x1000;
    drawTile(buffer, x, y, graphics + tiles[0] * 16, palette, false);         // top left
    drawTile(buffer, x, y + 8, graphics + tiles[1] * 16, palette, false);     // bottom left
    drawTile(buffer, x + 8, y, graphics + tiles[2] * 16, palette, false);     // top right
    drawTile(buffer, x + 8, y + 8, graphics + tiles[3] * 16, palette, false); // bottom right
}

void Draw::spriteTile(SMBEngine& game, uint32_t* buffer, int x, int y, uint8_t tile, int palette, bool flip)
{
    drawTile(buffer, x, y, game.getGraphics() + tile * 16, game.getPPU().getPalette() + 16 + palette * 4, flip);
}

void Draw::slope(SMBEngine& game, uint32_t* buffer, int x, int y, int leftHeight, int rightHeight)
{
    // the ground block uses background palette 1: light, orange, black
    const uint8_t* palette = game.getPPU().getPalette() + 4;
    uint32_t light = 0xff000000 | paletteRGB[palette[1] & 63];
    uint32_t body = 0xff000000 | paletteRGB[palette[2] & 63];
    uint32_t dark = 0xff000000 | paletteRGB[palette[3] & 63];

    for (int column = 0; column < 16; column++)
    {
        // height of the surface in this column of pixels
        int height = (leftHeight * (31 - 2 * column) + rightHeight * (2 * column + 1) + 16) / 32;
        for (int i = 0; i < height; i++)
        {
            int row = 16 - height + i;        // 0 = top of the block
            uint32_t color = body;
            if (i == 0) color = light;        // the riding surface
            else if (i == 1) color = dark;
            else if (row == 15 || (row == 7 && column != 0)) color = dark; // lines like the ground blocks
            else if (column == ((row < 8) ? 11 : 3)) color = dark;
            setPixel(buffer, x + column, y + row, color);
        }
    }
}
