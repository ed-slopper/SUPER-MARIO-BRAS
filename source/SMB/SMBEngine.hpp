#ifndef SMBENGINE_HPP
#define SMBENGINE_HPP

#include <cstdint>
#include <cstddef>

#include "../Emulation/MemoryAccess.hpp"

#include "SMBDataPointers.hpp"

class APU;
class Controller;
class PPU;

/**
 * Engine that runs Super Mario Bros.
 * Handles emulation of various NES subsystems for compatibility and accuracy.
 */
class SMBEngine
{
    friend class MemoryAccess;
    friend class PPU;
public:
    /**
     * Construct a new SMBEngine instance.
     *
     * @param romImage the data from the Super Mario Bros. ROM image.
     */
    SMBEngine(uint8_t* romImage);

    ~SMBEngine();

    /**
     * Callback for handling audio buffering.
     */
    void audioCallback(uint8_t* stream, int length);

    /**
     * Get player 1's controller.
     */
    Controller& getController1();

    /**
     * Get player 2's controller.
     */
    Controller& getController2();

    /**
     * Direct access to a byte of the game's 2KB of memory, for reading or writing.
     * Unlike the M() used by the game code, this has no side effects.
     */
    uint8_t& mem(uint16_t address) { return ram[address & 0x7ff]; }

    /**
     * Read a byte of the game's memory.
     */
    uint8_t peek(uint16_t address) const { return ram[address & 0x7ff]; }

    /**
     * The game's graphics (8KB of tile data from the ROM: sprites first, then background).
     */
    const uint8_t* getGraphics() const { return chr; }

    /**
     * The picture processing unit (palette, background tiles, sprites).
     */
    PPU& getPPU() { return *ppu; }

    /**
     * The four 8x8 background tiles that make up a 16x16 level block ("metatile"):
     * top left, bottom left, top right, bottom right.
     */
    const uint8_t* getMetatileTiles(uint8_t metatile);

    /**
     * Unused space in the game's data area, for mods that need the game to read their data.
     * customDataAddress() is the address the game would use to read customData()[0].
     */
    uint8_t* customData();
    uint16_t customDataAddress();

    /**
     * Render the screen to a buffer.
     *
     * @param buffer a 256x240 32-bit color buffer for storing the rendering.
     */
    void render(uint32_t* buffer);

    /**
     * Reset the game engine to power-on state.
     */
    void reset();

    /**
     * Update the game engine by one frame.
     */
    void update();

private:
    // NES Emulation subsystems:
    APU* apu;
    PPU* ppu;
    Controller* controller1;
    Controller* controller2;

    // Fields for NES CPU emulation:
    bool c;                      /**< Carry flag. */
    bool z;                      /**< Zero flag. */
    bool n;                      /**< Negative flag. */
    uint8_t registerA;           /**< Accumulator register. */
    uint8_t registerX;           /**< X index register. */
    uint8_t registerY;           /**< Y index register. */
    uint8_t registerS;           /**< Stack index register. */
    MemoryAccess a;              /**< Wrapper for A register. */
    MemoryAccess x;              /**< Wrapper for X register. */
    MemoryAccess y;              /**< Wrapper for Y register. */
    MemoryAccess s;              /**< Wrapper for S register. */
    uint8_t dataStorage[0x8000]; /**< 32kb of storage for constant data. */
    uint8_t ram[0x800];          /**< 2kb of RAM. */
    uint8_t* chr;                /**< Pointer to CHR data from the ROM. */
    int returnIndexStack[100];   /**< Stack for managing JSR subroutines. */
    int returnIndexStackTop;     /**< Current index of the top of the call stack. */

    // Pointers to constant data used in the decompiled code
    //
    SMBDataPointers dataPointers;

    /**
     * Run the decompiled code for the game.
     * 
     * See SMB.cpp for implementation.
     *
     * @param mode the mode to run. 0 runs initialization routines, 1 runs the logic for frames.
     */
    void code(int mode);

    /**
     * Logic for CMP, CPY, and CPY instructions.
     */
    void compare(uint8_t value1, uint8_t value2);

    /**
     * BIT instruction.
     */
    void bit(uint8_t value);

    /**
     * Get CHR data from the ROM.
     */
    uint8_t* getCHR();

    /**
     * Get a pointer to a byte in the address space.
     */
    uint8_t* getDataPointer(uint16_t address);

    /**
     * Get a memory access object for a particular address.
     */
    MemoryAccess getMemory(uint16_t address);

    /**
     * Get a word of memory from a zero-page address and the next byte (wrapped around),
     * in little-endian format.
     */
    uint16_t getMemoryWord(uint8_t address);

    /**
     * Load all constant data that was present in the SMB ROM.
     * 
     * See SMBData.cpp for implementation.
     */
    void loadConstantData();

    /**
     * PHA instruction.
     */
    void pha();

    /**
     * PLA instruction.
     */
    void pla();

    /**
     * Pop an index from the call stack.
     */
    int popReturnIndex();

    /**
     * Push an index to the call stack.
     */
    void pushReturnIndex(int index);

    /**
     * Read data from an address in the NES address space.
     */
    uint8_t readData(uint16_t address);

    /**
     * Set the zero and negative flags based on a result value.
     */
    void setZN(uint8_t value);

    /**
     * Write data to an address in the NES address space.
     */
    void writeData(uint16_t address, uint8_t value);

    /**
     * Map constant data to the address space. The address must be at least 0x8000.
     */
    void writeData(uint16_t address, const uint8_t* data, std::size_t length);
};

#endif // SMBENGINE_HPP
