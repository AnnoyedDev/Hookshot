/* host/button_icons.h: which ButtonPrompts set loads (tests/test_button_icons.py compiles and runs this) */
#include "button_icons.h"
#include <stdio.h>

static int failed;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failed = 1; } } while (0)

int main(void) {
    /* the folders, as find_mods names them */
    CHECK(button_icons_mod_kind("ButtonPrompts-PlayStation") == ICONS_PLAYSTATION);
    CHECK(button_icons_mod_kind("buttonprompts-xbox") == ICONS_XBOX);
    CHECK(button_icons_mod_kind("contents/0400486024DEC000/ButtonPrompts-SteamDeck") == ICONS_STEAMDECK);
    CHECK(button_icons_mod_kind("02-ButtonPrompts-Xbox") == ICONS_XBOX);
    CHECK(button_icons_mod_kind("ButtonPrompts-Xbox-v1.2") == ICONS_XBOX);
    CHECK(button_icons_mod_kind("ButtonPrompts-XboxOne") == 0);   /* (not one of the three) */
    CHECK(button_icons_mod_kind("ButtonPrompts-PlayStation/romfs") == 0);   /* (only the folder's own name) */
    CHECK(button_icons_mod_kind("HD Textures") == 0);
    CHECK(button_icons_mod_kind("") == 0);

    /* names and the menu's numbers */
    CHECK(button_icons_from_name("PlayStation") == ICONS_PLAYSTATION);
    CHECK(button_icons_from_name("steamdeck\n") == ICONS_STEAMDECK);
    CHECK(button_icons_from_name("3") == ICONS_XBOX);
    CHECK(button_icons_from_name("0\r\n") == ICONS_AUTO);
    CHECK(button_icons_from_name("5") == -1);
    CHECK(button_icons_from_name("xboxes") == -1);
    CHECK(button_icons_from_name("") == -1);
    CHECK(button_icons_from_name(NULL) == -1);

    /* SDL's pad types */
    CHECK(button_icons_pad_kind(3, 6) == ICONS_PLAYSTATION);   /* (SDL3 PS5) */
    CHECK(button_icons_pad_kind(3, 3) == ICONS_XBOX);
    CHECK(button_icons_pad_kind(3, 7) == ICONS_SWITCH);
    CHECK(button_icons_pad_kind(3, 1) == 0);   /* (STANDARD: not known) */
    CHECK(button_icons_pad_kind(2, 7) == ICONS_PLAYSTATION);   /* (SDL2 PS5) */
    CHECK(button_icons_pad_kind(2, 5) == ICONS_SWITCH);
    CHECK(button_icons_pad_kind(2, 1) == ICONS_XBOX);
    CHECK(button_icons_pad_kind(2, 6) == 0);   /* (VIRTUAL) */
    CHECK(button_icons_pad_kind(0, 3) == 0);

    /* the pick */
    const unsigned ps = 1u << ICONS_PLAYSTATION, xb = 1u << ICONS_XBOX, sd = 1u << ICONS_STEAMDECK;
    CHECK(button_icons_pick(ICONS_AUTO, 0, 0, ICONS_XBOX) == 0);   /* (none installed) */
    CHECK(button_icons_pick(ICONS_AUTO, ps, ICONS_PLAYSTATION, 0) == ICONS_PLAYSTATION);   /* (one: always it) */
    CHECK(button_icons_pick(ICONS_AUTO, ps, ICONS_PLAYSTATION, ICONS_XBOX) == ICONS_PLAYSTATION);
    CHECK(button_icons_pick(ICONS_AUTO, ps, ICONS_PLAYSTATION, ICONS_SWITCH) == ICONS_PLAYSTATION);
    CHECK(button_icons_pick(ICONS_AUTO, ps | xb | sd, ICONS_XBOX, ICONS_PLAYSTATION) == ICONS_PLAYSTATION);
    CHECK(button_icons_pick(ICONS_AUTO, ps | xb | sd, ICONS_XBOX, ICONS_STEAMDECK) == ICONS_STEAMDECK);
    CHECK(button_icons_pick(ICONS_AUTO, ps | xb, ICONS_XBOX, ICONS_SWITCH) == 0);   /* (a Switch pad: its own icons) */
    CHECK(button_icons_pick(ICONS_AUTO, ps | xb, ICONS_XBOX, 0) == ICONS_XBOX);   /* (not known: as before, the last) */
    CHECK(button_icons_pick(ICONS_AUTO, ps | xb, ICONS_XBOX, ICONS_STEAMDECK) == ICONS_XBOX);   /* (not installed) */
    CHECK(button_icons_pick(ICONS_SWITCH, ps | xb, ICONS_XBOX, ICONS_XBOX) == 0);
    CHECK(button_icons_pick(ICONS_PLAYSTATION, ps | xb, ICONS_XBOX, ICONS_XBOX) == ICONS_PLAYSTATION);
    CHECK(button_icons_pick(ICONS_STEAMDECK, ps | xb, ICONS_XBOX, 0) == 0);   /* (chosen, not installed: the game's own) */
    CHECK(button_icons_pick(99, ps | xb, ICONS_XBOX, ICONS_PLAYSTATION) == ICONS_PLAYSTATION);   /* (a bad value: Auto) */

    puts(failed ? "button icons: FAILED" : "button icons: ok");
    return failed;
}
