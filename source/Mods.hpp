#ifndef MODS_HPP
#define MODS_HPP

#include <cstdint>

class SMBEngine;

/** Where the custom level is kept (see Level.hpp), relative to the program's folder. */
#define LEVEL_FILE "levels/skatepark.txt"

/**
 * What the player is doing with the mouse and the keys that aren't part of the
 * game's own controller. Main.cpp fills this in every frame.
 */
struct ModInput
{
    int mouseX = -1;          // position in the game's 256x240 picture
    int mouseY = -1;
    bool mouseLeft = false;   // buttons held down
    bool mouseRight = false;
    int wheel = 0;            // mouse wheel movement this frame (up is positive)
    bool spinLeft = false;    // skate: spin (left / right arrow keys)
    bool spinRight = false;
    bool grab = false;        // skate: grab the board (down arrow key)
    bool manual = false;      // skate: manual on the ground (up arrow key)
    bool flipBack = false;    // skate: backflip in the air (down arrow key)
    bool flipFront = false;   // skate: frontflip in the air (up arrow key)
    bool fire = false;        // gun: fire (E)
    bool menuKey = false;     // open or close the menu (Esc or Tab)
    bool scrollLeft = false;  // level editor: scroll sideways (A / D)
    bool scrollRight = false;
    bool scrollUp = false;    // level editor: scroll up and down (W / S)
    bool scrollDown = false;
};

/**
 * Game mods. All of the mod code is in Mods.cpp; the rest of the program only
 * calls the functions below.
 */
namespace Mods
{
    /** Called once per frame with the mouse and extra keys, before update(). */
    void setInput(const ModInput& input);

    /** Called once per frame, just before the game runs its own logic for the frame. */
    void update(SMBEngine& engine);

    /** Is a menu or the level editor open? The game stands still while one is. */
    bool frozen();

    /** Did the player choose QUIT in the menu? */
    bool quitRequested();

    /** Called just before the frame is drawn (the place to change colors). */
    void beforeRender(SMBEngine& engine);

    /** Called after the frame is drawn into a 256x240 buffer (the place to draw on top). */
    void afterRender(SMBEngine& engine, uint32_t* buffer);

    /** One line describing what the mods are doing, for testing. */
    const char* debugText(SMBEngine& engine);

    // Hooks called from inside the game's own code (SMB.cpp, search for "MOD:")

    /** Is the player riding the skateboard right now? */
    bool skateActive(SMBEngine& engine);

    /**
     * Skateboard version of the game's ImposeFriction routine: works out the player's new
     * horizontal speed, stores it in Player_X_Speed and returns its absolute value.
     */
    uint8_t skateSpeed(SMBEngine& engine);

    /** Can the player shoot even without a fire flower? */
    bool gunEnabled();

    /** Is the gun being fired this frame? (fire button newly pressed, and reloaded) */
    bool gunTrigger();

    /** How far below the top of the player a new fireball starts, in pixels. */
    uint8_t fireballStartOffset(SMBEngine& engine);
}

#endif // MODS_HPP
