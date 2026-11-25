// Link Volley: an original 1-4 player GBA link cable game.
#include "bn_core.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"

#include "net/link_io.h"
#include "scenes/scenes.h"
#include "sound/sfx.h"

int main()
{
    bn::core::init();
    bn::bg_palettes::set_transparent_color(bn::color(2, 2, 3));   // #121216
    link_io::init();
    sfx::init();

    scenes::id next = scenes::id::TITLE;
    scenes::game_setup setup;

    while(true)
    {
        switch(next)
        {
        case scenes::id::TITLE:
            next = scenes::title();
            break;
        case scenes::id::LOBBY:
            next = scenes::lobby(setup);
            break;
        default:
            next = scenes::game(setup);
            break;
        }

        // One frame between scenes: Butano frees the old scene's sprite tiles on update, and the key press
        // that ended a scene (e.g. Select+Start) must not also trigger the next one (title -> lobby -> game
        // in a single frame used to exhaust the sprite tiles items).
        bn::core::update();
    }
}
