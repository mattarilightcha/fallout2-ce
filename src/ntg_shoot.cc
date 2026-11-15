// nettoge.com development / screenshot build ("shoot build").
// MODIFIED FILE NOTICE: this file is an addition to fallout2-ce (Sustainable
// Use License 1.0, see LICENSE.md). It is not part of the original sources.

#include "ntg_shoot.h"

#ifdef NTG_SHOOT

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <SDL.h>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

#include "actions.h"
#include "critter.h"
#include "game.h"
#include "game_movie.h"
#include "geometry.h"
#include "input.h"
#include "inventory.h"
#include "item.h"
#include "map.h"
#include "mouse.h"
#include "movie_lib.h"
#include "obj_types.h"
#include "stat.h"
#include "object.h"
#include "svga.h"
#include "tile.h"
#include "worldmap.h"

namespace fallout {

// Defined in game_movie.cc (shoot build only).
void shoot_movie_seen(int movie);

// Defined in input.cc (shoot build only).
void shoot_dump_screen(ScreenshotHandler* func);

static std::vector<std::string> g_queue;
static size_t g_pos = 0;
static std::string g_id;
static std::string g_out;
static unsigned int g_wait_until = 0;
static unsigned int g_next_poll = 0;
static int g_buttons = 0;
static bool g_busy = false;
static std::string g_shot_path;
static bool g_in_tick = false;

// Pending "press then release" of a click.
static int g_release_at = 0;

static const char* dir()
{
    const char* d = getenv("FO_SHOOT_DIR");
    return d != NULL ? d : "shoot";
}

static std::string path_of(const char* name)
{
    return std::string(dir()) + "/" + name;
}

static void out(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    g_out += buf;
    g_out += "\n";
}

static bool load_commands()
{
    std::string p = path_of("cmd.txt");
    FILE* f = fopen(p.c_str(), "rb");
    if (f == NULL) {
        return false;
    }

    std::string all;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        all.append(buf, n);
    }
    fclose(f);
    remove(p.c_str());

    g_queue.clear();
    g_pos = 0;
    g_id = "0";
    g_out.clear();

    size_t start = 0;
    while (start <= all.size()) {
        size_t end = all.find('\n', start);
        if (end == std::string::npos) {
            end = all.size();
        }
        std::string line = all.substr(start, end - start);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        if (line.compare(0, 3, "id ") == 0) {
            g_id = line.substr(3);
        } else if (!line.empty() && line[0] != '#') {
            g_queue.push_back(line);
        }
        start = end + 1;
    }
    return true;
}

static void finish()
{
    std::string tmp = path_of("done.tmp");
    FILE* f = fopen(tmp.c_str(), "wb");
    if (f != NULL) {
        fprintf(f, "id %s\n%s", g_id.c_str(), g_out.c_str());
        fclose(f);
        std::string dst = path_of("done.txt");
        remove(dst.c_str());
        rename(tmp.c_str(), dst.c_str());
    }
    g_busy = false;
}

static int write_bmp(int width, int height, unsigned char* data, unsigned char* palette)
{
    FILE* f = fopen(g_shot_path.c_str(), "wb");
    if (f == NULL) {
        return -1;
    }

    int row = (width + 3) & ~3;
    unsigned int offset = 14 + 40 + 1024;
    unsigned int size = offset + row * height;

    unsigned char head[54];
    memset(head, 0, sizeof(head));
    head[0] = 'B';
    head[1] = 'M';
    memcpy(head + 2, &size, 4);
    memcpy(head + 10, &offset, 4);
    unsigned int v = 40;
    memcpy(head + 14, &v, 4);
    memcpy(head + 18, &width, 4);
    memcpy(head + 22, &height, 4);
    unsigned short s = 1;
    memcpy(head + 26, &s, 2);
    s = 8;
    memcpy(head + 28, &s, 2);
    fwrite(head, 1, sizeof(head), f);

    // The system palette is 6 bits per channel.
    for (int i = 0; i < 256; i++) {
        unsigned char bgra[4];
        bgra[2] = palette[i * 3] << 2;
        bgra[1] = palette[i * 3 + 1] << 2;
        bgra[0] = palette[i * 3 + 2] << 2;
        bgra[3] = 0;
        fwrite(bgra, 1, 4, f);
    }

    unsigned char pad[4] = { 0, 0, 0, 0 };
    for (int y = height - 1; y >= 0; y--) {
        fwrite(data + y * width, 1, width, f);
        fwrite(pad, 1, row - width, f);
    }

    fclose(f);
    return 0;
}

static void shot_surface()
{
    SDL_Surface* sf = gSdlSurface;
    if (sf == NULL || sf->format->palette == NULL) {
        return;
    }
    int w = sf->w;
    int h = sf->h;
    std::vector<unsigned char> px(w * h);
    for (int y = 0; y < h; y++) {
        memcpy(&px[y * w], (unsigned char*)sf->pixels + sf->pitch * y, w);
    }
    unsigned char pal[768];
    SDL_Color* c = sf->format->palette->colors;
    for (int i = 0; i < 256; i++) {
        pal[i * 3] = c[i].r >> 2;
        pal[i * 3 + 1] = c[i].g >> 2;
        pal[i * 3 + 2] = c[i].b >> 2;
    }
    write_bmp(w, h, px.data(), pal);
}

static Object* find_critter(const char* name)
{
    Object* best = NULL;
    int bestDist = 0;
    for (Object* obj = objectFindFirstAtElevation(gElevation); obj != NULL; obj = objectFindNextAtElevation()) {
        if (FID_TYPE(obj->fid) != OBJ_TYPE_CRITTER || obj == gDude) {
            continue;
        }
        char* n = objectGetName(obj);
        if (n == NULL || SDL_strcasecmp(n, name) != 0) {
            continue;
        }
        int d = objectGetDistanceBetween(gDude, obj);
        if (best == NULL || d < bestDist) {
            best = obj;
            bestDist = d;
        }
    }
    return best;
}

// Runs one command. Returns false when the command needs more ticks.
static void run(const std::string& line)
{
    char cmd[32];
    cmd[0] = '\0';
    sscanf(line.c_str(), "%31s", cmd);
    const char* arg = line.c_str() + strlen(cmd);
    while (*arg == ' ') {
        arg++;
    }

    int a = 0;
    int b = 0;
    int c = 0;
    int n = sscanf(arg, "%d %d %d", &a, &b, &c);
    unsigned int now = SDL_GetTicks();

    if (strcmp(cmd, "key") == 0) {
        enqueueInputEvent(a);
        g_wait_until = now + 60;
    } else if (strcmp(cmd, "text") == 0) {
        for (const char* p = arg; *p != '\0'; p++) {
            enqueueInputEvent((unsigned char)*p);
        }
        g_wait_until = now + 60;
    } else if (strcmp(cmd, "move") == 0) {
        _mouse_set_position(a, b);
        g_wait_until = now + 60;
    } else if (strcmp(cmd, "click") == 0 || strcmp(cmd, "rclick") == 0) {
        if (n >= 2) {
            _mouse_set_position(a, b);
        }
        g_buttons = cmd[0] == 'r' ? MOUSE_STATE_RIGHT_BUTTON_DOWN : MOUSE_STATE_LEFT_BUTTON_DOWN;
        g_release_at = now + 120;
        g_wait_until = now + 300;
    } else if (strcmp(cmd, "down") == 0) {
        g_buttons = MOUSE_STATE_LEFT_BUTTON_DOWN;
        g_wait_until = now + 100;
    } else if (strcmp(cmd, "up") == 0) {
        g_buttons = 0;
        g_wait_until = now + 100;
    } else if (strcmp(cmd, "wait") == 0) {
        g_wait_until = now + a;
    } else if (strcmp(cmd, "shot") == 0) {
        g_shot_path = arg;
        shoot_dump_screen(write_bmp);
        out("shot %s", arg);
    } else if (strcmp(cmd, "shotsurf") == 0) {
        // What is on the screen surface, including movies (dump_screen
        // only copies the windows).
        g_shot_path = arg;
        shot_surface();
        out("shot %s", arg);
    } else if (strcmp(cmd, "mvstat") == 0) {
        int frames = 0;
        int drops = 0;
        MVE_rmFrameCounts(&frames, &drops);
        out("movie frames %d dropped %d", frames, drops);
    } else if (strcmp(cmd, "state") == 0) {
        int mx;
        int my;
        mouseGetPosition(&mx, &my);
        out("map %s index %d elevation %d tile %d mouse %d %d game_state %d",
            gMapHeader.name, mapGetCurrentMap(), gElevation, gDude != NULL ? gDude->tile : -1, mx, my, gameGetState());
    } else if (strcmp(cmd, "critters") == 0) {
        for (Object* obj = objectFindFirstAtElevation(gElevation); obj != NULL; obj = objectFindNextAtElevation()) {
            if (FID_TYPE(obj->fid) != OBJ_TYPE_CRITTER) {
                continue;
            }
            int x;
            int y;
            tileToScreenXY(obj->tile, &x, &y, gElevation);
            char* nm = objectGetName(obj);
            out("%s\ttile %d\tscreen %d %d\tdist %d%s", nm != NULL ? nm : "?", obj->tile, x + 16, y + 8,
                objectGetDistanceBetween(gDude, obj), obj == gDude ? "\t(player)" : "");
        }
    } else if (strcmp(cmd, "map") == 0) {
        char name[64];
        int elev = 0;
        int tile = -1;
        name[0] = '\0';
        sscanf(arg, "%63s %d %d", name, &elev, &tile);
        int index = wmMapMatchNameToIdx(name);
        // "map WORLD" opens the world map (-2 in MapTransition).
        bool special = false;
        if (strcmp(name, "world") == 0) { // the lookup above lowercases the name
            index = -2;
            special = true;
        }
        if (index == -1 && !special) {
            out("map not found: %s", name);
        } else {
            MapTransition t;
            t.map = index;
            t.elevation = elev;
            t.tile = tile;
            t.rotation = 0;
            mapSetTransition(&t);
            out("map %s index %d", name, index);
            g_wait_until = now + 1500;
        }
    } else if (strcmp(cmd, "tp") == 0) {
        Rect rect;
        objectSetLocation(gDude, a, gElevation, &rect);
        tileSetCenter(a, TILE_SET_CENTER_REFRESH_WINDOW);
        g_wait_until = now + 200;
    } else if (strcmp(cmd, "talk") == 0) {
        Object* target = find_critter(arg);
        if (target == NULL) {
            out("critter not found: %s", arg);
        } else {
            actionTalk(gDude, target);
            out("talk %s tile %d", arg, target->tile);
            g_wait_until = now + 500;
        }
    } else if (strcmp(cmd, "pip") == 0) {
        // The Pip-Boy is worn once the vault suit movie has been seen.
        shoot_movie_seen(MOVIE_VSUIT);
        out("pip");
    } else if (strcmp(cmd, "xp") == 0) {
        // Experience for the player (level up, perks).
        int gained = 0;
        pcAddExperience(atoi(arg), &gained);
        out("xp %d level %d", gained, pcGetStat(PC_STAT_LEVEL));
    } else if (strcmp(cmd, "give") == 0 || strcmp(cmd, "wield") == 0) {
        // give <pid> [count]: an item for the player. wield <pid> [hand]: in a hand (1 = right).
        int pid = atoi(arg);
        const char* rest = strchr(arg, ' ');
        int n = rest != NULL ? atoi(rest + 1) : (cmd[0] == 'g' ? 1 : 1);
        Object* item = NULL;
        if (objectCreateWithPid(&item, pid) == -1 || item == NULL) {
            out("no item %d", pid);
        } else if (cmd[0] == 'g') {
            int rc = itemAdd(gDude, item, n);
            out("give %d x%d rc %d", pid, n, rc);
        } else {
            int rc = itemAdd(gDude, item, 1);
            if (rc == 0) {
                // Only the hand flag: the player's art may lack the weapon's frames.
                item->flags |= n != 0 ? OBJECT_IN_RIGHT_HAND : OBJECT_IN_LEFT_HAND;
            }
            out("wield %d hand %d rc %d", pid, n, rc);
        }
    } else if (strcmp(cmd, "die") == 0) {
        // The player dies: the game over screen follows.
        critterKill(gDude, -1, true);
        out("die");
        g_wait_until = now + 500;
    } else if (strcmp(cmd, "quit") == 0) {
        out("quit");
        finish();
        exit(0);
    } else {
        out("unknown command: %s", line.c_str());
    }
}

#ifdef _WIN32
// On a crash, write the call stack (function, file, line) to shoot/crash.txt.
static LONG WINAPI crash_filter(EXCEPTION_POINTERS* ep)
{
    std::string p = path_of("crash.txt");
    FILE* f = fopen(p.c_str(), "w");
    if (f == NULL) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    HANDLE proc = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    SymInitialize(proc, NULL, TRUE);
    fprintf(f, "exception %08lx at %p\n", ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress);

    CONTEXT ctx = *ep->ContextRecord;
    STACKFRAME64 sf;
    memset(&sf, 0, sizeof(sf));
    sf.AddrPC.Offset = ctx.Eip;
    sf.AddrPC.Mode = AddrModeFlat;
    sf.AddrFrame.Offset = ctx.Ebp;
    sf.AddrFrame.Mode = AddrModeFlat;
    sf.AddrStack.Offset = ctx.Esp;
    sf.AddrStack.Mode = AddrModeFlat;
    for (int i = 0; i < 40; i++) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_I386, proc, thread, &sf, &ctx, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
            break;
        }
        char buf[sizeof(SYMBOL_INFO) + 256];
        SYMBOL_INFO* sym = (SYMBOL_INFO*)buf;
        memset(buf, 0, sizeof(buf));
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 255;
        DWORD64 disp = 0;
        const char* name = SymFromAddr(proc, sf.AddrPC.Offset, &disp, sym) ? sym->Name : "?";
        IMAGEHLP_LINE64 line;
        memset(&line, 0, sizeof(line));
        line.SizeOfStruct = sizeof(line);
        DWORD disp2 = 0;
        if (SymGetLineFromAddr64(proc, sf.AddrPC.Offset, &disp2, &line)) {
            fprintf(f, "%s  %s:%lu\n", name, line.FileName, line.LineNumber);
        } else {
            fprintf(f, "%s\n", name);
        }
    }
    fclose(f);
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

void shoot_tick()
{
    // Commands such as "shot" redraw windows and may come back here.
    if (g_in_tick) {
        return;
    }
    g_in_tick = true;

#ifdef _WIN32
    static bool installed = false;
    if (!installed) {
        installed = true;
        SetUnhandledExceptionFilter(crash_filter);
    }
#endif

    unsigned int now = SDL_GetTicks();

    if (g_release_at != 0 && now >= (unsigned int)g_release_at) {
        g_buttons = 0;
        g_release_at = 0;
    }

    // The simulated mouse replaces the real one.
    _mouse_simulate_input(0, 0, g_buttons);

    if (!g_busy && now >= g_next_poll) {
        g_next_poll = now + 50;
        if (load_commands()) {
            g_busy = true;
            g_wait_until = 0;
        }
    }

    while (g_busy && now >= g_wait_until && g_release_at == 0) {
        if (g_pos >= g_queue.size()) {
            finish();
            break;
        }
        run(g_queue[g_pos++]);
        now = SDL_GetTicks();
    }

    g_in_tick = false;
}

} // namespace fallout

#endif /* NTG_SHOOT */
