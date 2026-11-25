// Lobby: shows who is plugged in. Player 1 (the host, LinkCable id 0) presses Start for everyone.
#include "scenes/scenes.h"

#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_regular_bg_ptr.h"
#include "bn_regular_bg_items_bg_p1.h"
#include "bn_vector.h"
#include "bn_sprite_ptr.h"

#include "net/link_io.h"
#include "net/lockstep.h"
#include "render/text.h"
#include "sound/sfx.h"

namespace scenes
{

id lobby(game_setup& setup)
{
    link_io::activate();
    link_io::drain();

    bn::regular_bg_ptr bg = bn::regular_bg_items::bg_p1.create_bg(8, 48);
    bn::vector<bn::sprite_ptr, 16> heading;
    bn::vector<bn::sprite_ptr, 16> slots;
    bn::vector<bn::sprite_ptr, 32> info;
    bn::vector<bn::sprite_ptr, 32> help;
    text::big(text::ink::SAND).generate(0, -56, "LOBBY", heading);

    int shown_count = -1;
    int stable = 0;
    int stable_count = -1;
    int shown_me = -1;
    uint32_t ticks = 0;

    while(true)
    {
        ++ticks;
        link_io::sync();
        int count = link_io::player_count();
        int me = link_io::my_id();

        // Player 0's GO packet starts the match on every console.
        for(int p = 0; p < 4; ++p)
        {
            while(link_io::can_read(p))
            {
                uint16_t pkt = link_io::read(p);

                if(p == 0 && me != 0 && net::is_go_packet(pkt))
                {
                    setup.practice = false;
                    setup.my_id = me;
                    setup.player_count = net::go_players(pkt);
                    setup.seed = uint32_t(net::go_seed(pkt)) * 2654435761u + 1;
                    return id::GAME;
                }
            }
        }

        if(count != shown_count || me != shown_me)
        {
            shown_count = count;
            shown_me = me;
            slots.clear();
            info.clear();
            help.clear();

            // The four screens as they sit on the table: P3 P4 on top, P1 P2 below.
            constexpr int xs[4] = { -36, -36, 36, 36 };   // P2 P4 / P1 P3, like the consoles on the table
            constexpr int ys[4] = { 2, -22, 2, -22 };

            for(int slot = 0; slot < 4; ++slot)
            {
                char label[3] = { 'P', char('1' + slot), 0 };
                bool on = slot < count;
                bn::sprite_text_generator& g = text::small(on ? text::player_ink(slot) : text::ink::DIM);
                g.set_center_alignment();
                g.generate(xs[slot], ys[slot], on ? label : "--", slots);
            }

            char you[] = "YOU ARE P1";
            you[9] = char('1' + me);
            bn::sprite_text_generator& gi = text::small(text::player_ink(me));
            gi.set_center_alignment();
            gi.generate(0, 30, count > 1 ? you : "PRACTICE", info);

            text::tiny(text::ink::CREAM).generate(0, 54, me == 0 ? (count > 1 ? "START: PLAY" : "NO LINK - START: PLAY VS CPU") : "WAITING FOR P1 TO START", help);
        }

        if(bn::keypad::b_pressed())
        {
            return id::TITLE;
        }

        stable = count == stable_count ? stable + 1 : 0;
        stable_count = count;
        bool start = bn::keypad::start_pressed();

#ifdef LV_AUTOPLAY
        // Autoplay build: the host starts once the cable has reported the same player count for 3 s
        // (practice after 10 s alone).
        start |= (count > 1 && stable == 180) || (count <= 1 && stable == 600);
#endif

        if(me == 0 && start)
        {
            sfx::hit();
            setup.my_id = 0;

            if(count <= 1)
            {
                setup.practice = true;
                setup.player_count = 1;
                setup.seed = ticks * 2654435761u + 7;
                return id::GAME;
            }

            uint8_t seed8 = uint8_t(ticks | 1);

            while(! link_io::can_send())
            {
                bn::core::update();
            }

            link_io::send(net::make_go(count, seed8));
            setup.practice = false;
            setup.player_count = count;
            setup.seed = uint32_t(seed8) * 2654435761u + 1;
            return id::GAME;
        }

        sfx::update();
        bn::core::update();
    }
}

}
