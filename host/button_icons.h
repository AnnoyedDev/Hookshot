/* Button icons (the menu's Gameplay > Button icons row, button_icons= in hookshot_options.ini; HOS_BUTTON_ICONS):
   which of the ButtonPrompts-PlayStation / -Xbox / -SteamDeck RomFS mods in the mods folder the game loads
   (github.com/exgde/oot-switch2-button-prompts: the player downloads them; nothing of theirs ships with Hookshot).
   With more than one installed, all of them loaded and the one sorting last won (Xbox over PlayStation): now one set is
   picked and the others are left out, so they can all stay installed. They load at the start: a change applies after a
   restart. Auto: the only set installed; else the set of the controller used last (host/pad_sdl.c remembers its kind in
   hookshot_pad.txt beside the game; the Steam Deck by its SteamDeck=1); else the one sorting last, as before.
   Pure functions: host/mods.c and tests/button_icons_test.c use them. */
#ifndef HOOKSHOT_BUTTON_ICONS_H
#define HOOKSHOT_BUTTON_ICONS_H
#include <ctype.h>
#include <string.h>

/* the row's values (and a pad's kind, without ICONS_AUTO) */
enum { ICONS_AUTO, ICONS_SWITCH, ICONS_PLAYSTATION, ICONS_XBOX, ICONS_STEAMDECK, ICONS_N };
static const char *const button_icons_names[ICONS_N] = {"auto", "switch", "playstation", "xbox", "steamdeck"};

/* a name (any case) as its value, or -1 */
static inline int button_icons_from_name(const char *s) {
    if (!s) return -1;
    for (int i = 0; i < ICONS_N; i++) {
        const char *a = s, *b = button_icons_names[i];
        while (*a && *b && tolower((unsigned char)*a) == *b) a++, b++;
        if (!*b && (!*a || *a == '\r' || *a == '\n' || *a == ' ')) return i;
    }
    if (s[0] >= '0' && s[0] < '0' + ICONS_N && (!s[1] || s[1] == '\r' || s[1] == '\n')) return s[0] - '0';
    return -1;
}

/* the set a mod folder is ("ButtonPrompts-Xbox", also under contents/<title id>/, or "02-ButtonPrompts-Xbox"), or 0 */
static inline int button_icons_mod_kind(const char *mod_name) {
    const char *last = mod_name;
    for (const char *p = mod_name; *p; p++)
        if (*p == '/' || *p == '\\') last = p + 1;
    static const char mark[] = "buttonprompts-";
    for (const char *p = last; *p; p++) {
        size_t i = 0;
        while (mark[i] && tolower((unsigned char)p[i]) == mark[i]) i++;
        if (mark[i]) continue;
        const char *k = p + i;
        static const struct { const char *name; int kind; } sets[] = {
            {"playstation", ICONS_PLAYSTATION}, {"xbox", ICONS_XBOX}, {"steamdeck", ICONS_STEAMDECK}};
        for (size_t s = 0; s < sizeof sets / sizeof sets[0]; s++) {
            size_t j = 0;
            while (sets[s].name[j] && tolower((unsigned char)k[j]) == sets[s].name[j]) j++;
            if (!sets[s].name[j] && !isalnum((unsigned char)k[j])) return sets[s].kind;
        }
        return 0;
    }
    return 0;
}

/* a pad's kind from SDL_GetGamepadType (SDL3) / SDL_GameControllerGetType (SDL2), or 0 not known (a generic pad) */
static inline int button_icons_pad_kind(int sdl_version, int type) {
    if (sdl_version == 3) {   /* (XBOX360 2, XBOXONE 3, PS3 4, PS4 5, PS5 6, SWITCH_PRO 7, JOYCON_LEFT/RIGHT/PAIR 8-10) */
        if (type == 2 || type == 3) return ICONS_XBOX;
        if (type >= 4 && type <= 6) return ICONS_PLAYSTATION;
        if (type >= 7 && type <= 10) return ICONS_SWITCH;
    } else if (sdl_version == 2) {   /* (XBOX360 1, XBOXONE 2, PS3 3, PS4 4, SWITCH_PRO 5, PS5 7, JOYCON 11-13) */
        if (type == 1 || type == 2) return ICONS_XBOX;
        if (type == 3 || type == 4 || type == 7) return ICONS_PLAYSTATION;
        if (type == 5 || (type >= 11 && type <= 13)) return ICONS_SWITCH;
    }
    return 0;
}

/* the set to load: installed has bit k for each set installed, last the set of the installed mod sorting last; want the
   row's value; seen the controller's kind (0 not known). 0: none of them (the game's own Switch icons). */
static inline int button_icons_pick(int want, unsigned installed, int last, int seen) {
    if (!installed) return 0;
    if (want == ICONS_SWITCH) return 0;
    if (want > ICONS_SWITCH && want < ICONS_N) return installed & (1u << want) ? want : 0;
    if (!(installed & (installed - 1))) return last;   /* (Auto, one set installed: the player's choice) */
    if (seen == ICONS_SWITCH) return 0;
    if (seen > ICONS_SWITCH && seen < ICONS_N && (installed & (1u << seen))) return seen;
    return last;
}
#endif
