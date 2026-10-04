Super Mario Bros. - C++ version with mods
=========================================

RUN:    double-click smb-mods.exe
        (SDL2.dll, the .nes file and the levels folder must stay next to it)

KEYS:   A / D = move        W = up        S = down (crouch)
        Space = jump        Left Shift = run (and throw fireballs when the gun is off)
        E = fire the gun (GUN mod: a bolt-action sniper rifle - one fast shot, then a
            short reload)
        Enter = start the game / pause
        Esc or Tab = menu   F = fullscreen

MENU:   press Esc or Tab, or click MENU in the top right corner, then click with the mouse:
        - click a mod to switch it on or off (FLY, COLOR, BIG, GUN, SKATE)
        - PLAY SKATE PARK    the custom level, with the skateboard on
        - PLAY NORMAL GAME   the original game from 1-1
        - LEVEL EDITOR       change the custom level
        The game stands still while the menu is open.

SKATE MOD (not in water levels):
        A / D                 push; let go to keep rolling; the opposite direction brakes
        S, then Space         crouch for a moment, then jump = big ollie
        Space                 ollie
        Up arrow              manual (ride on the back wheels) while rolling. It scores
                              while it lasts; hold it as you land and the combo carries
                              on through the manual; let go to bank it
        Up / Down arrow       in the air: frontflip / backflip. Let go when you are nearly
                              upright and he rights himself; land on your head and you bail
        Left / Right arrow    spin in the air. Let go when you are nearly straight or
                              backwards and the board lines up; land sideways and you bail
        S                     in the air: grab the board - let go before you land or you bail
        Grind                 land on a pipe, bricks, blocks, a bridge or a cannon with
                              some speed; still spinning as you land = boardslide
                              Grinding speeds you up, beyond what pushing can reach.
        Ramps                 (custom levels) slow you going up and speed you up going
                              down; ride off the top of one and it throws you into the air.
        A / D barely steer in the air: the speed you take off with is the speed you keep.
        Tricks add up in a combo (points x number of tricks) that is kept on a clean
        landing and lost on a bail. All the numbers are at the top of source\Mods.cpp.

LEVEL EDITOR:
        Click a tool in the top row, then draw on the level.
        Left button = draw (hold and drag)      Right button = rub out
        A / D or mouse wheel = scroll sideways  W / S = scroll up and down
        arrow buttons = jump a whole screen left, right, up or down
        The level is 52 rows high (four screens) and the game shows 13 at a time, with a
        camera that follows the player up and down. The dotted lines mark the 13 rows of
        an ordinary one-screen level; build above and below them as you like. The lowest
        row you use is the bottom of the world: fall below it and you lose a life.
        START (the S in the top row of the toolbar): click the block the player should
        start in.
        PIPE: click where the top should be; it grows down to the ground.
        RAMPS (top row of the toolbar): steep up, steep down, gentle up, gentle down. A
        gentle ramp is two blocks long. A ramp is only a surface: put solid blocks under
        it if it is off the ground, and behind its high end if you want a platform there.
        Enemies and bullets ignore ramps.
        FLAGPOLE and CASTLE: one of each. Click the block just above the ground where it
        should stand (the pole needs 10 clear blocks above it); put the castle about 6
        blocks after the flag, on the same row.
        PLAY saves and starts the level. SAVE saves. EXIT (or Esc) saves and goes back.
        The level is the text file levels\skatepark.txt, one character per block, so it
        can also be edited in Notepad. The characters are listed at the top of the file.
        (A level that only uses the 13 ordinary rows is saved as 13 lines, a taller one
        as all 52.)
        In a custom level the background is clouds only (no hills or bushes), and an
        enemy only appears if its row is on screen when you reach it.

CHANGE THINGS:
        source\Mods.cpp      the menu and all the mods - start here
        source\Level.cpp     custom levels          source\Editor.cpp   the level editor
        source\Draw.cpp      drawing helpers        source\Main.cpp     window, keys, mouse
        source\SMB\SMB.cpp   the game itself, translated from assembly line by line
                             (search for "MOD:" to see the places it was changed)
        source\SMB\SMBConstants.hpp   the name of every piece of game memory
        docs\smbdis.asm      the original commented disassembly, explains everything

BUILD:  double-click build.bat, then run smb-mods.exe again.
        It needs nothing installed: the compiler (Zig) is included in tools\ and is
        unpacked the first time.

SETTINGS (optional): create smbc.conf next to the exe, for example
        [video]
        scale = 4
        scanlines = 1
        [audio]
        enabled = 0

WHERE THIS CAME FROM: the C++ translation is the open-source project
SuperMarioBros-C by Mitchell Sternke (github.com/MitchellSternke/SuperMarioBros-C),
made from doppelganger's disassembly. See ORIGINAL-README.md. The skate mod's rules
are loosely based on the Skate 3 mechanics in github.com/SK8-ENGINE/skate-3-rust-engine
(ideas only, none of its code).
