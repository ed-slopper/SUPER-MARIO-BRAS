// Editor.cpp - the level editor.
//
// The screen is laid out like the game: a 32 pixel strip at the top (here the toolbar
// instead of the score) and below it a window onto the level, 16 blocks wide and 13
// high, each block 16x16 pixels. The level itself is much bigger than the window.
//
//   left mouse button    draw with the selected tool (hold and drag for most tools)
//   right mouse button   rub out
//   A / D, mouse wheel   scroll sideways
//   W / S                scroll up and down

#include <cstdio>
#include <cstring>

#include "SMB/SMBConstants.hpp"
#include "SMB/SMBEngine.hpp"

#include "Draw.hpp"
#include "Editor.hpp"
#include "Level.hpp"
#include "Mods.hpp"

namespace
{

struct Tool
{
    char character;    // what it puts in the level (see Level.hpp)
    const char* name;
    uint8_t icon;      // level block shown on the button, or 0 to show the letter instead
    const char* letter;
    int x;             // where its button is in the toolbar
    int y;
};

const Tool tools[] = {
    // bottom row of the toolbar
    {'.', "RUBBER", 0, "-", 1, 16},
    {'#', "GROUND", 0x54, "", 20, 16},
    {'B', "BRICK", 0x51, "", 39, 16},
    {'?', "COIN BLOCK", 0xc0, "", 58, 16},
    {'M', "POWER-UP BLOCK", 0xc1, "M", 77, 16},
    {'X', "SOLID BLOCK", 0x61, "", 96, 16},
    {'P', "PIPE", 0x12, "", 115, 16},
    {'=', "BRIDGE", 0x63, "", 134, 16},
    {'o', "COIN", 0xc2, "", 153, 16},
    {'g', "GOOMBA", 0, "G", 172, 16},
    {'k', "KOOPA", 0, "K", 191, 16},
    {'F', "FLAGPOLE", 0, "F", 210, 16},
    {'C', "CASTLE", 0, "C", 229, 16},
    // top row, between the buttons: the ramps and the start
    {'/', "RAMP UP", 0, "", 116, 0},
    {'\\', "RAMP DOWN", 0, "", 133, 0},
    {'1', "GENTLE RAMP UP", 0, "", 150, 0},
    {'3', "GENTLE RAMP DOWN", 0, "", 167, 0},
    {'S', "START", 0, "S", 184, 0},
};
const int TOOL_COUNT = sizeof(tools) / sizeof(tools[0]);

// Layout
const int TOOLBAR_HEIGHT = 32;
const int VIEW_COLUMNS = 16;
const int VIEW_ROWS = Level::SCREEN_ROWS;

struct Button
{
    int x;
    int width;
    const char* label;
};
enum { BUTTON_PLAY, BUTTON_SAVE, BUTTON_EXIT, BUTTON_LEFT_PAGE, BUTTON_RIGHT_PAGE, BUTTON_UP_PAGE, BUTTON_DOWN_PAGE, BUTTON_COUNT };
const Button buttons[BUTTON_COUNT] = {
    {2, 36, "PLAY"},
    {40, 36, "SAVE"},
    {78, 36, "EXIT"},
    {204, 12, ""},  // the four arrows: move a whole screen left, right, up, down
    {217, 12, ""},
    {230, 12, ""},
    {243, 12, ""},
};
const int BUTTON_Y = 2;
const int BUTTON_HEIGHT = 12;

bool editorOpen = false;
int camera = 0;                               // the leftmost column on screen
int cameraRow = Level::BASE_ROW;              // the top row on screen
int tool = 1;
int scrollDelay = 0;
int messageTimer = 0;
char message[24] = "";
bool waitForRelease = false; // ignore the mouse until the click that opened the editor is over

bool inside(const ModInput& input, int x, int y, int width, int height)
{
    return input.mouseX >= x && input.mouseX < x + width && input.mouseY >= y && input.mouseY < y + height;
}

void showMessage(const char* text)
{
    snprintf(message, sizeof(message), "%s", text);
    messageTimer = 90;
}

void save()
{
    showMessage(Level::save(LEVEL_FILE) ? "SAVED" : "SAVE FAILED");
}

void scrollTo(int column, int row)
{
    if (column > Level::MAX_COLUMNS - VIEW_COLUMNS) column = Level::MAX_COLUMNS - VIEW_COLUMNS;
    if (column < 0) column = 0;
    if (row > Level::HEIGHT - VIEW_ROWS) row = Level::HEIGHT - VIEW_ROWS;
    if (row < 0) row = 0;
    camera = column;
    cameraRow = row;
}

/** Rub out every copy of a character (for things there can only be one of). */
void removeAll(char character)
{
    for (int column = 0; column < Level::width(); column++)
    {
        for (int row = 0; row < Level::HEIGHT; row++)
        {
            if (Level::get(column, row) == character)
            {
                Level::set(column, row, '.');
            }
        }
    }
}

void useTool(int column, int row, bool click)
{
    char character = tools[tool].character;
    switch (character)
    {
    case 'P':
        // a pipe: two blocks wide, from here down to whatever it lands on
        if (click)
        {
            for (int r = row; r < Level::HEIGHT; r++)
            {
                char left = Level::get(column, r);
                char right = Level::get(column + 1, r);
                if ((left != '.' && left != 'P') || (right != '.' && right != 'P'))
                {
                    break;
                }
                Level::set(column, r, 'P');
                Level::set(column + 1, r, 'P');
            }
        }
        break;
    case '1':
    case '3':
        // a gentle ramp is two blocks long: its low half and its high half
        if (click)
        {
            Level::set(column, row, character);
            Level::set(column + 1, row, character == '1' ? '2' : '4');
        }
        break;
    case 'F':
    case 'C':
    case 'S':
        // there is only one flagpole, one castle and one start
        if (click)
        {
            removeAll(character);
            Level::set(column, row, character);
        }
        break;
    default:
        Level::set(column, row, character);
        break;
    }
}

/** A small arrow in a button: direction 0 = left, 1 = right, 2 = up, 3 = down. */
void drawArrow(uint32_t* buffer, int x, int y, int direction, uint32_t color)
{
    for (int i = 0; i < 4; i++)
    {
        // a triangle, 4 rows of 1, 3, 5, 7 pixels
        int length = 1 + 2 * i;
        switch (direction)
        {
        case 0: Draw::fillRect(buffer, x + 2 + i, y + 6 - i, 1, length, color); break;
        case 1: Draw::fillRect(buffer, x + 8 - i, y + 6 - i, 1, length, color); break;
        case 2: Draw::fillRect(buffer, x + 5 - i, y + 4 + i, length, 1, color); break;
        case 3: Draw::fillRect(buffer, x + 5 - i, y + 8 - i, length, 1, color); break;
        }
    }
}

} // end of private section

bool Editor::isOpen()
{
    return editorOpen;
}

void Editor::open(SMBEngine& game)
{
    editorOpen = true;
    waitForRelease = true;
    if (Level::active)
    {
        // start where the player is
        scrollTo(game.mem(ScreenLeft_PageLoc) * 16 + game.mem(ScreenLeft_X_Pos) / 16, Level::cameraRow());
    }
    else
    {
        scrollTo(Level::startColumn() - 2, Level::startRow() - 10);
    }
}

void Editor::close()
{
    if (editorOpen)
    {
        Level::save(LEVEL_FILE);
    }
    editorOpen = false;
}

bool Editor::update(SMBEngine& game, const ModInput& input, bool click)
{
    if (messageTimer > 0)
    {
        messageTimer--;
    }

    // Scrolling
    if (input.wheel != 0)
    {
        scrollTo(camera - input.wheel * 2, cameraRow);
    }
    if (scrollDelay > 0)
    {
        scrollDelay--;
    }
    else if (input.scrollLeft || input.scrollRight || input.scrollUp || input.scrollDown)
    {
        scrollTo(camera + (input.scrollRight ? 1 : 0) - (input.scrollLeft ? 1 : 0),
                 cameraRow + (input.scrollDown ? 1 : 0) - (input.scrollUp ? 1 : 0));
        scrollDelay = 2;
    }

    if (waitForRelease)
    {
        waitForRelease = input.mouseLeft || input.mouseRight;
        return false;
    }

    if (input.mouseY < TOOLBAR_HEIGHT)
    {
        if (!click)
        {
            return false;
        }
        for (int i = 0; i < BUTTON_COUNT; i++)
        {
            if (inside(input, buttons[i].x, BUTTON_Y, buttons[i].width, BUTTON_HEIGHT))
            {
                switch (i)
                {
                case BUTTON_PLAY:
                    close(); // saves
                    return true;
                case BUTTON_SAVE:
                    save();
                    break;
                case BUTTON_EXIT:
                    close();
                    break;
                case BUTTON_LEFT_PAGE:
                    scrollTo(camera - VIEW_COLUMNS, cameraRow);
                    break;
                case BUTTON_RIGHT_PAGE:
                    scrollTo(camera + VIEW_COLUMNS, cameraRow);
                    break;
                case BUTTON_UP_PAGE:
                    scrollTo(camera, cameraRow - VIEW_ROWS);
                    break;
                case BUTTON_DOWN_PAGE:
                    scrollTo(camera, cameraRow + VIEW_ROWS);
                    break;
                }
            }
        }
        for (int i = 0; i < TOOL_COUNT; i++)
        {
            if (inside(input, tools[i].x, tools[i].y, 16, 16))
            {
                tool = i;
            }
        }
        return false;
    }

    // Drawing on the level
    int column = camera + input.mouseX / 16;
    int row = cameraRow + (input.mouseY - TOOLBAR_HEIGHT) / 16;
    if (input.mouseX >= 0 && input.mouseX < 256 && input.mouseY < 240)
    {
        if (input.mouseRight)
        {
            Level::set(column, row, '.');
        }
        else if (input.mouseLeft)
        {
            useTool(column, row, click);
        }
    }
    return false;
}

void Editor::draw(SMBEngine& game, uint32_t* buffer, const ModInput& input)
{
    const uint32_t white = 0xfff8f8f8;
    const uint32_t gray = 0xff909090;
    const uint32_t dark = 0xff202020;
    const uint32_t yellow = 0xfff8b800;

    // The level
    uint32_t sky = buffer[0]; // the game has already filled the picture; its top left pixel is sky
    Draw::fillRect(buffer, 0, 0, 256, 240, sky);
    for (int y = 0; y < VIEW_ROWS; y++)
    {
        int row = cameraRow + y;
        int screenY = TOOLBAR_HEIGHT + y * 16;
        if (row == Level::BASE_ROW || row == Level::BASE_ROW + Level::SCREEN_ROWS)
        {
            // dotted lines above and below the rows of a normal one-screen level
            for (int x = 0; x < 256; x += 4)
            {
                Draw::setPixel(buffer, x, screenY, white);
            }
        }
        for (int x = 0; x < VIEW_COLUMNS; x++)
        {
            int column = camera + x;
            int screenX = x * 16;
            if (column % 16 == 0 && y == 0)
            {
                // a dotted line and a number at the start of every screen-width of level
                for (int dot = TOOLBAR_HEIGHT; dot < 240; dot += 4)
                {
                    Draw::setPixel(buffer, screenX, dot, white);
                }
                char number[8];
                snprintf(number, sizeof(number), "%d", column / 16 + 1);
                Draw::shadowText(game, buffer, screenX + 3, TOOLBAR_HEIGHT + 2, number, white);
            }
            uint8_t block = Level::metatileAt(column, row);
            if (block != 0)
            {
                Draw::metatile(game, buffer, screenX, screenY, block);
            }
            int leftHeight, rightHeight;
            if (Level::slopeAt(column, row, leftHeight, rightHeight))
            {
                Draw::slope(game, buffer, screenX, screenY, leftHeight, rightHeight);
            }
            char c = Level::get(column, row);
            if (c == 'g' || c == 'k')
            {
                Draw::fillRect(buffer, screenX + 2, screenY + 2, 12, 12, c == 'g' ? 0xffa85010 : 0xff00a800);
                Draw::text(game, buffer, screenX + 4, screenY + 4, c == 'g' ? "G" : "K", white);
            }
            else if (c == 'M')
            {
                Draw::shadowText(game, buffer, screenX + 4, screenY + 4, "M", white);
            }
            else if (c == 'C')
            {
                Draw::shadowText(game, buffer, screenX + 4, screenY + 4, "C", yellow);
            }
            if (column == Level::startColumn() && row == Level::startRow())
            {
                // where the player starts
                Draw::frame(buffer, screenX, screenY, 16, 16, yellow);
                Draw::shadowText(game, buffer, screenX + 4, screenY + 4, "S", yellow);
            }
        }
    }

    // The block under the mouse, and where it is
    if (input.mouseY >= TOOLBAR_HEIGHT && input.mouseY < 240 && input.mouseX >= 0 && input.mouseX < 256)
    {
        int x = input.mouseX / 16;
        int y = (input.mouseY - TOOLBAR_HEIGHT) / 16;
        Draw::frame(buffer, x * 16, TOOLBAR_HEIGHT + y * 16, 16, 16, white);
        char where[32];
        snprintf(where, sizeof(where), "%d-%d", camera + x + 1, cameraRow + y + 1);
        Draw::shadowText(game, buffer, 252 - (int)strlen(where) * 8, 229, where, white);
    }

    // The toolbar
    Draw::fillRect(buffer, 0, 0, 256, TOOLBAR_HEIGHT, dark);
    for (int i = 0; i < BUTTON_COUNT; i++)
    {
        bool hover = inside(input, buttons[i].x, BUTTON_Y, buttons[i].width, BUTTON_HEIGHT);
        Draw::fillRect(buffer, buttons[i].x, BUTTON_Y, buttons[i].width, BUTTON_HEIGHT, hover ? 0xff505050 : 0xff303030);
        Draw::frame(buffer, buttons[i].x, BUTTON_Y, buttons[i].width, BUTTON_HEIGHT, hover ? white : gray);
        if (i >= BUTTON_LEFT_PAGE)
        {
            drawArrow(buffer, buttons[i].x, BUTTON_Y, i - BUTTON_LEFT_PAGE, white);
        }
        else
        {
            Draw::text(game, buffer, buttons[i].x + 2, BUTTON_Y + 2, buttons[i].label, white);
        }
    }

    for (int i = 0; i < TOOL_COUNT; i++)
    {
        int x = tools[i].x;
        int y = tools[i].y;
        Draw::fillRect(buffer, x, y, 16, 16, sky);
        if (tools[i].icon != 0)
        {
            Draw::metatile(game, buffer, x, y, tools[i].icon);
        }
        if (tools[i].letter[0] != 0)
        {
            Draw::shadowText(game, buffer, x + 4, y + 4, tools[i].letter, white);
        }
        switch (tools[i].character)
        {
        case '/':  Draw::slope(game, buffer, x, y, 0, 16); break;
        case '\\': Draw::slope(game, buffer, x, y, 16, 0); break;
        case '1':  Draw::slope(game, buffer, x, y, 0, 8); break;
        case '3':  Draw::slope(game, buffer, x, y, 8, 0); break;
        }
        if (i == tool)
        {
            Draw::frame(buffer, x, y, 16, 16, white);
            Draw::frame(buffer, x + 1, y + 1, 14, 14, yellow);
        }
        else if (inside(input, x, y, 16, 16))
        {
            Draw::frame(buffer, x, y, 16, 16, gray);
        }
    }

    // The name of the selected tool, or a message, in the bottom corner
    Draw::shadowText(game, buffer, 4, 229, messageTimer > 0 ? message : tools[tool].name, yellow);
}
