/* Mods, in the layout emulators use, so RomFS mods made for them work as they are:
     <mods>/<name>/romfs/...          files that replace (or add to) the game's RomFS
   <mods> is the "mods" folder beside the executable, or HOS_MODS=<dir>. A mod may sit deeper, as emulators keep them
   (<mods>/contents/<title id>/<name>/romfs): any folder holding a romfs or exefs folder is a mod. Folders whose names
   start with '-' or '_' are left out (off). When two mods have the same file, the one whose path sorts last wins (name
   them 01-..., 02-... to order them).
   Every mod file is indexed once at the start (no disk lookups per open); nn_fs.c asks hos_mods_romfs for each rom:
   path and lists a folder's mod-only entries with the game's own (hos_mods_dir_extras). HOS_FS_OVERLAY (nn_fs.c) is
   looked at after this, so a test overlay wins over mods.
   exefs patches (.pchtxt, .ips) are not applied: the code is compiled ahead of time from the original instructions, and
   the game folder has no module map (image.json) to place data patches with. A mod's exefs folder is logged as left out.
   From Skybound's loader (NotRexed/SkyboundTotk a64-kit/hos/mods.c, MIT): its RomFS part. */
#include "hos.h"
#include "hos_paths.h"
#include "button_icons.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct { char *key; char *host; uint64_t size; int is_dir, logged; } Entry;   /* key: romfs path, lower case, '/' */
static Entry *table;
static size_t table_cap, table_n;

static uint64_t hash_str(const char *s) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (; *s; s++) h = (h ^ (uint8_t)*s) * 0x100000001b3ull;
    return h;
}
static void lower_slash(char *s) {
    for (; *s; s++) *s = *s == '\\' ? '/' : (char)tolower((unsigned char)*s);
}
static const char *last_sep(const char *s) {   /* (a host path: '\' on Windows, '/' from host/posix) */
    const char *b = strrchr(s, '\\'), *f = strrchr(s, '/');
    return b > f ? b : f;
}
static Entry *find(const char *key) {
    if (!table_cap) return NULL;
    for (size_t i = hash_str(key) & (table_cap - 1);; i = (i + 1) & (table_cap - 1)) {
        if (!table[i].key) return NULL;
        if (!strcmp(table[i].key, key)) return &table[i];
    }
}
static void put(const char *key, const char *host, uint64_t size, int is_dir) {
    if ((table_n + 1) * 2 > table_cap) {   /* (grow: rehash) */
        Entry *old = table;
        const size_t old_cap = table_cap;
        table_cap = table_cap ? table_cap * 2 : 1024;
        table = (Entry *)calloc(table_cap, sizeof *table);
        table_n = 0;
        for (size_t i = 0; i < old_cap; i++)
            if (old[i].key) {
                size_t j = hash_str(old[i].key) & (table_cap - 1);
                while (table[j].key) j = (j + 1) & (table_cap - 1);
                table[j] = old[i];
                table_n++;
            }
        free(old);
    }
    size_t i = hash_str(key) & (table_cap - 1);
    while (table[i].key && strcmp(table[i].key, key)) i = (i + 1) & (table_cap - 1);
    if (!table[i].key) table[i].key = _strdup(key), table_n++;
    else free(table[i].host);
    table[i].host = _strdup(host);
    table[i].size = size;
    table[i].is_dir = is_dir;
}

/* a mod's romfs folder, recursively: rel is the path inside the RomFS so far ("" at its root) */
static int index_romfs(const char *host_dir, const char *rel, int depth) {
    if (depth > 32) return 0;   /* (hookshot: no RomFS is this deep) */
    if (!depth) {   /* (hookshot) the romfs folder itself a link (to / in a mod's archive): the walk covered the whole disk */
        const DWORD at = GetFileAttributesA(host_dir);
        if (at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_REPARSE_POINT)) {
            hos_log("mods: %s is a link to a folder: left out", host_dir);
            return 0;
        }
    }
    char pat[MAX_PATH];
    snprintf(pat, sizeof pat, "%s\\*", host_dir);
    WIN32_FIND_DATAA fd;
    HANDLE f = FindFirstFileA(pat, &fd);
    if (f == INVALID_HANDLE_VALUE) return 0;
    int files = 0;
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        char host[MAX_PATH], key[MAX_PATH];
        snprintf(host, sizeof host, "%s\\%s", host_dir, fd.cFileName);
        /* (hookshot) a link to a folder is not followed (two links back up made the walk endless); links to files are
           (mod managers place files as links) */
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            hos_log("mods: %s is a link to a folder: left out", host);
            continue;
        }
        snprintf(key, sizeof key, "%s%s%s", rel, *rel ? "/" : "", fd.cFileName);
        lower_slash(key);
        const int is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        put(key, host, ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow, is_dir);
        if (is_dir) {
            char sub_rel[MAX_PATH];
            snprintf(sub_rel, sizeof sub_rel, "%s%s%s", rel, *rel ? "/" : "", fd.cFileName);
            files += index_romfs(host, sub_rel, depth + 1);
        } else {
            files++;
        }
    } while (FindNextFileA(f, &fd));
    FindClose(f);
    return files;
}

/* the mod folders under root, sorted (a mod: a folder holding romfs or exefs) */
typedef struct { char path[MAX_PATH]; char name[MAX_PATH]; } Mod;
static Mod *mods;
static int nmods, cap_mods;
static void find_mods(const char *dir, const char *name, int depth) {
    char probe[MAX_PATH];
    snprintf(probe, sizeof probe, "%s\\romfs", dir);
    const int has_romfs = GetFileAttributesA(probe) != INVALID_FILE_ATTRIBUTES;
    snprintf(probe, sizeof probe, "%s\\exefs", dir);
    const int has_exefs = GetFileAttributesA(probe) != INVALID_FILE_ATTRIBUTES;
    if ((has_romfs || has_exefs) && depth > 0) {
        if (nmods == cap_mods) mods = (Mod *)realloc(mods, sizeof *mods * (size_t)(cap_mods = cap_mods ? cap_mods * 2 : 16));
        snprintf(mods[nmods].path, MAX_PATH, "%s", dir);
        snprintf(mods[nmods].name, MAX_PATH, "%s", name);
        nmods++;
        return;
    }
    if (depth >= 4) return;
    char pat[MAX_PATH];
    snprintf(pat, sizeof pat, "%s\\*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE f = FindFirstFileA(pat, &fd);
    if (f == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.') continue;
        if (fd.cFileName[0] == '-' || fd.cFileName[0] == '_') {
            hos_log("mods: %s\\%s is off (its name starts with %c)", dir, fd.cFileName, fd.cFileName[0]);
            continue;
        }
        char sub[MAX_PATH], sub_name[MAX_PATH];
        snprintf(sub, sizeof sub, "%s\\%s", dir, fd.cFileName);
        snprintf(sub_name, sizeof sub_name, "%s%s%s", name, *name ? "/" : "", fd.cFileName);
        find_mods(sub, sub_name, depth + 1);
    } while (FindNextFileA(f, &fd));
    FindClose(f);
}
static int mod_cmp(const void *a, const void *b) { return _stricmp(((const Mod *)a)->name, ((const Mod *)b)->name); }

/* (hookshot) a file beside the game (name: "hookshot_options.ini", "hookshot_pad.txt"); 0 when the game's path is not known */
static int beside_game(char *out, size_t cap, const char *name) {
    hos_game_path_a(out, (DWORD)cap);
    char *slash = (char *)last_sep(out);
    if (!slash) return 0;
    snprintf(slash + 1, cap - (size_t)(slash + 1 - out), "%s", name);
    return 1;
}
/* the first line of a small file beside the game whose start is key (key "" : the file's first line), or "" */
static void read_beside(const char *name, const char *key, char *out, size_t cap) {
    *out = 0;
    char path[MAX_PATH], line[256];
    if (!beside_game(path, sizeof path, name)) return;
    FILE *f = fopen(path, "r");
    if (!f) return;
    const size_t kn = strlen(key);
    while (fgets(line, sizeof line, f))
        if (!strncmp(line, key, kn)) {
            line[strcspn(line, "\r\n")] = 0;
            snprintf(out, cap, "%s", line + kn);
            break;
        }
    fclose(f);
}

/* (hookshot) host/pad_sdl.c, as a controller opens: its kind kept in hookshot_pad.txt for Button icons' Auto at the
   next start (button_icons.h). Written only when it changes. */
void hos_mods_pad_seen(int sdl_version, int type) {
    const int kind = button_icons_pad_kind(sdl_version, type);
    static int known = -1;   /* (the polling thread's own) */
    if (!kind || kind == known) return;
    if (known < 0) {
        char was[32];
        read_beside("hookshot_pad.txt", "", was, sizeof was);
        known = button_icons_from_name(was);
        if (kind == known) return;
    }
    known = kind;
    char path[MAX_PATH];
    if (!beside_game(path, sizeof path, "hookshot_pad.txt")) return;
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%s\n", button_icons_names[kind]);
    fclose(f);
}

/* (hookshot) Button icons (button_icons.h): with ButtonPrompts-* sets installed, only the chosen one stays a mod */
static void pick_button_icons(void) {
    unsigned installed = 0;
    int last = 0;
    for (int i = 0; i < nmods; i++) {
        const int k = button_icons_mod_kind(mods[i].name);
        if (k) installed |= 1u << k, last = k;
    }
    if (!installed) return;
    char v[32];
    int want = ICONS_AUTO;
    if (getenv("HOS_BUTTON_ICONS")) {
        want = button_icons_from_name(getenv("HOS_BUTTON_ICONS"));
    } else {
        read_beside("hookshot_options.ini", "button_icons=", v, sizeof v);
        if (*v) want = button_icons_from_name(v);
    }
    if (want < 0) want = ICONS_AUTO;
    int seen = 0;
    const char *deck = getenv("SteamDeck");   /* (Steam sets it on the Steam Deck, also for a game under Proton) */
    if (deck && !strcmp(deck, "1")) {
        seen = ICONS_STEAMDECK;
    } else {
        read_beside("hookshot_pad.txt", "", v, sizeof v);
        seen = button_icons_from_name(v);
        if (seen <= ICONS_AUTO) seen = 0;
    }
    const int pick = button_icons_pick(want, installed, last, seen);
    if (want > ICONS_SWITCH && pick != want)
        hos_log("mods: Button icons: %s is not installed (a mods\\ButtonPrompts-... folder): the game's own icons",
                button_icons_names[want]);
    hos_log("mods: Button icons (%s%s%s): %s", button_icons_names[want], want == ICONS_AUTO && seen ? ", controller: " : "",
            want == ICONS_AUTO && seen ? button_icons_names[seen] : "", pick ? button_icons_names[pick] : "the game's own");
    int n = 0;
    for (int i = 0; i < nmods; i++) {
        const int k = button_icons_mod_kind(mods[i].name);
        if (k && k != pick) {
            hos_log("mods: %s: left out (Button icons: %s)", mods[i].name, pick ? button_icons_names[pick] : "switch");
            continue;
        }
        mods[n++] = mods[i];
    }
    nmods = n;
}

/* hos_main.c, once the image is loaded (image_path: kept for exefs patches, which are not applied here) */
void hos_mods_init(const char *image_path) {
    (void)image_path;
    char root[MAX_PATH];
    if (getenv("HOS_MODS")) {
        snprintf(root, sizeof root, "%s", getenv("HOS_MODS"));
    } else {
        hos_game_path_a(root, sizeof root);
        char *slash = (char *)last_sep(root);
        if (!slash) return;
        snprintf(slash + 1, sizeof root - (size_t)(slash + 1 - root), "mods");
    }
    if (GetFileAttributesA(root) == INVALID_FILE_ATTRIBUTES) return;
    find_mods(root, "", 0);
    if (!nmods) {
        hos_log("mods: none in %s (a mod: <name>\\romfs)", root);
        return;
    }
    qsort(mods, (size_t)nmods, sizeof *mods, mod_cmp);
    pick_button_icons();
    for (int i = 0; i < nmods; i++) {
        char dir[MAX_PATH];
        snprintf(dir, sizeof dir, "%s\\romfs", mods[i].path);
        const int files = index_romfs(dir, "", 0);
        snprintf(dir, sizeof dir, "%s\\exefs", mods[i].path);
        if (GetFileAttributesA(dir) != INVALID_FILE_ATTRIBUTES)
            hos_log("mods: %s: exefs patches are left out (the code is compiled ahead of time)", mods[i].name);
        hos_log("mods: %s: %d RomFS files", mods[i].name, files);
    }
    hos_log("mods: %d mods from %s: %zu RomFS entries", nmods, root, table_n);
}

/* nn_fs.c: the mod file for a RomFS path ("UI/x.zs", no mount, no leading slash), or 0 */
int hos_mods_romfs(const char *rel, char *out, size_t cap) {
    if (!table_n) return 0;
    char key[MAX_PATH];
    snprintf(key, sizeof key, "%s", rel);
    lower_slash(key);
    Entry *e = find(key);
    if (!e || e->is_dir) return 0;
    snprintf(out, cap, "%s", e->host);
    static volatile LONG logged;
    if (!e->logged && InterlockedIncrement(&logged) <= 200) hos_log("mods: %s <- %s", rel, e->host);
    e->logged = 1;
    return 1;
}

/* nn_fs.c, listing a RomFS folder: fn(arg, name, is_dir, size) for each mod entry directly in it that the game's own
   folder (base_dir, a host path; may not exist) does not have. Returns how many. */
int hos_mods_dir_extras(const char *rel, const char *base_dir, void (*fn)(void *, const char *, int, uint64_t),
                        void *arg) {
    if (!table_n) return 0;
    char prefix[MAX_PATH];
    snprintf(prefix, sizeof prefix, "%s", rel);
    lower_slash(prefix);
    size_t pn = strlen(prefix);
    while (pn && prefix[pn - 1] == '/') prefix[--pn] = 0;
    int n = 0;
    for (size_t i = 0; i < table_cap; i++) {
        const Entry *e = &table[i];
        if (!e->key) continue;
        const char *k = e->key;
        if (pn && (strncmp(k, prefix, pn) || k[pn] != '/')) continue;
        const char *name = pn ? k + pn + 1 : k;
        if (!*name || strchr(name, '/')) continue;   /* (deeper) */
        const char *real = last_sep(e->host);   /* (the name as the mod spells it) */
        real = real ? real + 1 : e->host;
        char base[MAX_PATH];
        snprintf(base, sizeof base, "%s\\%s", base_dir, real);
        if (GetFileAttributesA(base) != INVALID_FILE_ATTRIBUTES) continue;
        fn(arg, real, e->is_dir, e->size);
        n++;
    }
    return n;
}
