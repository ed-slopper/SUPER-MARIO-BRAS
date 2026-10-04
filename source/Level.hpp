#ifndef LEVEL_HPP
#define LEVEL_HPP

#include <cstdint>

class SMBEngine;

/**
 * A custom level: a grid of characters that replaces the game's own level while
 * "active" is on. It is kept in a text file that can be edited by hand or with the
 * in-game editor (Editor.cpp).
 *
 * The grid is HEIGHT rows high (four screens) and as wide as you like. The game only
 * ever shows SCREEN_ROWS rows at a time; a camera follows the player up and down.
 *
 * One character per 16x16 block:
 *   .  nothing             #  ground              B  brick
 *   ?  question block      M  question block with a power-up
 *   X  solid block         P  pipe (two side by side; the top is added automatically)
 *   =  bridge              o  coin
 *   g  goomba              k  koopa
 *   /  ramp, rising to the right      \  ramp, rising to the left
 *   12 gentle ramp rising to the right, two blocks long (1 = low half, 2 = high half)
 *   34 gentle ramp rising to the left (3 = high half, 4 = low half)
 *      (ramps are only a surface: put solid blocks under and behind them)
 *   S  where the player starts (he stands in this block)
 *   F  flagpole: marks the block it stands on; the pole is the 10 blocks above it
 *   C  castle: marks its bottom left corner; it is 5 wide and 5 high
 *
 * A file with 13 rows or fewer is an ordinary one-screen-high level: its rows are put
 * at BASE_ROW, leaving room to build above and below. A taller file fills the grid
 * from the top.
 */
namespace Level
{
    const int SCREEN_ROWS = 13;   // rows the game shows at once
    const int HEIGHT = 52;        // rows in the grid
    const int BASE_ROW = 26;      // where a one-screen level sits in the grid
    const int MAX_COLUMNS = 1024;
    const int CASTLE_WIDTH = 5;

    extern bool active;  // is the custom level being played instead of the game's own?

    bool load(const char* fileName);
    bool save(const char* fileName);

    int width();                           // number of columns in use
    char get(int column, int row);         // '.' outside the level
    void set(int column, int row, char c);

    int startColumn();                     // where the player starts
    int startRow();

    /**
     * Is there a ramp at a position? If so, gives the height of its surface above the
     * bottom of the block (0-16) at its left and right edges.
     */
    bool slopeAt(int column, int row, int& leftHeight, int& rightHeight);

    /** The level block ("metatile" number) shown at a position; 0 = nothing. */
    uint8_t metatileAt(int column, int row);

    /** Start (or restart) playing from the beginning: the custom level or the normal game. */
    void restart(SMBEngine& game, bool custom);

    /**
     * The camera. The game's own numbers for vertical positions only cover the 13 rows
     * on screen: its row 0 is row cameraRow() of the grid.
     */
    int cameraRow();

    /**
     * How many pixels (0-15) higher than the game's own numbers say everything has to
     * be drawn in the picture being shown, because the camera is between two rows.
     */
    int pictureOffset();

    /** Called once per frame while playing: moves the camera and keeps the game's idea of the blocks around the player up to date. */
    void update(SMBEngine& game);

    /** Called while the picture is drawn, after the game's background and before its sprites: draws the level. */
    void drawBackground(SMBEngine& game, uint32_t* buffer);

    // Hooks called from inside the game's own code (SMB.cpp, search for "MOD:")

    /** Called when the game is about to set up a level. */
    void beforeAreaLoad(SMBEngine& game);

    /** Called once the game has found its own level data. */
    void afterAreaLoad(SMBEngine& game);

    /** Called when the game has put the player at the start of a level. */
    void placePlayer(SMBEngine& game);

    /** Called for each column of the level as it scrolls into view, to fill in its 13 blocks. */
    void fillColumn(SMBEngine& game);

    /** Called after the game has noted those blocks for collisions, before it draws them. */
    void columnStored(SMBEngine& game);
}

#endif // LEVEL_HPP
