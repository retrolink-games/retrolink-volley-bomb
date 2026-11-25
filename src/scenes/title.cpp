// Title: "LINK VOLLEY" over an attract-mode rally (two AIs, the practice layout seen from P1's screen).
#include "scenes/scenes.h"

#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_vector.h"

#include "render/court_view.h"
#include "render/text.h"
#include "sound/sfx.h"

namespace scenes
{

id title()
{
    sim::state s;
    sim::init(s, 1, 0xC0FFEE);
    court_view view(0, false);

    bn::vector<bn::sprite_ptr, 24> logo;
    bn::vector<bn::sprite_ptr, 24> sub;
    bn::vector<bn::sprite_ptr, 16> press;
    bn::sprite_text_generator& top = text::small(text::ink::SAND);
    top.set_center_alignment();
    top.generate(0, -66, "RETRO LINK", logo);
    text::big(text::ink::P1).generate(0, -40, "VOLLEY", logo);
    text::big(text::ink::P2).generate(0, -12, "BOMB", logo);

    text::tiny(text::ink::SAND).generate(0, 10, "1-4 PLAYERS  /  LINK CABLE", sub);

    for(int frame = 0; ; ++frame)
    {
        uint8_t in[sim::MAX_PLAYERS] = {};
        in[0] = sim::ai_input(s, 0);
        in[1] = sim::ai_input(s, 1);
        sim::step(s, in);

        if(s.phase == sim::PHASE_GAME_OVER)
        {
            sim::init(s, 1, s.rng);
        }

        view.update(s);

        if((frame & 31) == 0)
        {
            press.clear();
        }
        else if((frame & 31) == 12)
        {
            bn::sprite_text_generator& pg = text::small(text::ink::CREAM);
            pg.set_center_alignment();
            pg.generate(0, 28, "PRESS START", press);
        }

#ifdef LV_AUTOPLAY
        if(frame == 90)
        {
            return id::LOBBY;
        }
#endif

        if(bn::keypad::start_pressed() || bn::keypad::a_pressed())
        {
            sfx::hit();
            return id::LOBBY;
        }

        sfx::update();
        bn::core::update();
    }
}

}
