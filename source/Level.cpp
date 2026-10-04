// Level.cpp - custom levels. See Level.hpp for the file format.
//
// How it works. The original game knows one screen-height of level: 13 rows. It keeps
// the blocks near the player in two "block buffers" (one per 16-column page, 13 rows
// each) which it uses for all collisions, and it draws them into the background as
// they scroll into view.
//
// A custom level is taller than that, so here:
//  - "live" holds every block of the whole level as it is right now (bricks that have
//    been smashed are gone from it, and so on)
//  - a camera follows the player up and down. The game is only ever given the 13 rows
//    the camera is looking at: every frame update() copies them into the block buffers,
//    and copies back anything the game changed
//  - when the camera moves by a whole row, everything in the game (player, enemies,
//    fireballs...) is moved 16 pixels the other way, so the game never notices
//  - the game's own background is left empty, and drawBackground() draws the level
// Enemies are handed over in the game's own enemy list format, so they behave normally.

#include <cstdio>
#include <cstring>

#include "Emulation/PPU.hpp"
#include "SMB/SMBConstants.hpp"
#include "SMB/SMBEngine.hpp"

#include "Draw.hpp"
#include "Level.hpp"

bool Level::active = false;

namespace
{

char grid[Level::HEIGHT][Level::MAX_COLUMNS];     // the level as designed (see Level.hpp)
uint8_t live[Level::HEIGHT][Level::MAX_COLUMNS];  // the level being played, as game blocks
int levelWidth = 0;
int flagColumn = -1, flagRow = 0;      // the F, or column -1
int castleColumn = -1, castleRow = 0;  // the C, or column -1
int startAtColumn = 2, startAtRow = Level::BASE_ROW + 10;
int cameraMinRow = 0, cameraMaxRow = 0; // how far the camera may go
bool gridReady = false;

// The camera
int cameraPixels = 0;      // how far down the grid the top of the picture is, in pixels
// The picture on screen is always one frame behind the game (the sprites it shows were
// set up during the frame before), so it has to be drawn with the camera where it was
// one frame ago. Otherwise everything jumps 16 pixels for a frame whenever the camera
// crosses from one row to the next.
int picturePixels = 0;

// What update() last put in the game's block buffers, to spot what the game changes
uint8_t shadow[32][Level::SCREEN_ROWS];
int shadowColumn[32];
int shadowRow = 0;
bool shadowValid = false;
bool enemySeen[6];

const uint8_t AREA_1_1 = 0x25; // the game's number for level 1-1, whose sky and music are borrowed

// The small castle, top row first
const uint8_t castleBlocks[5][Level::CASTLE_WIDTH] = {
    {0x00, 0x45, 0x45, 0x45, 0x00},
    {0x00, 0x48, 0x47, 0x46, 0x00},
    {0x45, 0x49, 0x49, 0x49, 0x45},
    {0x47, 0x47, 0x4a, 0x47, 0x47},
    {0x47, 0x47, 0x4b, 0x47, 0x47},
};

void clearGrid()
{
    memset(grid, '.', sizeof(grid));
    levelWidth = 0;
    gridReady = true;
}

/** Work out the width, where the special things are, and how far the camera may move. */
void scan()
{
    levelWidth = 0;
    flagColumn = -1;
    castleColumn = -1;
    startAtColumn = 2;
    startAtRow = Level::BASE_ROW + 10;
    int firstRow = Level::HEIGHT, lastRow = -1;
    for (int row = 0; row < Level::HEIGHT; row++)
    {
        for (int column = 0; column < Level::MAX_COLUMNS; column++)
        {
            char c = grid[row][column];
            if (c == '.')
            {
                continue;
            }
            if (column + 1 > levelWidth) levelWidth = column + 1;
            if (row < firstRow) firstRow = row;
            if (row > lastRow) lastRow = row;
            if (c == 'F' && flagColumn < 0) { flagColumn = column; flagRow = row; }
            if (c == 'C' && castleColumn < 0) { castleColumn = column; castleRow = row; }
            if (c == 'S') { startAtColumn = column; startAtRow = row; }
        }
    }
    if (castleColumn >= 0 && levelWidth < castleColumn + Level::CASTLE_WIDTH)
    {
        levelWidth = castleColumn + Level::CASTLE_WIDTH;
    }
    if (lastRow < 0)
    {
        firstRow = Level::BASE_ROW;
        lastRow = Level::BASE_ROW + Level::SCREEN_ROWS - 1;
    }
    if (flagColumn >= 0 && flagRow - 10 < firstRow) firstRow = flagRow - 10;

    // The camera stops with the lowest row of the level at the bottom of the screen
    // (fall below that and you have fallen out of the world), and half a screen above
    // the highest thing in it
    cameraMaxRow = lastRow - (Level::SCREEN_ROWS - 1);
    if (cameraMaxRow > Level::HEIGHT - Level::SCREEN_ROWS) cameraMaxRow = Level::HEIGHT - Level::SCREEN_ROWS;
    if (cameraMaxRow < 0) cameraMaxRow = 0;
    cameraMinRow = firstRow - 6;
    if (cameraMinRow < 0) cameraMinRow = 0;
    if (cameraMinRow > cameraMaxRow) cameraMinRow = cameraMaxRow;
}

/**
 * The game only keeps blocks that can be touched in its block buffers; scenery such as
 * the castle is left out. This is the game's own rule for which is which.
 */
uint8_t touchable(uint8_t block)
{
    static const uint8_t lowest[4] = {0x10, 0x51, 0x88, 0xc0};
    return (block >= lowest[block >> 6]) ? block : 0;
}

uint8_t liveAt(int column, int row)
{
    if (column < 0 || column >= Level::MAX_COLUMNS || row < 0 || row >= Level::HEIGHT)
    {
        return 0;
    }
    return live[row][column];
}

/** Where in the game's memory it keeps a column of blocks (it has room for 32 columns). */
int blockBufferAddress(int column)
{
    return (((column >> 4) & 1) ? Block_Buffer_2 : Block_Buffer_1) + (column & 15);
}

/** Read a 16-bit vertical position the way the game stores them, add to it and store it back. */
void shiftPosition(SMBEngine& game, int highAddress, int lowAddress, int pixels)
{
    int value = game.mem(highAddress) * 256 + game.mem(lowAddress) + pixels;
    if (value < 0) value = 0;
    if (value > 0xffff) value = 0xffff;
    game.mem(highAddress) = (uint8_t)(value >> 8);
    game.mem(lowAddress) = (uint8_t)value;
}

/** Move everything in the game up or down, to make up for the camera moving by whole rows. */
void shiftEverything(SMBEngine& game, int pixels)
{
    // the player, 6 enemies, 2 fireballs, 4 bumped blocks, 9 odds and ends, 3 bubbles
    for (int i = 0; i < 25; i++)
    {
        shiftPosition(game, SprObject_Y_HighPos + i, SprObject_Y_Position + i, pixels);
    }
    shiftPosition(game, JumpOrigin_Y_HighPos, JumpOrigin_Y_Position, pixels);
    for (int i = 0; i < 7; i++) // the little score numbers that float up
    {
        int y = game.mem(FloateyNum_Y_Pos + i) + pixels;
        game.mem(FloateyNum_Y_Pos + i) = (uint8_t)(y < 0 ? 0 : y > 255 ? 255 : y);
    }
    int y = game.mem(FlagpoleFNum_Y_Pos) + pixels;
    game.mem(FlagpoleFNum_Y_Pos) = (uint8_t)(y < 0 ? 0 : y > 255 ? 255 : y);
}

void storePosition(SMBEngine& game, int slot, int y)
{
    // the game's vertical positions have a "high" part that is 1 for things on screen
    int value = 256 + y;
    if (value < 0) value = 0;
    game.mem(Enemy_Y_HighPos + slot) = (uint8_t)(value >> 8);
    game.mem(Enemy_Y_Position + slot) = (uint8_t)value;
}

/** How far along the level the left edge of the picture being drawn is, in pixels. */
int pictureLeft(SMBEngine& game)
{
    // The picture can be a frame behind the game's own numbers, so trust the scroll
    // it was drawn with
    int worldLeft = game.mem(ScreenLeft_PageLoc) * 256 + game.mem(ScreenLeft_X_Pos);
    int difference = (worldLeft - game.getPPU().getScrollX()) & 511;
    if (difference > 256) difference -= 512;
    return worldLeft - difference;
}

} // end of private section

int Level::width() { return levelWidth; }
int Level::startColumn() { return startAtColumn; }
int Level::startRow() { return startAtRow; }
int Level::cameraRow() { return cameraPixels / 16; }
int Level::pictureOffset() { return picturePixels % 16; }

char Level::get(int column, int row)
{
    if (!gridReady) clearGrid();
    if (column < 0 || column >= MAX_COLUMNS || row < 0 || row >= HEIGHT)
    {
        return '.';
    }
    return grid[row][column];
}

void Level::set(int column, int row, char c)
{
    if (!gridReady) clearGrid();
    if (column < 0 || column >= MAX_COLUMNS || row < 0 || row >= HEIGHT)
    {
        return;
    }
    grid[row][column] = c;
    scan();
}

bool Level::load(const char* fileName)
{
    clearGrid();
    FILE* file = fopen(fileName, "r");
    if (file == NULL)
    {
        scan();
        return false;
    }
    // Read every row first: how many there are decides where they go
    static char lines[HEIGHT][MAX_COLUMNS + 16];
    int count = 0;
    while (count < HEIGHT && fgets(lines[count], sizeof(lines[count]), file) != NULL)
    {
        if (lines[count][0] != ';') // a line starting with ; is a comment
        {
            count++;
        }
    }
    fclose(file);

    int firstRow = (count <= SCREEN_ROWS) ? BASE_ROW : 0;
    for (int i = 0; i < count; i++)
    {
        const char* line = lines[i];
        for (int column = 0; column < MAX_COLUMNS && line[column] != 0 && line[column] != '\n' && line[column] != '\r'; column++)
        {
            grid[firstRow + i][column] = (line[column] == ' ') ? '.' : line[column];
        }
    }
    scan();
    return true;
}

bool Level::save(const char* fileName)
{
    if (!gridReady) clearGrid();
    FILE* file = fopen(fileName, "w");
    if (file == NULL)
    {
        return false;
    }
    fprintf(file, "; Custom level, one character per block.\n");
    fprintf(file, "; . nothing  # ground  B brick  ? question block  M power-up block  X solid block\n");
    fprintf(file, "; P pipe  = bridge  o coin  g goomba  k koopa  S start  F flagpole  C castle (bottom left)\n");
    fprintf(file, "; ramps: / up to the right  \\ up to the left  12 gentle up to the right  34 gentle up to the left\n");

    // A level that only uses the 13 base rows is saved as just those; a taller one is
    // saved whole (52 rows: the base rows are rows 27 to 39 of them)
    bool tall = false;
    for (int row = 0; row < HEIGHT; row++)
    {
        for (int column = 0; column < levelWidth; column++)
        {
            if (grid[row][column] != '.' && (row < BASE_ROW || row >= BASE_ROW + SCREEN_ROWS))
            {
                tall = true;
            }
        }
    }
    int columns = (levelWidth < 16) ? 16 : levelWidth;
    int firstRow = tall ? 0 : BASE_ROW;
    int rows = tall ? HEIGHT : SCREEN_ROWS;
    if (tall)
    {
        fprintf(file, "; This level is %d rows high. The row the original ground is on is row %d.\n", HEIGHT, BASE_ROW + 12);
    }
    for (int row = firstRow; row < firstRow + rows; row++)
    {
        fwrite(grid[row], 1, columns, file);
        fputc('\n', file);
    }
    fclose(file);
    return true;
}

bool Level::slopeAt(int column, int row, int& leftHeight, int& rightHeight)
{
    switch (get(column, row))
    {
    case '/':  leftHeight = 0;  rightHeight = 16; return true;
    case '\\': leftHeight = 16; rightHeight = 0;  return true;
    case '1':  leftHeight = 0;  rightHeight = 8;  return true;
    case '2':  leftHeight = 8;  rightHeight = 16; return true;
    case '3':  leftHeight = 16; rightHeight = 8;  return true;
    case '4':  leftHeight = 8;  rightHeight = 0;  return true;
    default:   return false;
    }
}

uint8_t Level::metatileAt(int column, int row)
{
    if (flagColumn >= 0 && column == flagColumn && row <= flagRow && row >= flagRow - 10)
    {
        if (row == flagRow) return 0x61;      // block it stands on
        if (row == flagRow - 10) return 0x24; // ball on top
        return 0x25;                          // pole
    }
    if (castleColumn >= 0 && column >= castleColumn && column < castleColumn + CASTLE_WIDTH && row <= castleRow && row >= castleRow - 4)
    {
        uint8_t block = castleBlocks[row - (castleRow - 4)][column - castleColumn];
        if (row == castleRow && column == castleColumn + 3)
        {
            block = 0x52; // a solid brick inside the doorway, which is what stops the player
        }
        if (block != 0)
        {
            return block;
        }
    }

    switch (get(column, row))
    {
    case '#': return 0x54;
    case 'B': return 0x51;
    case '?': return 0xc0;
    case 'M': return 0xc1;
    case 'X': return 0x61;
    case '=': return 0x63;
    case 'o': return 0xc2;
    case 'P':
    {
        // left or right half? count the pipe blocks to the left of this one
        int run = 0;
        while (get(column - run - 1, row) == 'P')
        {
            run++;
        }
        bool right = (run % 2) == 1;
        bool top = get(column, row - 1) != 'P';
        // 0x12/0x13 are the pipe tops that can't be entered (0x10/0x11 are the warp pipe tops)
        return top ? (right ? 0x13 : 0x12) : (right ? 0x15 : 0x14);
    }
    default:
        return 0;
    }
}

void Level::restart(SMBEngine& game, bool custom)
{
    active = custom;
    // The same things the game does itself to restart a level after losing a life
    game.mem(WorldNumber) = 0;
    game.mem(LevelNumber) = 0;
    game.mem(AreaNumber) = 0;
    game.mem(AreaPointer) = AREA_1_1;
    game.mem(HalfwayPage) = 0;
    game.mem(DisableScreenFlag) = 1;
    game.mem(Sprite0HitDetectFlag) = 0;
    game.mem(EventMusicQueue) = Silence;
    game.mem(PlayerSize) = 1;   // small
    game.mem(PlayerStatus) = 0;
    game.mem(FetchNewGameTimerFlag) = 1;
    game.mem(TimerControl) = 0;
    game.mem(GamePauseStatus) = 0;
    game.mem(GameEngineSubroutine) = 0;
    game.mem(NumberofLives) = 2;
    game.mem(OperMode) = GameModeValue;
    game.mem(OperMode_Task) = 0; // 0 = set the level up from scratch
}

void Level::beforeAreaLoad(SMBEngine& game)
{
    if (!active)
    {
        return;
    }
    if (!gridReady) clearGrid();

    // Whatever level the game thinks is next, play the custom one (dressed as 1-1)
    game.mem(WorldNumber) = 0;
    game.mem(LevelNumber) = 0;
    game.mem(AreaNumber) = 0;
    game.mem(AreaPointer) = AREA_1_1;
    // A level is divided into "pages" of 16 columns. The game can start a level at a
    // later page (it does so for its halfway points), which is how the start is moved.
    game.mem(HalfwayPage) = (uint8_t)(startAtColumn / 16);

    // Start with every block as designed
    for (int row = 0; row < HEIGHT; row++)
    {
        for (int column = 0; column < MAX_COLUMNS; column++)
        {
            live[row][column] = (column < levelWidth) ? metatileAt(column, row) : 0;
        }
    }
    shadowValid = false;
    memset(enemySeen, 0, sizeof(enemySeen));

    // Point the camera so that the player starts where he normally stands on screen
    int row = startAtRow - 10;
    if (row > cameraMaxRow) row = cameraMaxRow;
    if (row < cameraMinRow) row = cameraMinRow;
    cameraPixels = row * 16;
    picturePixels = cameraPixels;
}

void Level::afterAreaLoad(SMBEngine& game)
{
    if (!active)
    {
        return;
    }

    // Write the enemies as the game's own enemy list: two bytes each, in order from left
    // to right.
    //   page entry: 0x0f, page number
    //   enemy:      (column within page * 16 + row), kind of enemy
    //   end:        0xff
    // The row given here doesn't matter: update() puts each enemy at its real height
    // when the game brings it to life.
    uint8_t* list = game.customData();
    int length = 0;
    int currentPage = -1;
    for (int column = 0; column < levelWidth && length < 500; column++)
    {
        for (int row = 0; row < HEIGHT; row++)
        {
            char c = get(column, row);
            if (c != 'g' && c != 'k')
            {
                continue;
            }
            int page = column / 16;
            if (page != currentPage)
            {
                list[length++] = 0x0f;
                list[length++] = (uint8_t)page;
                currentPage = page;
            }
            list[length++] = (uint8_t)(((column % 16) << 4) | 0x0b);
            list[length++] = (c == 'g') ? Goomba : GreenKoopa;
            break; // one enemy per column
        }
    }
    list[length] = 0xff;

    uint16_t address = game.customDataAddress();
    game.mem(EnemyDataLow) = address & 0xff;
    game.mem(EnemyDataHigh) = address >> 8;
}

void Level::placePlayer(SMBEngine& game)
{
    if (!active)
    {
        return;
    }
    // The game has put him near the left of the starting page, on the ground of a normal level
    int x = (startAtColumn % 16) * 16 + 8;
    game.mem(Player_X_Position) = (uint8_t)(x > 240 ? 240 : x);
    // standing in the start block: his position is where the top of big mario would be
    game.mem(Player_Y_Position) = (uint8_t)((startAtRow - cameraRow() + 1) * 16);
}

void Level::fillColumn(SMBEngine& game)
{
    int page = game.mem(CurrentPageLoc);
    int column = page * 16 + game.mem(CurrentColumnPos);
    int topRow = cameraRow();

    // Hand the game the 13 blocks the camera can see in this column (it has already
    // put its own scenery there, which is thrown away)
    for (int row = 0; row < SCREEN_ROWS; row++)
    {
        game.mem(MetatileBuffer + row) = liveAt(column, topRow + row);
    }

    // The flagpole and the castle each come with an object that the game normally
    // creates when it builds them
    if (column == flagColumn)
    {
        // the flag itself, in the last of the game's six enemy slots
        int x = (column % 16) * 16 - 8;
        game.mem(Enemy_X_Position + 5) = (uint8_t)x;
        game.mem(Enemy_PageLoc + 5) = (uint8_t)(page - (x < 0 ? 1 : 0));
        storePosition(game, 5, 0x30 + (flagRow - 10 - topRow) * 16);
        int numberY = 0xb0 + (flagRow - 10 - topRow) * 16;
        game.mem(FlagpoleFNum_Y_Pos) = (uint8_t)(numberY < 0 ? 0 : numberY > 255 ? 255 : numberY);
        game.mem(Enemy_ID + 5) = FlagpoleFlagObject;
        game.mem(Enemy_Flag + 5) = 1;
        enemySeen[5] = true;
    }
    if (castleColumn >= 0 && column == castleColumn + 2)
    {
        // the little flag that rises from the castle, which also runs the end-of-level
        // countdown; it needs a free enemy slot
        for (int slot = 0; slot < 5; slot++)
        {
            if (game.mem(Enemy_Flag + slot) == 0)
            {
                game.mem(Enemy_X_Position + slot) = (uint8_t)((column % 16) * 16);
                game.mem(Enemy_PageLoc + slot) = (uint8_t)page;
                game.mem(Enemy_Flag + slot) = 1;
                storePosition(game, slot, 0x90 + (castleRow - 10 - topRow) * 16);
                game.mem(Enemy_ID + slot) = StarFlagObject;
                enemySeen[slot] = true;
                break;
            }
        }
    }
    if (castleColumn >= 0 && column == castleColumn + 9)
    {
        game.mem(ScrollLock) = 1; // the screen stops scrolling once the castle is in view
    }
}

void Level::columnStored(SMBEngine& game)
{
    if (!active)
    {
        return;
    }
    // The game is about to draw the column into its background. Give it nothing to
    // draw: drawBackground() draws the level instead, because the game's background
    // can't scroll up and down.
    for (int row = 0; row < SCREEN_ROWS; row++)
    {
        game.mem(MetatileBuffer + row) = 0;
    }
}

void Level::update(SMBEngine& game)
{
    if (!active || game.mem(OperMode) != GameModeValue || game.mem(OperMode_Task) != 3)
    {
        return;
    }

    picturePixels = cameraPixels; // where the camera was for the frame about to be shown

    // 1. What has the game changed since last frame? (a brick smashed, a coin taken...)
    if (shadowValid)
    {
        for (int i = 0; i < 32; i++)
        {
            int column = shadowColumn[i];
            int address = blockBufferAddress(column);
            for (int row = 0; row < SCREEN_ROWS; row++)
            {
                uint8_t now = game.mem(address + row * 16);
                int levelRow = shadowRow + row;
                if (now != shadow[i][row] && column >= 0 && column < MAX_COLUMNS && levelRow < HEIGHT)
                {
                    live[levelRow][column] = now;
                }
            }
        }
    }

    // 2. Move the camera. It follows the player when he gets near the top or bottom of
    //    the screen.
    int oldRow = cameraRow();
    int target = cameraPixels;
    if (game.mem(GameEngineSubroutine) == 0x0b)
    {
        // he is dying: leave the camera where it is
    }
    else if (game.mem(Player_Y_HighPos) == 1)
    {
        int playerY = game.mem(Player_Y_Position) + oldRow * 16; // in the grid, in pixels
        if (playerY - cameraPixels < 64) target = playerY - 64;
        if (playerY - cameraPixels > 128) target = playerY - 128;
    }
    else if (game.mem(Player_Y_HighPos) >= 2)
    {
        target = cameraPixels + 64; // he has dropped off the bottom: chase him
    }
    int playerColumn = game.mem(Player_PageLoc) * 16 + game.mem(Player_X_Position) / 16;
    if (flagColumn >= 0 && playerColumn >= flagColumn - 12)
    {
        // The end of the level only works with the flagpole in its usual place on screen
        target = (flagRow - 10) * 16;
    }
    if (target > cameraMaxRow * 16) target = cameraMaxRow * 16;
    if (target < cameraMinRow * 16) target = cameraMinRow * 16;
    const int speed = 6; // pixels per frame
    if (target > cameraPixels + speed) target = cameraPixels + speed;
    if (target < cameraPixels - speed) target = cameraPixels - speed;
    cameraPixels = target;

    int topRow = cameraRow();
    if (topRow != oldRow)
    {
        shiftEverything(game, (oldRow - topRow) * 16);
    }

    // 3. Enemies the game has just brought to life: put them at their real height
    for (int slot = 0; slot < 5; slot++)
    {
        bool alive = game.mem(Enemy_Flag + slot) != 0;
        if (alive && !enemySeen[slot] && (game.mem(Enemy_ID + slot) == Goomba || game.mem(Enemy_ID + slot) == GreenKoopa))
        {
            int column = game.mem(Enemy_PageLoc + slot) * 16 + game.mem(Enemy_X_Position + slot) / 16;
            for (int row = 0; row < HEIGHT; row++)
            {
                char c = get(column, row);
                if (c == 'g' || c == 'k')
                {
                    // an enemy standing in a block is 8 pixels above the block's top
                    storePosition(game, slot, (row - topRow) * 16 + 32 - 8);
                    break;
                }
            }
        }
        enemySeen[slot] = alive;
    }

    // 4. Give the game the blocks around the screen, at the camera's height
    int leftColumn = game.mem(ScreenLeft_PageLoc) * 16 + game.mem(ScreenLeft_X_Pos) / 16;
    for (int column = leftColumn - 7; column < leftColumn + 25; column++)
    {
        int i = column & 31;
        int address = blockBufferAddress(column);
        shadowColumn[i] = column;
        for (int row = 0; row < SCREEN_ROWS; row++)
        {
            uint8_t block = touchable(liveAt(column, topRow + row));
            game.mem(address + row * 16) = block;
            shadow[i][row] = block;
        }
    }
    shadowRow = topRow;
    shadowValid = true;
}

void Level::drawBackground(SMBEngine& game, uint32_t* buffer)
{
    if (!active || game.mem(OperMode) != GameModeValue || game.mem(OperMode_Task) != 3)
    {
        return;
    }
    int worldLeft = pictureLeft(game);
    int topRow = picturePixels / 16;
    int offset = picturePixels % 16;

    Draw::setTop(32); // keep off the score at the top of the screen

    // A few clouds, drifting past more slowly than the level to look far away
    for (int i = worldLeft / 2 / 112; i <= worldLeft / 2 / 112 + 3; i++)
    {
        unsigned random = (unsigned)i * 2654435761u;
        int x = i * 112 + (int)((random >> 8) % 48) - worldLeft / 2;
        int y = 44 + (int)((random >> 16) % 4) * 16 + (cameraMaxRow * 16 - picturePixels) / 4;
        int length = 1 + (int)((random >> 24) % 3);
        for (int part = 0; part < length + 2; part++)
        {
            uint8_t top = (part == 0) ? 0x80 : (part == length + 1) ? 0x82 : 0x81;
            Draw::metatile(game, buffer, x + part * 16, y, top);
            Draw::metatile(game, buffer, x + part * 16, y + 16, top + 3);
        }
    }

    // The level: the 13 rows the camera is on, plus one more for when it is part-way down
    for (int column = worldLeft / 16; column <= worldLeft / 16 + 16; column++)
    {
        for (int row = 0; row <= SCREEN_ROWS; row++)
        {
            int x = column * 16 - worldLeft;
            int y = 32 + row * 16 - offset;
            uint8_t block = liveAt(column, topRow + row);
            if (block != 0)
            {
                Draw::metatile(game, buffer, x, y, block);
            }
            int left, right;
            if (slopeAt(column, topRow + row, left, right))
            {
                Draw::slope(game, buffer, x, y, left, right);
            }
        }
    }
    Draw::setTop(0);
}
