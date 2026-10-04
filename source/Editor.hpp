#ifndef EDITOR_HPP
#define EDITOR_HPP

#include <cstdint>

class SMBEngine;
struct ModInput;

/**
 * The level editor: a full-screen view of the custom level (see Level.hpp) that is
 * drawn on and changed with the mouse.
 */
namespace Editor
{
    bool isOpen();
    void open(SMBEngine& game);
    void close();

    /**
     * Handle a frame of input. click/rightClick are true only on the frame the button
     * goes down. Returns true if the player asked to play the level.
     */
    bool update(SMBEngine& game, const ModInput& input, bool click);

    void draw(SMBEngine& game, uint32_t* buffer, const ModInput& input);
}

#endif // EDITOR_HPP
