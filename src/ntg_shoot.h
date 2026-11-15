// nettoge.com development / screenshot build ("shoot build").
// MODIFIED FILE NOTICE: this file is an addition to fallout2-ce (Sustainable
// Use License 1.0, see LICENSE.md). It is not part of the original sources.
//
// Compiled only with -DNTG_SHOOT=ON (fallout2-ce-shoot.exe). The play build
// contains none of this. The shoot build keeps its window hidden, ignores the
// real mouse and focus changes, and is driven by command files:
//
//   <game folder>/shoot/cmd.txt   one command per line, first line "id <n>"
//   <game folder>/shoot/done.txt  written when all lines are done: "id <n>"
//                                 followed by the output of the commands
//
// Commands (see tools/fo.py):
//   key <code>            game key code (27 = Esc, 13 = Enter, 112 = 'p' ...)
//   text <string>         types the characters
//   move <x> <y>          mouse position in game pixels (640x480)
//   click <x> <y>         left click      rclick <x> <y>   right click
//   down / up             left button held / released (drag)
//   wait <ms>
//   shot <file.bmp>       the game screen as an 8-bit BMP
//   state                 map, tile, mouse, game state
//   critters              critters on this elevation: name, tile, screen x y
//   map <name> [elev] [tile]   go to a map (e.g. ARVILLAG.MAP)
//   tp <tile>             move the player
//   talk <name>           walk to the critter and talk
//   pip                   give the Pip-Boy (marks the vault suit movie as seen)
//   xp <n>                experience for the player (level up, perks)
//   give <pid> [count]    an item for the player
//   wield <pid> [hand]    a new item in a hand (0 = left, 1 = right)
//   die                   the player dies (game over screen)
//   quit

#ifndef FALLOUT_NTG_SHOOT_H_
#define FALLOUT_NTG_SHOOT_H_

#ifdef NTG_SHOOT

namespace fallout {

// Called from _process_bk() instead of reading the real mouse.
void shoot_tick();

} // namespace fallout

#endif

#endif /* FALLOUT_NTG_SHOOT_H_ */
