// Mods.cpp - all game mods live in this file.
//
// MENU: press Esc or Tab (or click MENU in the corner), then click with the mouse.
//
// GUN: E fires. It is a bolt-action sniper rifle: one fast shot, then a short reload.
//
// SKATE controls (see the "Skateboard" section for how it works):
//   A / D on the ground        push / brake (let go to keep rolling)
//   S on the ground            crouch: hold it for a moment, then jump, for a bigger ollie
//   Space                      ollie
//   Up arrow on the ground     manual (ride on the back wheels). Hold it as you land and
//                              the combo carries on; let go to bank it
//   Left / Right arrow in air  spin (land straight or backwards, not sideways!)
//   Up / Down arrow in air     frontflip / backflip (land the right way up!)
//   S in the air               grab the board (let go before landing!)
//   Land on a pipe, bricks, blocks or a bridge with some speed to grind
//   (keep spinning as you land on it for a boardslide)

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Emulation/Controller.hpp"
#include "Emulation/PPU.hpp"
#include "SMB/SMBConstants.hpp"
#include "SMB/SMBEngine.hpp"

#include "Draw.hpp"
#include "Editor.hpp"
#include "Level.hpp"
#include "Mods.hpp"

namespace
{

//---------------------------------------------------------------------
// The list of mods
//---------------------------------------------------------------------

enum ModId
{
    MOD_FLY,
    MOD_COLOR,
    MOD_BIG,
    MOD_GUN,
    MOD_SKATE,
    MOD_COUNT
};

struct Mod
{
    const char* name; // shown in the menu (letters, digits, space, - and !)
    bool on;
};

Mod mods[MOD_COUNT] = {
    {"FLY", false},
    {"COLOR", false},
    {"BIG", false},
    {"GUN", false},
    {"SKATE", false},
};

//---------------------------------------------------------------------
// Settings
//---------------------------------------------------------------------

const uint8_t FLY_RISE_SPEED = 0xfd;  // vertical speed while flying (-3; 0xfc = -4 is faster)
const uint8_t FLY_CEILING = 0x30;     // don't fly higher than this Y position

const int GUN_Y_BIG = 12;             // where the top of the gun is drawn, in pixels below the top of the player
const int GUN_Y_SMALL = 21;          // same for small or crouching player
const int GUN_X = -5;                 // where the butt of the gun is, relative to the player's back edge
const int GUN_BARREL_ROW = 5;         // which row of the gun picture the barrel is on
const uint8_t BULLET_SPEED = 0x78;    // horizontal speed of bullets (normal fireballs are 0x40, the most is 0x7f)
const int GUN_RELOAD_FRAMES = 35;     // time between shots (working the bolt)
const int GUN_FLASH_FRAMES = 4;       // how long the muzzle flash and tracer show

// Skateboard. Speeds are in the game's units: 16 = one pixel per frame (mario runs at 40).
const float SKATE_MAX_SPEED = 48;      // fastest you can get by pushing
const float SKATE_PUSH_SLOW = 1.0f;    // speed gained per frame when pushing from a standstill...
const float SKATE_PUSH_FAST = 0.15f;   // ...fading to this near top speed
const float SKATE_ROLL_FRICTION = 0.07f; // speed lost per frame when just rolling
const float SKATE_TUCK_FRICTION = 0.03f; // same while crouching
const float SKATE_BRAKE = 1.5f;        // speed lost per frame when holding the opposite direction
const float SKATE_AIR_STEER = 0.5f;    // speed change per frame from A / D in the air...
const float SKATE_AIR_STEER_MAX = 20;  // ...which can't take you faster than this
const float SKATE_GRIND_PUSH = 0.3f;   // speed gained per frame while grinding...
const float SKATE_GRIND_MAX_SPEED = 58; // ...up to this (faster than you can push)
const float SKATE_MANUAL_MIN_SPEED = 4; // too slow and the manual drops
const int   MANUAL_LIFT = 5;           // how many pixels the nose of the board comes up in a manual
const float SKATE_GRIND_MIN_SPEED = 8; // slower than this and you just land instead of grinding
const int   SKATE_CHARGE_FRAMES = 12;  // how long to crouch for a big ollie
const uint8_t SKATE_BIG_OLLIE_SPEED = 0xf9; // vertical speed of a big ollie (-7; a normal jump is -4 or -5)
const float SPIN_ACCELERATION = 2.0f;  // degrees per frame, per frame, while holding left/right in the air
const float SPIN_MAX = 15;             // fastest spin in degrees per frame
const float SPIN_DAMPING = 0.85f;      // spin keeps this fraction of its speed each frame when you let go
const float SPIN_STRAIGHTEN_RANGE = 60; // let go within this many degrees of straight and the board lines itself up
const float SPIN_STRAIGHTEN = 0.2f;    // how quickly it lines up (fraction of the remaining angle per frame)
const float FLIP_ACCELERATION = 2.0f;  // same for flips (up / down arrow in the air)
const float FLIP_MAX = 14;
const float FLIP_DAMPING = 0.85f;
const float FLIP_STRAIGHTEN_RANGE = 75; // let go within this many degrees of upright and he rights himself
const float FLIP_CLEAN = 30;           // land within this many degrees of upright for a clean landing
const float FLIP_SKETCHY = 60;         // up to here it is sketchy; beyond is a bail
const float SLOPE_GRAVITY = 0.35f;     // speed lost per frame going up a 45 degree ramp (gained going down)
const float SLOPE_LAUNCH = 1.6f;       // how hard a ramp throws you into the air when you ride off its top
const uint8_t SLOPE_LAUNCH_GRAVITY = 0x30; // how fast you come back down after that (the game uses 0x28 to 0x90; lower = floatier)
const float LANDING_CLEAN = 30;        // land within this many degrees of straight for a clean landing
const float LANDING_SKETCHY = 55;      // up to here it is a sketchy landing; beyond is a bail
const int   BAIL_FRAMES = 40;          // how long you are stopped after a bail
const int   BOARD_LIFT = 3;            // mario is drawn this many pixels higher so he stands on the board

// The gun, facing right: a long bolt-action sniper rifle with a scope.
// Each character is one pixel; see gunColor() for the colors.
const int GUN_WIDTH = 30;
const int GUN_HEIGHT = 9;
const char* const gunPicture[GUN_HEIGHT] = {
    "          #######             ",
    "          #ggggg#             ",
    "           #   #              ",
    " ####################         ",
    "#ddddd#mmmmmmmmmmmmm##########",
    "#dddd##mmmmmmmmmmmmm---------#",
    "#ddd# #kk# #m#      ##########",
    " ###  #kk# #m#                ",
    "      ####  #                 ",
};

uint32_t gunColor(char c)
{
    switch (c)
    {
    case '#': return 0xff101010; // outline
    case '-': return 0xffb0b0b8; // barrel
    case 'm': return 0xff707078; // receiver
    case 'd': return 0xff4a5a2a; // stock
    case 'k': return 0xff303030; // grip
    case 'g': return 0xff58c8f8; // scope glass
    default:  return 0;          // transparent
    }
}

//---------------------------------------------------------------------
// State
//---------------------------------------------------------------------

// Everything the skateboard remembers between frames
struct Skate
{
    float speed = 0;          // horizontal speed, with fractions (the game only stores whole numbers)
    int lastWritten = 0;      // the whole number last given to the game, to notice when the game changes it
    bool wasInAir = false;
    bool jumped = false;      // did this air time start with an ollie (rather than rolling off an edge)?
    bool bigOllie = false;
    int crouchCharge = 0;     // frames spent crouching on the ground
    float angle = 0;          // how far the skater has spun in the air, in degrees
    float spinRate = 0;       // degrees per frame
    float flipAngle = 0;      // how far he has flipped head over heels, in degrees (+ = backflip)
    float flipRate = 0;
    int takeoffFacing = 1;    // PlayerFacingDir when leaving the ground (1 = right, 2 = left)
    bool grabbing = false;
    bool tucked = false;      // did we set the crouch pose for a grab?
    int grabFrames = 0;
    bool grinding = false;
    bool boardslide = false;  // grinding with the board sideways
    int grindFrames = 0;
    bool manual = false;      // riding on the back wheels
    int manualFrames = 0;
    int comboPoints = 0;      // points collected since the last clean landing
    int comboTricks = 0;      // number of tricks in the combo (the multiplier)
    char label[48] = "";      // name of the latest trick
    char message[48] = "";    // shown after landing
    uint32_t messageColor = 0;
    int messageTimer = 0;
    int total = 0;            // score
    int stun = 0;             // frames left lying on the ground after a bail
};
Skate skate;

// Where the player's sprite is on screen this frame (see beforeRender)
int playerDrawX = 0;
int playerDrawY = 0;
int playerLift = 0;
uint8_t savedSprites[256];      // the sprites as the game set them up (see beforeRender)
uint8_t playerSprites[8][4];    // the player's 8 sprites as they are being drawn
uint8_t shownSpriteY[8];       // where the player's sprites are drawn (after lifting him onto the board)
bool flipping = false;         // is the player being drawn turned over this frame?
unsigned randomSeed = 12345;

// Ramps (custom levels only)
struct Ramp
{
    bool on = false;   // is the player standing on a ramp?
    bool grounded = false; // was he on the ground or a ramp last frame?
    float grade = 0;   // how steep: +1 = 45 degrees rising to the right, -1 = rising to the left
};
Ramp ramp;

int gunReload = 0;             // frames until the gun can fire again
int gunFlash = 0;              // frames of muzzle flash and tracer left
bool gunFired = false;         // was a shot let off this frame?
uint32_t layer[256 * 240];     // a spare picture, for drawing things that then get rotated

ModInput input;                // mouse and extra keys this frame
ModInput previousInput;        // and on the previous frame
bool menuOpen = false;
bool quitChosen = false;
bool levelLoaded = false;
uint8_t savedColors[3];        // the player's real colors while the color mod is drawing

//---------------------------------------------------------------------
// Helpers
//---------------------------------------------------------------------

/** True while a level is being played (not on the title screen, between levels, etc). */
bool isPlaying(SMBEngine& game)
{
    return game.mem(OperMode) == GameModeValue && game.mem(OperMode_Task) == 3;
}

/** True while the player is under normal control (not dying, in a pipe, growing, etc). */
bool playerInControl(SMBEngine& game)
{
    return game.mem(GameEngineSubroutine) == 8 && game.mem(Player_Y_HighPos) == 1;
}

using Draw::setPixel;
using Draw::fillRect;

void drawText(SMBEngine& game, uint32_t* buffer, int x, int y, const char* text, uint32_t color)
{
    Draw::text(game, buffer, x, y, text, color);
}

void drawShadowText(SMBEngine& game, uint32_t* buffer, int x, int y, const char* text, uint32_t color)
{
    Draw::shadowText(game, buffer, x, y, text, color);
}

//---------------------------------------------------------------------
// The menu (Esc or Tab, or click MENU in the corner; then click things)
//---------------------------------------------------------------------

enum MenuAction
{
    ACTION_PLAY_PARK,
    ACTION_PLAY_NORMAL,
    ACTION_EDITOR,
    ACTION_RESUME,
    ACTION_QUIT,
    ACTION_COUNT
};

const char* const actionNames[ACTION_COUNT] = {
    "PLAY SKATE PARK",
    "PLAY NORMAL GAME",
    "LEVEL EDITOR",
    "RESUME",
    "QUIT",
};

const int MENU_WIDTH = 168;
const int MENU_ROW_HEIGHT = 12;
const int MENU_HEIGHT = 24 + MOD_COUNT * MENU_ROW_HEIGHT + 8 + ACTION_COUNT * MENU_ROW_HEIGHT + 8;
const int MENU_LEFT = (256 - MENU_WIDTH) / 2;
const int MENU_TOP = (240 - MENU_HEIGHT) / 2;

// The little MENU button shown in the top right corner while playing
const int MENU_BUTTON_X = 218;
const int MENU_BUTTON_Y = 3;
const int MENU_BUTTON_WIDTH = 36;
const int MENU_BUTTON_HEIGHT = 11;

/** Top of a menu row. Rows 0 to MOD_COUNT-1 are the mods, the rest are the actions. */
int menuRowY(int row)
{
    int y = MENU_TOP + 24 + row * MENU_ROW_HEIGHT;
    return (row >= MOD_COUNT) ? y + 8 : y;
}

/** Which menu row is the mouse on? -1 if none. */
int menuRowAtMouse()
{
    if (input.mouseX < MENU_LEFT + 4 || input.mouseX >= MENU_LEFT + MENU_WIDTH - 4)
    {
        return -1;
    }
    for (int row = 0; row < MOD_COUNT + ACTION_COUNT; row++)
    {
        int y = menuRowY(row);
        if (input.mouseY >= y - 2 && input.mouseY < y - 2 + MENU_ROW_HEIGHT)
        {
            return row;
        }
    }
    return -1;
}

void updateMenu(SMBEngine& game, bool click)
{
    int row = menuRowAtMouse();
    if (!click || row < 0)
    {
        return;
    }
    if (row < MOD_COUNT)
    {
        mods[row].on = !mods[row].on;
        return;
    }
    switch (row - MOD_COUNT)
    {
    case ACTION_PLAY_PARK:
        mods[MOD_SKATE].on = true;
        Level::restart(game, true);
        menuOpen = false;
        break;
    case ACTION_PLAY_NORMAL:
        Level::restart(game, false);
        menuOpen = false;
        break;
    case ACTION_EDITOR:
        Editor::open(game);
        menuOpen = false;
        break;
    case ACTION_RESUME:
        menuOpen = false;
        break;
    case ACTION_QUIT:
        quitChosen = true;
        break;
    }
}

void drawMenuButton(SMBEngine& game, uint32_t* buffer)
{
    bool hover = input.mouseX >= MENU_BUTTON_X && input.mouseX < MENU_BUTTON_X + MENU_BUTTON_WIDTH &&
                 input.mouseY >= MENU_BUTTON_Y && input.mouseY < MENU_BUTTON_Y + MENU_BUTTON_HEIGHT;
    fillRect(buffer, MENU_BUTTON_X, MENU_BUTTON_Y, MENU_BUTTON_WIDTH, MENU_BUTTON_HEIGHT, hover ? 0xff505050 : 0xff000000);
    Draw::frame(buffer, MENU_BUTTON_X, MENU_BUTTON_Y, MENU_BUTTON_WIDTH, MENU_BUTTON_HEIGHT, 0xfff8f8f8);
    drawText(game, buffer, MENU_BUTTON_X + 2, MENU_BUTTON_Y + 2, "MENU", 0xfff8f8f8);
}

void drawMenu(SMBEngine& game, uint32_t* buffer)
{
    const uint32_t white = 0xfff8f8f8;
    const uint32_t gray = 0xff909090;
    const uint32_t green = 0xff58d854;
    const uint32_t yellow = 0xfff8b800;

    // darken the game behind the menu
    for (int i = 0; i < 256 * 240; i++)
    {
        buffer[i] = 0xff000000 | ((buffer[i] >> 1) & 0x7f7f7f);
    }

    fillRect(buffer, MENU_LEFT, MENU_TOP, MENU_WIDTH, MENU_HEIGHT, white);
    fillRect(buffer, MENU_LEFT + 2, MENU_TOP + 2, MENU_WIDTH - 4, MENU_HEIGHT - 4, 0xff000000);
    drawText(game, buffer, MENU_LEFT + MENU_WIDTH / 2 - 16, MENU_TOP + 8, "MENU", yellow);

    int hover = menuRowAtMouse();
    for (int row = 0; row < MOD_COUNT + ACTION_COUNT; row++)
    {
        int y = menuRowY(row);
        if (row == hover)
        {
            fillRect(buffer, MENU_LEFT + 4, y - 2, MENU_WIDTH - 8, MENU_ROW_HEIGHT, 0xff404040);
        }
        if (row < MOD_COUNT)
        {
            drawText(game, buffer, MENU_LEFT + 16, y, mods[row].name, white);
            drawText(game, buffer, MENU_LEFT + 120, y, mods[row].on ? "ON" : "OFF", mods[row].on ? green : gray);
        }
        else
        {
            drawText(game, buffer, MENU_LEFT + 16, y, actionNames[row - MOD_COUNT], row == hover ? yellow : white);
        }
    }
}

//---------------------------------------------------------------------
// The gun
//---------------------------------------------------------------------

void drawGun(SMBEngine& game, uint32_t* buffer)
{
    if (!isPlaying(game) || !playerInControl(game))
    {
        return;
    }

    bool small = game.mem(PlayerSize) != 0 || game.mem(CrouchingFlag) != 0;
    bool facingLeft = game.mem(PlayerFacingDir) == 2;
    // the player is 16 wide; the gun starts just behind his back and points forward
    int x = facingLeft ? playerDrawX + 16 - GUN_X - GUN_WIDTH : playerDrawX + GUN_X;
    int y = playerDrawY - playerLift + (small ? GUN_Y_SMALL : GUN_Y_BIG);

    for (int row = 0; row < GUN_HEIGHT; row++)
    {
        for (int column = 0; column < GUN_WIDTH; column++)
        {
            uint32_t color = gunColor(gunPicture[row][facingLeft ? GUN_WIDTH - 1 - column : column]);
            if (color != 0)
            {
                setPixel(buffer, x + column, y + row, color);
            }
        }
    }

    if (gunFlash > 0)
    {
        // muzzle flash, and a tracer along the bullet's path
        int direction = facingLeft ? -1 : 1;
        int muzzleX = facingLeft ? x - 1 : x + GUN_WIDTH;
        int muzzleY = y + GUN_BARREL_ROW;
        for (int i = 0; i < 4; i++)
        {
            fillRect(buffer, muzzleX + direction * i - (facingLeft ? 0 : 0), muzzleY - (3 - i), 1, 2 * (3 - i) + 1, i < 2 ? 0xfff8f8f8 : 0xfff8b800);
        }
        for (int i = 6; i < 256; i += 2)
        {
            if ((i / 2 + gunFlash) % 4 != 0)
            {
                setPixel(buffer, muzzleX + direction * i, muzzleY, 0xfff8f8a0);
            }
        }
    }
}

//---------------------------------------------------------------------
// Ramps
//
// The original game has no slopes, so the game itself treats a ramp block as empty
// air. Everything a ramp does is done here: every frame, if the player's feet have
// reached a ramp's surface he is put on it and told he is standing on the ground.
//---------------------------------------------------------------------

/**
 * Look for a ramp surface near the player's feet at one position across his width.
 * Positions use the game's numbers: worldX in pixels from the start of the level,
 * feetY as the game stores vertical positions (the top of the first row on screen is 32).
 */
bool rampSurfaceAt(int worldX, int feetY, int& surfaceY, float& grade)
{
    int column = worldX >> 4;
    int across = worldX & 15;
    int topRow = Level::cameraRow(); // the level's row that is at the top of the screen
    int feetRow = ((feetY - 32) >> 4) + topRow;
    bool found = false;
    for (int row = feetRow - 1; row <= feetRow + 1; row++)
    {
        int left, right;
        if (Level::slopeAt(column, row, left, right))
        {
            int height = left + (right - left) * across / 16;
            int y = 32 + 16 * (row - topRow + 1) - height;
            if (!found || y < surfaceY)
            {
                surfaceY = y;
                grade = (right - left) / 16.0f;
                found = true;
            }
        }
    }
    return found;
}

void updateRamps(SMBEngine& game)
{
    if (!Level::active || !playerInControl(game))
    {
        ramp.on = false;
        ramp.grounded = false;
        return;
    }

    int worldX = game.mem(Player_PageLoc) * 256 + game.mem(Player_X_Position);
    int feetY = game.mem(Player_Y_Position) + 32;
    int verticalSpeed = (int8_t)game.mem(Player_Y_Speed);
    uint8_t state = game.mem(Player_State); // 0 = on the ground, 1 = jumping, 2 = falling

    // Take the highest surface under his two feet and his middle
    int surfaceY = 0;
    float grade = 0;
    bool found = false;
    const int feet[3] = {3, 8, 12};
    for (int i = 0; i < 3; i++)
    {
        int y;
        float g;
        if (rampSurfaceAt(worldX + feet[i], feetY, y, g) && (!found || y < surfaceY))
        {
            surfaceY = y;
            grade = g;
            found = true;
        }
    }

    bool rising = state != 0 && verticalSpeed < 0; // jumping, or just launched
    if (found && !rising)
    {
        int below = feetY - surfaceY; // how far his feet are under the surface
        // On the ground or a ramp already: follow the surface down as well as up.
        // In the air: only once he has come down to it.
        int reach = (ramp.on || ramp.grounded) ? 8 : 0;
        // (If the game has him standing on solid blocks that are higher than the ramp,
        // such as the deck at the top of it, leave him there.)
        bool onHigherGround = state == 0 && below < 0;
        if (below >= -reach && below <= 12 && !onHigherGround)
        {
            game.mem(Player_Y_Position) = (uint8_t)(surfaceY - 32);
            game.mem(Player_Y_Speed) = 0;
            game.mem(SprObject_Y_MoveForce) = 0;
            game.mem(Player_State) = 0;
            ramp.on = true;
            ramp.grounded = true;
            ramp.grade = grade;
            return;
        }
    }

    if (ramp.on && state != 0 && !rising)
    {
        // He has just ridden off the end of a ramp into the air. If he was going up it,
        // he keeps going up: that is what makes a ramp a jump.
        float climb = (int8_t)game.mem(Player_X_Speed) / 16.0f * ramp.grade; // pixels per frame
        if (climb > 0.5f)
        {
            int launch = (int)(climb * SLOPE_LAUNCH + 0.5f);
            if (launch > 7) launch = 7;
            game.mem(Player_Y_Speed) = (uint8_t)(int8_t)(-launch);
            game.mem(SprObject_Y_MoveForce) = 0;
            game.mem(VerticalForceDown) = SLOPE_LAUNCH_GRAVITY; // the same air every time
            game.mem(VerticalForce) = SLOPE_LAUNCH_GRAVITY;
        }
    }
    ramp.on = false;
    ramp.grounded = state == 0;
}

//---------------------------------------------------------------------
// Skateboard
//
// Loosely modelled on how Skate 3 works, flattened to 2D:
//  - pushing adds a lot of speed when slow and little when fast, up to a top speed,
//    and the board keeps rolling when you stop pushing
//  - an ollie is a crouch followed by a pop: the longer crouch gives the bigger pop
//  - in the air you can barely steer; the arrow keys spin you instead, and the spin
//    has momentum
//  - what matters on landing is which way the board points: straight (or backwards)
//    is clean, a bit off is sketchy and costs speed, sideways is a bail. Letting go of
//    the spin when nearly straight lines the board up by itself
//  - landing on an edge starts a grind; landing on it turned (still spinning as you
//    touch down) makes it a boardslide
//  - grinding speeds you up, past the speed you can reach by pushing
//  - a manual scores while it lasts and links tricks: land in one and the combo carries on
//  - a grab only counts if you let go before you land
//  - tricks add up in a combo that is banked on a clean landing and lost on a bail
//---------------------------------------------------------------------

/** Can the player skate right now? (not in water levels, not while dying, in a pipe, etc) */
bool skateUsable(SMBEngine& game)
{
    return mods[MOD_SKATE].on && isPlaying(game) && playerInControl(game) && game.mem(AreaType) != 0;
}

/**
 * The kind of block under the player's feet (a "metatile" number, see docs/smbdis.asm).
 * offset is how far right of the player's left edge to look (the player is 16 wide).
 */
uint8_t blockUnderPlayer(SMBEngine& game, int offset)
{
    int x = game.mem(Player_X_Position) + offset;
    int page = game.mem(Player_PageLoc) + (x >> 8);
    int y = game.mem(Player_Y_Position) + 32; // just below the feet
    if (y < 32 || y >= 240)
    {
        return 0;
    }
    // The game keeps the blocks of two screens in memory, 16 columns by 13 rows each
    int buffer = (page & 1) ? Block_Buffer_2 : Block_Buffer_1;
    return game.mem(buffer + ((y & 0xf0) - 32) + ((x & 0xff) >> 4));
}

/** Things you can grind on. */
bool isGrindable(uint8_t block)
{
    return (block >= 0x10 && block <= 0x13)    // top of a pipe
        || (block >= 0x1c && block <= 0x1e)    // top of a sideways pipe
        || (block >= 0x51 && block <= 0x53)    // bricks
        || (block >= 0x55 && block <= 0x5e)    // bricks with something inside
        || block == 0x63                       // bridge
        || block == 0x64                       // top of a bullet bill cannon
        || block == 0xc0 || block == 0xc1      // question blocks
        || block == 0xc4;                      // used block
}

/** Is the player standing on something grindable? (checks under both feet and the middle) */
bool onGrindable(SMBEngine& game)
{
    return isGrindable(blockUnderPlayer(game, 3)) || isGrindable(blockUnderPlayer(game, 8)) || isGrindable(blockUnderPlayer(game, 12));
}

void skateSetSpeed(SMBEngine& game, float speed)
{
    skate.speed = speed;
    skate.lastWritten = (int)speed; // drops the fraction
    game.mem(Player_X_Speed) = (uint8_t)(int8_t)skate.lastWritten;
    game.mem(Player_X_MoveForce) = 0;
}

void skateMessage(const char* text, int points, uint32_t color)
{
    if (points > 0)
    {
        snprintf(skate.message, sizeof(skate.message), "%s %d", text, points);
    }
    else
    {
        snprintf(skate.message, sizeof(skate.message), "%s", text);
    }
    skate.messageColor = color;
    skate.messageTimer = 120;
}

void skateAddTrick(const char* name, int points)
{
    snprintf(skate.label, sizeof(skate.label), "%s", name);
    skate.comboPoints += points;
    skate.comboTricks++;
}

/** A clean landing: the combo's points are multiplied by the number of tricks and kept. */
void skateBankCombo(bool sketchy)
{
    if (skate.comboTricks == 0)
    {
        return;
    }
    int points = skate.comboPoints * skate.comboTricks;
    if (sketchy)
    {
        points /= 2;
    }
    skate.total += points;
    char text[48];
    snprintf(text, sizeof(text), "%s%s", sketchy ? "SKETCHY " : "", skate.label);
    skateMessage(text, points, sketchy ? 0xfff8b800 : 0xff58d854);
    skate.comboPoints = 0;
    skate.comboTricks = 0;
    skate.label[0] = 0;
}

void skateBail(SMBEngine& game)
{
    skate.comboPoints = 0;
    skate.comboTricks = 0;
    skate.label[0] = 0;
    skate.grinding = false;
    skate.manual = false;
    skate.stun = BAIL_FRAMES;
    skateSetSpeed(game, 0);
    skateMessage("BAIL!", 0, 0xfff83800);
}

/** Name and score what was done in the air. */
void skateScoreAir(SMBEngine& game)
{
    char name[48] = "";
    int points = 0;
    int halfTurns = (int)(fabsf(skate.angle) / 180 + 0.5f);
    if (halfTurns >= 1)
    {
        // "frontside" here = spinning the way you are travelling
        bool movingRight = skate.speed >= 0;
        bool spinningRight = skate.angle > 0;
        snprintf(name, sizeof(name), "%s %d", movingRight == spinningRight ? "FS" : "BS", halfTurns * 180);
        points += 100 * halfTurns * (halfTurns + 1) / 2; // 180 = 100, 360 = 300, 540 = 600...
    }
    int flips = (int)(fabsf(skate.flipAngle) / 360 + 0.5f);
    if (flips >= 1)
    {
        if (name[0]) strncat(name, " ", sizeof(name) - strlen(name) - 1);
        if (flips >= 2) strncat(name, "DOUBLE ", sizeof(name) - strlen(name) - 1);
        strncat(name, skate.flipAngle > 0 ? "BACKFLIP" : "FRONTFLIP", sizeof(name) - strlen(name) - 1);
        points += 500 * flips * (flips + 1) / 2; // one = 500, two = 1500
    }
    if (skate.grabFrames >= 8)
    {
        strncat(name, name[0] ? " GRAB" : "GRAB", sizeof(name) - strlen(name) - 1);
        points += 50 + 2 * skate.grabFrames;
    }
    if (points == 0 && skate.jumped)
    {
        snprintf(name, sizeof(name), "%s", skate.bigOllie ? "BIG OLLIE" : "OLLIE");
        points = skate.bigOllie ? 100 : 50;
    }
    if (points > 0)
    {
        skateAddTrick(name, points);
    }
}

void skateEndGrind()
{
    if (skate.grinding && skate.grindFrames >= 3)
    {
        skateAddTrick(skate.boardslide ? "BOARDSLIDE" : "50-50", 50 + 3 * skate.grindFrames);
    }
    skate.grinding = false;
}

void skateEndManual()
{
    if (skate.manual && skate.manualFrames >= 10)
    {
        skateAddTrick("MANUAL", 20 + 2 * skate.manualFrames);
    }
    skate.manual = false;
}

void skateTakeoff(SMBEngine& game)
{
    skateEndGrind(); // ollie out of a grind, or rolled off the end: the combo carries on
    skateEndManual(); // same for a manual
    skate.angle = 0;
    skate.spinRate = 0;
    skate.flipAngle = 0;
    skate.flipRate = 0;
    skate.grabFrames = 0;
    skate.grabbing = false;
    skate.takeoffFacing = game.mem(PlayerFacingDir);
    skate.jumped = game.mem(Player_State) == 1 && (game.mem(Player_Y_Speed) & 0x80) != 0;
    skate.bigOllie = skate.jumped && skate.crouchCharge >= SKATE_CHARGE_FRAMES;
    if (skate.bigOllie)
    {
        game.mem(Player_Y_Speed) = SKATE_BIG_OLLIE_SPEED;
    }
    skate.crouchCharge = 0;
}

void skateInAir(SMBEngine& game, Controller& controller)
{
    // Spin: the arrow keys speed the spin up, and it slowly dies away when let go
    int spin = (input.spinRight ? 1 : 0) - (input.spinLeft ? 1 : 0);
    if (spin != 0)
    {
        skate.spinRate += spin * SPIN_ACCELERATION;
        if (skate.spinRate > SPIN_MAX) skate.spinRate = SPIN_MAX;
        if (skate.spinRate < -SPIN_MAX) skate.spinRate = -SPIN_MAX;
    }
    else
    {
        skate.spinRate *= SPIN_DAMPING;
    }
    skate.angle += skate.spinRate;
    if (spin == 0)
    {
        // Let go close to straight (or backwards): the skater lines the board up for landing
        float nearest = 180 * floorf(skate.angle / 180 + 0.5f);
        if (fabsf(nearest - skate.angle) < SPIN_STRAIGHTEN_RANGE)
        {
            skate.angle += (nearest - skate.angle) * SPIN_STRAIGHTEN;
        }
    }

    // Flips work the same way as spins, head over heels (see Mods::afterRender for the drawing)
    int flip = (input.flipBack ? 1 : 0) - (input.flipFront ? 1 : 0);
    if (flip != 0)
    {
        skate.flipRate += flip * FLIP_ACCELERATION;
        if (skate.flipRate > FLIP_MAX) skate.flipRate = FLIP_MAX;
        if (skate.flipRate < -FLIP_MAX) skate.flipRate = -FLIP_MAX;
    }
    else
    {
        skate.flipRate *= FLIP_DAMPING;
    }
    skate.flipAngle += skate.flipRate;
    if (flip == 0)
    {
        float nearest = 360 * floorf(skate.flipAngle / 360 + 0.5f);
        if (fabsf(nearest - skate.flipAngle) < FLIP_STRAIGHTEN_RANGE)
        {
            skate.flipAngle += (nearest - skate.flipAngle) * SPIN_STRAIGHTEN;
        }
    }

    // Show the spin by turning mario around whenever his back is towards us
    bool turnedAway = cosf(skate.angle * 3.14159265f / 180) < 0;
    int opposite = (skate.takeoffFacing == 1) ? 2 : 1;
    game.mem(PlayerFacingDir) = turnedAway ? opposite : skate.takeoffFacing;

    // Grab: down in the air. Big mario tucks (the game's crouching pose).
    skate.grabbing = controller.getButtonState(BUTTON_DOWN) || input.grab;

    if (skate.grabbing)
    {
        skate.grabFrames++;
        if (game.mem(PlayerSize) == 0 && game.mem(CrouchingFlag) == 0)
        {
            game.mem(CrouchingFlag) = 4;
            skate.tucked = true;
        }
    }
    else if (skate.tucked)
    {
        game.mem(CrouchingFlag) = 0;
        skate.tucked = false;
    }
}

void skateLand(SMBEngine& game)
{
    // How far from straight is the board? 0 = straight (or exactly backwards), 90 = sideways
    float turned = fabsf(skate.angle);
    int halfTurns = (int)(turned / 180 + 0.5f);
    float offAxis = fabsf(turned - halfTurns * 180);
    // And how far from upright is he? 0 = upright, 180 = on his head
    float offUpright = fabsf(skate.flipAngle - 360 * floorf(skate.flipAngle / 360 + 0.5f));

    // Face the way we are rolling again
    if (skate.speed != 0)
    {
        game.mem(PlayerFacingDir) = (skate.speed > 0) ? 1 : 2;
    }
    else
    {
        game.mem(PlayerFacingDir) = skate.takeoffFacing;
    }
    if (skate.tucked)
    {
        game.mem(CrouchingFlag) = 0;
        skate.tucked = false;
    }

    bool onEdge = onGrindable(game) && fabsf(skate.speed) >= SKATE_GRIND_MIN_SPEED;
    if (skate.grabbing || offUpright > FLIP_SKETCHY)
    {
        skateBail(game); // still holding the board, or not the right way up
    }
    else if (onEdge)
    {
        skateScoreAir(game);
        skate.grinding = true;
        skate.boardslide = offAxis > LANDING_CLEAN; // came in turned: slide on the middle of the board
        skate.grindFrames = 0;
    }
    else if (offAxis > LANDING_SKETCHY)
    {
        skateBail(game); // landed sideways
    }
    else
    {
        skateScoreAir(game);
        bool sketchy = offAxis > LANDING_CLEAN || offUpright > FLIP_CLEAN;
        if (sketchy)
        {
            skateSetSpeed(game, skate.speed * 0.5f);
        }
        if (sketchy || !input.manual)
        {
            skateBankCombo(sketchy);
        } // otherwise the landing goes straight into a manual and the combo carries on
    }
    skate.angle = 0;
    skate.spinRate = 0;
    skate.flipAngle = 0;
    skate.flipRate = 0;
    skate.grabbing = false;
}

/** Called once per frame from Mods::update. */
void updateSkate(SMBEngine& game, Controller& controller)
{
    if (skate.messageTimer > 0)
    {
        skate.messageTimer--;
    }
    if (!skateUsable(game))
    {
        skate.wasInAir = false;
        skate.grinding = false;
        skate.manual = false;
        return;
    }
    if (skate.stun > 0)
    {
        skate.stun--;
    }

    uint8_t state = game.mem(Player_State); // 0 = on the ground, 1 = jumping, 2 = falling, 3 = climbing
    bool inAir = (state == 1 || state == 2);
    if (inAir)
    {
        if (!skate.wasInAir)
        {
            skateTakeoff(game);
        }
        skateInAir(game, controller);
    }
    else
    {
        if (skate.wasInAir)
        {
            skateLand(game);
        }

        // Crouching winds up the ollie; it unwinds quickly once you stand up
        if (controller.getButtonState(BUTTON_DOWN))
        {
            skate.crouchCharge++;
        }
        else if (skate.crouchCharge > 0)
        {
            skate.crouchCharge = (skate.crouchCharge > SKATE_CHARGE_FRAMES + 8) ? SKATE_CHARGE_FRAMES + 8 : skate.crouchCharge;
            skate.crouchCharge -= 2;
            if (skate.crouchCharge < 0) skate.crouchCharge = 0;
        }

        if (skate.grinding)
        {
            skate.grindFrames++;
            if (!onGrindable(game) || fabsf(skate.speed) < 2)
            {
                skateEndGrind(); // slid off the end onto flat ground
                if (!input.manual)
                {
                    skateBankCombo(false);
                }
            }
        }

        // Manual: up arrow while rolling
        bool wantManual = input.manual && !skate.grinding && skate.stun == 0 && fabsf(skate.speed) >= SKATE_MANUAL_MIN_SPEED;
        if (wantManual)
        {
            if (!skate.manual)
            {
                skate.manual = true;
                skate.manualFrames = 0;
            }
            skate.manualFrames++;
        }
        else if (skate.manual)
        {
            skateEndManual(); // put the front wheels down: the combo is banked
            skateBankCombo(false);
        }
        else if (!skate.grinding && skate.comboTricks > 0)
        {
            skateBankCombo(false); // a combo that was being carried, but no manual followed
        }

        // No pushing going on: hold mario's legs still instead of the running animation
        bool pushing = game.mem(Left_Right_Buttons) != 0;
        if (!pushing || skate.grinding || skate.manual)
        {
            game.mem(PlayerAnimTimer) = 2;
        }
    }
    skate.wasInAir = inAir;
}

void drawBoard(SMBEngine& game, uint32_t* buffer)
{
    const uint32_t deck = 0xff8a4a1a;
    const uint32_t grip = 0xff202020;
    const uint32_t wheel = 0xfff8f8f8;

    uint8_t state = game.mem(Player_State);
    bool inAir = (state == 1 || state == 2);
    bool facingRight = game.mem(PlayerFacingDir) != 2;
    int direction = facingRight ? 1 : -1;

    int centerX = playerDrawX + 8;
    int bottom = playerDrawY + 32; // the lowest row of pixels above the ground
    if (inAir && skate.grabbing)
    {
        bottom -= 2; // board pulled up into the hand
    }

    // Spinning shows as the board getting shorter (seen end-on at 90 degrees)
    float length = fabsf(cosf(skate.angle * 3.14159265f / 180));
    if (skate.grinding && skate.boardslide)
    {
        length = 0.3f;
    }
    int half = (int)(8 * length + 0.5f);
    if (half < 2) half = 2;

    // Nose up while rising, nose down while falling
    float tilt = 0;
    if (inAir && !skate.grabbing)
    {
        tilt = -(float)(int8_t)game.mem(Player_Y_Speed) * 0.8f;
        if (tilt > 4) tilt = 4;
        if (tilt < -3) tilt = -3;
    }

    if (!inAir && ramp.on)
    {
        tilt = ramp.grade * direction * 5; // the board lies along the ramp
    }

    // In a manual, and in a grind along the edge, the board rides on its back wheels
    bool noseUp = !inAir && (skate.manual || (skate.grinding && !skate.boardslide));

    for (int i = -half; i <= half; i++)
    {
        int y = bottom - 2 - (int)lroundf(tilt * (i * direction) / half);
        if (noseUp)
        {
            // the tail stays down and the nose comes up
            y = bottom - 2 - (int)lroundf((float)MANUAL_LIFT * (i * direction + half) / (2 * half));
        }
        setPixel(buffer, centerX + i, y - 1, grip);
        setPixel(buffer, centerX + i, y, deck);
        if (i == -half || i == half)
        {
            setPixel(buffer, centerX + i, y - 2, grip); // kicked-up nose and tail
        }
        bool isWheel = (half >= 5) ? (i == -half + 2 || i == -half + 3 || i == half - 2 || i == half - 3) : (i == -1 || i == 0 || i == 1);
        if (isWheel)
        {
            setPixel(buffer, centerX + i, y + 1, wheel);
            setPixel(buffer, centerX + i, y + 2, wheel);
        }
    }

    if (inAir && skate.grabbing)
    {
        // a hand on the board
        fillRect(buffer, centerX + direction * 3 - 1, bottom - 6, 3, 3, 0xfffca044);
    }

    if (skate.grinding)
    {
        // sparks flying out behind the board
        const uint32_t sparkColors[4] = {0xfff8f8f8, 0xfff8d878, 0xfff8b800, 0xfff83800};
        for (int i = 0; i < 10; i++)
        {
            randomSeed = randomSeed * 1103515245u + 12345u;
            int back = (int)((randomSeed >> 16) % 14);
            int x = centerX - direction * (2 + back);
            int y = bottom + 1 - (int)((randomSeed >> 22) % (2 + back / 2));
            setPixel(buffer, x, y, sparkColors[(randomSeed >> 28) & 3]);
        }
    }
}

void drawSkateText(SMBEngine& game, uint32_t* buffer)
{
    const uint32_t white = 0xfff8f8f8;
    char text[64];

    snprintf(text, sizeof(text), "SK8 %06d", skate.total);
    drawShadowText(game, buffer, 24, 34, text, white);

    if (skate.comboTricks > 0 || skate.grinding || skate.manual)
    {
        // the combo so far, including the grind or manual still going on
        const char* name = skate.label;
        int points = skate.comboPoints;
        int tricks = skate.comboTricks;
        if (skate.grinding)
        {
            name = skate.boardslide ? "BOARDSLIDE" : "50-50";
            points += 50 + 3 * skate.grindFrames;
            tricks++;
        }
        else if (skate.manual)
        {
            name = "MANUAL";
            points += 20 + 2 * skate.manualFrames;
            tricks++;
        }
        snprintf(text, sizeof(text), "%s %d X %d", name, points, tricks);
        drawShadowText(game, buffer, 128 - (int)strlen(text) * 4, 48, text, white);
    }
    else if (skate.messageTimer > 0)
    {
        drawShadowText(game, buffer, 128 - (int)strlen(skate.message) * 4, 48, skate.message, skate.messageColor);
    }
}

} // end of private section

//---------------------------------------------------------------------
// Called by the rest of the program
//---------------------------------------------------------------------

void Mods::setInput(const ModInput& newInput)
{
    input = newInput;
}

bool Mods::frozen()
{
    return menuOpen || Editor::isOpen();
}

bool Mods::quitRequested()
{
    return quitChosen;
}

void Mods::update(SMBEngine& game)
{
    if (!levelLoaded)
    {
        Level::load(LEVEL_FILE);
        levelLoaded = true;
    }

    bool click = input.mouseLeft && !previousInput.mouseLeft;
    bool menuKey = input.menuKey && !previousInput.menuKey;
    previousInput = input;

    if (Editor::isOpen())
    {
        if (menuKey)
        {
            Editor::close();
        }
        else if (Editor::update(game, input, click))
        {
            Level::restart(game, true); // PLAY was clicked
        }
        return;
    }
    if (menuKey)
    {
        menuOpen = !menuOpen;
    }
    else if (menuOpen)
    {
        updateMenu(game, click);
    }
    else if (click && input.mouseX >= MENU_BUTTON_X && input.mouseX < MENU_BUTTON_X + MENU_BUTTON_WIDTH &&
             input.mouseY >= MENU_BUTTON_Y && input.mouseY < MENU_BUTTON_Y + MENU_BUTTON_HEIGHT)
    {
        menuOpen = true;
    }
    if (frozen())
    {
        return;
    }

    Controller& controller = game.getController1();
    if (!isPlaying(game))
    {
        return;
    }

    Level::update(game); // the custom level's camera, before anything looks at where the player is
    updateRamps(game); // before the skateboard, which needs to know whether he is on the ground

    // SKATE
    if (mods[MOD_SKATE].on)
    {
        updateSkate(game, controller);
    }

    // GUN: reloading
    if (gunReload > 0) gunReload--;
    if (gunFlash > 0) gunFlash--;

    // BIG: whenever the player is small, make him big
    if (mods[MOD_BIG].on && playerInControl(game) && game.mem(PlayerStatus) == 0)
    {
        game.mem(PlayerStatus) = 1; // 0 = small, 1 = big, 2 = fiery
        game.mem(PlayerSize) = 0;   // 0 = big, 1 = small
    }

    // FLY: holding A makes the player rise
    if (mods[MOD_FLY].on && playerInControl(game) &&
        controller.getButtonState(BUTTON_A) &&
        game.mem(Player_Y_Position) >= FLY_CEILING)
    {
        game.mem(Player_Y_Speed) = FLY_RISE_SPEED;
    }

    // GUN: fireballs become bullets that fly straight and fast
    // (shooting without a fire flower is handled by gunEnabled() below)
    if (mods[MOD_GUN].on)
    {
        for (int i = 0; i < 2; i++)
        {
            if (game.mem(Fireball_State + i) == 1) // in flight?
            {
                game.mem(Fireball_Y_Speed + i) = 0; // cancel gravity
                game.mem(SprObject_Y_MoveForce + 7 + i) = 0;
                bool movingLeft = (game.mem(Fireball_X_Speed + i) & 0x80) != 0;
                game.mem(Fireball_X_Speed + i) = movingLeft ? (uint8_t)(0x100 - BULLET_SPEED) : BULLET_SPEED;
            }
        }
    }
}

void Mods::beforeRender(SMBEngine& game)
{
    // Where is the player on screen? The sprites being shown were set up at the start of
    // the frame, so read their position rather than the game's newer numbers. The player
    // is made of 8 sprites; the first is his top left corner.
    // A custom level's camera can be part-way between two rows: everything is then drawn
    // that many pixels higher (except sprite 0, which is part of the score display)
    uint8_t* allSprites = game.getPPU().getOAM();
    memcpy(savedSprites, allSprites, 256);
    int cameraOffset = (Level::active && isPlaying(game)) ? Level::pictureOffset() : 0;
    for (int i = 1; i < 64 && cameraOffset != 0; i++)
    {
        uint8_t y = allSprites[i * 4];
        if (y < 0xef)
        {
            allSprites[i * 4] = (y >= cameraOffset) ? y - cameraOffset : 0xf8;
        }
    }

    uint8_t* sprites = game.getPPU().getOAM() + game.mem(Player_SprDataOffset);
    playerDrawX = sprites[3];
    playerDrawY = sprites[0];

    // SKATE: draw mario a few pixels higher so that he stands on the board
    playerLift = skateUsable(game) ? BOARD_LIFT : 0;
    // In a manual or a grind along the edge he leans back over the tail: his head and
    // body are drawn a little behind his feet, and he rides a bit higher
    bool leaning = playerLift != 0 && game.mem(Player_State) == 0 && (skate.manual || (skate.grinding && !skate.boardslide));
    int back = (game.mem(PlayerFacingDir) == 2) ? 1 : -1;
    const int leanByRow[4] = {3, 2, 1, 0}; // head, body, legs, feet
    if (leaning)
    {
        playerLift += 2;
    }
    // In a flip the whole player is drawn turned, which the game can't do: his sprites
    // are hidden here and drawn by afterRender instead
    flipping = playerLift != 0 && fabsf(skate.flipAngle) > 1;
    for (int i = 0; i < 8; i++)
    {
        if (playerLift != 0 && sprites[i * 4] < 0xef)
        {
            sprites[i * 4] -= playerLift;
            if (leaning)
            {
                sprites[i * 4 + 3] += back * leanByRow[i / 2];
            }
        }
        shownSpriteY[i] = sprites[i * 4];
        memcpy(playerSprites[i], sprites + i * 4, 4);
        if (flipping)
        {
            sprites[i * 4] = 0xf8; // off the bottom of the screen
        }
    }

    // COLOR: the player's three colors cycle through the rainbow.
    // Colors here are NES color numbers: low digit = hue (1-c), high digit = brightness (0-3).
    uint8_t* playerColors = game.getPPU().getPalette() + 0x11;
    for (int i = 0; i < 3; i++)
    {
        savedColors[i] = playerColors[i];
    }
    if (mods[MOD_COLOR].on)
    {
        int step = game.mem(FrameCounter) / 8; // changes every 8 frames
        playerColors[0] = 0x10 + (step % 12) + 1;       // main color
        playerColors[2] = 0x20 + ((step + 6) % 12) + 1; // second color, opposite hue and brighter
        // playerColors[1] is the skin, left as it is
    }
}

void Mods::afterRender(SMBEngine& game, uint32_t* buffer)
{
    // put the player's real colors back
    uint8_t* playerColors = game.getPPU().getPalette() + 0x11;
    for (int i = 0; i < 3; i++)
    {
        playerColors[i] = savedColors[i];
    }

    memcpy(game.getPPU().getOAM(), savedSprites, 256); // put the sprites back as the game had them

    if (flipping)
    {
        // Draw the player, his board and his gun on the spare picture, then copy that
        // onto the real one turned about his middle
        memset(layer, 0, sizeof(layer));
        for (int i = 0; i < 8; i++)
        {
            if (shownSpriteY[i] < 0xef)
            {
                uint8_t attributes = playerSprites[i][2];
                Draw::spriteTile(game, layer, playerSprites[i][3], shownSpriteY[i] + 1, playerSprites[i][1], attributes & 3, (attributes & 0x40) != 0);
            }
        }
        drawBoard(game, layer);
        if (mods[MOD_GUN].on)
        {
            drawGun(game, layer);
        }

        bool small = game.mem(PlayerSize) != 0;
        int centerX = playerDrawX + 8;
        int centerY = playerDrawY + 1 - playerLift + (small ? 24 : 18);
        // a backflip turns him backwards over his head: which way that is on screen
        // depends on which way he was facing when he took off
        float radians = skate.flipAngle * 3.14159265f / 180 * (skate.takeoffFacing == 2 ? 1 : -1);
        float c = cosf(radians);
        float sn = sinf(radians);
        for (int dy = -40; dy <= 40; dy++)
        {
            for (int dx = -40; dx <= 40; dx++)
            {
                // which pixel of the unturned picture ends up here?
                int sourceX = centerX + (int)lroundf(dx * c + dy * sn);
                int sourceY = centerY + (int)lroundf(-dx * sn + dy * c);
                if (sourceX >= 0 && sourceX < 256 && sourceY >= 0 && sourceY < 240 && layer[sourceY * 256 + sourceX] != 0)
                {
                    setPixel(buffer, centerX + dx, centerY + dy, layer[sourceY * 256 + sourceX]);
                }
            }
        }
        if (mods[MOD_SKATE].on)
        {
            drawSkateText(game, buffer);
        }
    }
    else
    {
        if (mods[MOD_SKATE].on)
        {
            if (skateUsable(game))
            {
                drawBoard(game, buffer);
            }
            drawSkateText(game, buffer);
        }

        if (mods[MOD_GUN].on)
        {
            drawGun(game, buffer);
        }
    }

    if (Editor::isOpen())
    {
        Editor::draw(game, buffer, input);
    }
    else if (menuOpen)
    {
        drawMenu(game, buffer);
    }
    else
    {
        drawMenuButton(game, buffer);
    }
}

bool Mods::gunEnabled()
{
    return mods[MOD_GUN].on;
}

bool Mods::gunTrigger()
{
    // fire on a new press of the button, once the last shot has been reloaded
    static bool heldBefore = false;
    bool pressed = input.fire && !heldBefore;
    heldBefore = input.fire;
    return pressed && gunReload == 0;
}

uint8_t Mods::fireballStartOffset(SMBEngine& game)
{
    if (!mods[MOD_GUN].on)
    {
        return 0; // normal fireballs start at the top of the player
    }
    // A shot has been fired
    gunReload = GUN_RELOAD_FRAMES;
    gunFlash = GUN_FLASH_FRAMES;
    // Bullets start at the barrel. The fireball picture is 8 pixels high with the ball
    // in the middle, and it drops 4 pixels on its first frame.
    return (game.mem(PlayerSize) != 0 ? GUN_Y_SMALL : GUN_Y_BIG) + GUN_BARREL_ROW - 3 - 4;
}

bool Mods::skateActive(SMBEngine& game)
{
    return skateUsable(game);
}

uint8_t Mods::skateSpeed(SMBEngine& game)
{
    // If the game changed the speed itself (hit a wall, bounced off something), go with that
    int current = (int8_t)game.mem(Player_X_Speed);
    if (current != skate.lastWritten)
    {
        skate.speed = (float)current;
    }
    float speed = skate.speed;

    bool onGround = game.mem(Player_State) == 0;
    if (skate.stun > 0)
    {
        speed = 0;
    }
    else if (!onGround)
    {
        // In the air the speed carries. A / D only nudge it: enough to hop onto a pipe
        // from a standstill, not enough to change a fast jump much.
        uint8_t buttons = game.mem(Left_Right_Buttons) & game.mem(Player_CollisionBits);
        int push = (buttons & 1) ? 1 : (buttons & 2) ? -1 : 0;
        if (push != 0 && speed * push < SKATE_AIR_STEER_MAX)
        {
            speed += push * SKATE_AIR_STEER;
        }
    }
    else if (skate.grinding)
    {
        // grinding speeds you up
        float direction = (speed >= 0) ? 1.0f : -1.0f;
        if (fabsf(speed) < SKATE_GRIND_MAX_SPEED)
        {
            speed += direction * SKATE_GRIND_PUSH;
        }
    }
    else
    {
        if (ramp.on)
        {
            speed -= ramp.grade * SLOPE_GRAVITY; // uphill slows you down, downhill speeds you up
        }

        // which way is the player pushing? (not into a wall, and not while crouching)
        uint8_t buttons = game.mem(Left_Right_Buttons) & game.mem(Player_CollisionBits);
        int push = (buttons & 1) ? 1 : (buttons & 2) ? -1 : 0;
        bool crouching = (game.mem(Up_Down_Buttons) & 4) != 0;
        if (crouching)
        {
            push = 0;
        }

        if (push == 0)
        {
            // rolling: slow down a little
            float friction = crouching ? SKATE_TUCK_FRICTION : SKATE_ROLL_FRICTION;
            if (speed > friction) speed -= friction;
            else if (speed < -friction) speed += friction;
            else if (!ramp.on) speed = 0; // (on a ramp you never quite stop: you roll back down)
        }
        else if (speed * push < 0)
        {
            speed += push * SKATE_BRAKE; // pushing against the roll: brake
        }
        else
        {
            // pushing: big gains when slow, small gains when fast (as in Skate 3)
            // (speed gained on a grind is kept: pushing can't add to it, but doesn't take it away)
            if (fabsf(speed) < SKATE_MAX_SPEED)
            {
                float fraction = fabsf(speed) / SKATE_MAX_SPEED;
                speed += push * (SKATE_PUSH_SLOW + (SKATE_PUSH_FAST - SKATE_PUSH_SLOW) * fraction);
                if (speed > SKATE_MAX_SPEED) speed = SKATE_MAX_SPEED;
                if (speed < -SKATE_MAX_SPEED) speed = -SKATE_MAX_SPEED;
            }
            else
            {
                speed -= push * SKATE_ROLL_FRICTION;
            }
        }
    }

    skateSetSpeed(game, speed);
    return (uint8_t)abs(skate.lastWritten);
}

const char* Mods::debugText(SMBEngine& game)
{
    static char text[320];
    static char spriteText[80];
    snprintf(spriteText, sizeof(spriteText), "%d,%d,%d,%d,%d,%d,%d,%d", playerSprites[0][0], playerSprites[1][0], playerSprites[2][0], playerSprites[3][0],
        playerSprites[4][0], playerSprites[5][0], playerSprites[6][0], playerSprites[7][0]);
    snprintf(text, sizeof(text), "cam %d drawY %d speed %.1f air %d angle %.0f grab %d manual %d flip %.0f ramp %d grind %d slide %d combo %d x%d [%s] msg [%s] total %d stun %d block %03x sprites %s",
        Level::cameraRow() * 16, playerDrawY, skate.speed, skate.wasInAir, skate.angle, skate.grabFrames, skate.manual ? skate.manualFrames : 0, skate.flipAngle, ramp.on, skate.grinding, skate.boardslide,
        skate.comboPoints, skate.comboTricks, skate.label, skate.message, skate.total, skate.stun, onGrindable(game) ? blockUnderPlayer(game, 8) | 0x100 : blockUnderPlayer(game, 8), spriteText);
    return text;
}
