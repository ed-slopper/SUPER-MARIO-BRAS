#ifndef CONFIGURATION_HPP
#define CONFIGURATION_HPP

#include <string>

/**
 * Settings read from smbc.conf (INI format: [section] then name = value).
 * Every setting has a default, so the file is optional.
 */
class Configuration
{
public:
    static void initialize(const std::string& fileName);

    static bool getAudioEnabled();          // audio.enabled      (default 1)
    static int getAudioFrequency();         // audio.frequency    (default 48000)
    static int getFrameRate();              // game.frame_rate    (default 60)
    static const std::string& getPaletteFileName(); // video.palette_file (default none)
    static const std::string& getRomFileName();     // game.rom_file
    static int getRenderScale();            // video.scale        (default 3)
    static bool getScanlinesEnabled();      // video.scanlines    (default 0)
    static bool getVsyncEnabled();          // video.vsync        (default 1)
};

#endif // CONFIGURATION_HPP
