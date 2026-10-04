#include <cstdlib>
#include <fstream>
#include <map>

#include "Configuration.hpp"

static std::map<std::string, std::string> values = {
    {"audio.enabled", "1"},
    {"audio.frequency", "48000"},
    {"game.frame_rate", "60"},
    {"game.rom_file", "Super Mario Bros. (World).nes"},
    {"video.palette_file", ""},
    {"video.scale", "3"},
    {"video.scanlines", "0"},
    {"video.vsync", "1"},
};

static std::string trim(const std::string& text)
{
    size_t first = text.find_first_not_of(" \t\r\n\"");
    if (first == std::string::npos)
    {
        return "";
    }
    size_t last = text.find_last_not_of(" \t\r\n\"");
    return text.substr(first, last - first + 1);
}

static int getInt(const char* name)
{
    return atoi(values[name].c_str());
}

void Configuration::initialize(const std::string& fileName)
{
    std::ifstream file(fileName.c_str());
    std::string line;
    std::string section;
    while (std::getline(file, line))
    {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#')
        {
            continue;
        }
        if (line[0] == '[')
        {
            section = trim(line.substr(1, line.find(']') - 1));
            continue;
        }
        size_t equals = line.find('=');
        if (equals != std::string::npos)
        {
            values[section + "." + trim(line.substr(0, equals))] = trim(line.substr(equals + 1));
        }
    }
}

bool Configuration::getAudioEnabled() { return getInt("audio.enabled") != 0; }
int Configuration::getAudioFrequency() { return getInt("audio.frequency"); }
int Configuration::getFrameRate() { return getInt("game.frame_rate"); }
const std::string& Configuration::getPaletteFileName() { return values["video.palette_file"]; }
const std::string& Configuration::getRomFileName() { return values["game.rom_file"]; }
int Configuration::getRenderScale() { return getInt("video.scale"); }
bool Configuration::getScanlinesEnabled() { return getInt("video.scanlines") != 0; }
bool Configuration::getVsyncEnabled() { return getInt("video.vsync") != 0; }
