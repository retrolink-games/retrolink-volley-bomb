// The match. Practice: local input + AI, no link. Linked: lockstep over LinkCable (inputs only, delay D,
// stall when anyone's input is missing, 4-bit hash slices checked every frame).
#include "scenes/scenes.h"

#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_sprite_ptr.h"
#include "bn_vector.h"

#include "net/link_io.h"
#include "net/lockstep.h"
#include "render/court_view.h"
#include "render/text.h"
#include "sim/sim.h"
#include "sound/sfx.h"

namespace scenes
{

namespace
{
    constexpr int STALL_GIVE_UP = 180;   // frames stuck waiting for input before Start can quit to the title

    uint8_t sample_keys(const sim::state& s, int slot)
    {
#ifdef LV_AUTOPLAY
        // Autoplay build: the AI plays this console's own slot; it also accepts rematches.
        uint8_t ai = sim::ai_input(s, slot);

        if(! bn::keypad::any_held())
        {
            return ai;
        }
#else
        (void) s;
#endif

        uint8_t b = 0;

        if(bn::keypad::left_held())
        {
            b |= sim::BTN_LEFT;
        }

        if(bn::keypad::right_held())
        {
            b |= sim::BTN_RIGHT;
        }

        if(bn::keypad::a_held())
        {
            b |= sim::BTN_A;
        }

        if(bn::keypad::b_held())
        {
            b |= sim::BTN_B;
        }

        if(bn::keypad::start_held())
        {
            b |= sim::BTN_START;
        }

        // P3/P4 see the world rotated 180 degrees: their screen-right is the world's left.
        uint8_t lr = b & (sim::BTN_LEFT | sim::BTN_RIGHT);

        if(sim::slot_is_top(slot) && (lr == sim::BTN_LEFT || lr == sim::BTN_RIGHT))
        {
            b ^= sim::BTN_LEFT | sim::BTN_RIGHT;
        }

        return b;
    }

    void slot_name(char* out, int slot)
    {
        out[0] = 'P';
        out[1] = char('1' + slot);
    }

    // Frozen screen for DESYNC / link errors. Start returns to the title.
    id freeze_screen(court_view& view, const net::lockstep& ls, bool desync)
    {
        // Line 2: the frame, and (desync) my full hash for it.
        char line2[40] = "FRAME 000000  HASH 00000000";
        text::dec(line2 + 6, ls.bad_frame % 1000000, 6);

        if(desync)
        {
            text::hex(line2 + 19, ls.my_hash, 8);
        }
        else
        {
            line2[12] = 0;
        }

        view.set_message(desync ? "DESYNC" : "LINK ERROR", line2, int(text::ink::RED));
        sfx::point();

        // Line 3: every player's 4-bit hash slice for that frame (or what went wrong).
        char line3[40] = {};
        int n = 0;

        if(desync)
        {
            for(int p = 0; p < ls.player_count(); ++p)
            {
                slot_name(line3 + n, p);
                n += 2;
                line3[n++] = ':';
                text::hex(line3 + n, ls.slices[p], 1);
                ++n;
                line3[n++] = ' ';
                line3[n++] = ' ';
            }
        }
        else
        {
            const char* msg = "LOST OR OUT-OF-ORDER PACKET";

            while(msg[n])
            {
                line3[n] = msg[n];
                ++n;
            }
        }

        line3[n] = 0;
        bn::vector<bn::sprite_ptr, 40> info;
        text::tiny(text::ink::SAND).generate(0, 26, line3, info);
        text::tiny(text::ink::DIM).generate(0, 44, "START: TITLE", info);

        while(true)
        {
            link_io::sync();
            link_io::drain();

            if(bn::keypad::start_pressed())
            {
                return id::TITLE;
            }

            sfx::update();
            bn::core::update();
        }
    }

    id link_lost(court_view& view)
    {
        // Waits for Start, so the player sees what happened instead of being dropped into a menu.
        view.set_message("LINK LOST", "START: TITLE", int(text::ink::RED));
        sfx::point();

        while(! bn::keypad::start_pressed())
        {
            link_io::sync();
            link_io::drain();
            sfx::update();
            bn::core::update();
        }

        return id::TITLE;
    }

    void status_message(court_view& view, const sim::state& s, int stall, bool practice, int my_slot_for_status)
    {
        if(s.phase == sim::PHASE_GAME_OVER)
        {
            // The big "Pn WINS!" title, hops and confetti are drawn by court_view.
            if(s.timer > 0)
            {
                view.set_message("", "", 0);   // wait for the title to land
                return;
            }

            if(practice)
            {
                view.set_message("", "A: REMATCH   SELECT+START: QUIT", 0);
                return;
            }

            // Who still has to press A for the rematch.
            char ready[32] = "A: REMATCH";

            if(! practice)
            {
                int n = 10;

                for(int slot = 0; slot < sim::MAX_PLAYERS; ++slot)
                {
                    if((s.occupied >> slot) & 1)
                    {
                        ready[n++] = ' ';
                        slot_name(ready + n, slot);
                        n += 2;
                        ready[n++] = ((s.rematch_mask >> slot) & 1) ? '+' : '.';
                    }
                }

                ready[n] = 0;
            }

            view.set_message("", ready, 0);
        }
        else if(s.starting)
        {
            // Match start: frozen until everyone has pressed A (so it doesn't look like someone paused).
            char ready[32] = "TO START";

            if(! practice)
            {
                int n = 8;

                for(int slot = 0; slot < sim::MAX_PLAYERS; ++slot)
                {
                    if((s.occupied >> slot) & 1)
                    {
                        ready[n++] = ' ';
                        slot_name(ready + n, slot);
                        n += 2;
                        ready[n++] = ((s.rematch_mask >> slot) & 1) ? '+' : '.';
                    }
                }

                ready[n] = 0;
            }

            view.set_message("PRESS A", ready, int(text::ink::SAND));
        }
        else if(! practice && ! ((s.alive >> my_slot_for_status) & 1))
        {
            view.set_message("BOOM! YOU'RE OUT", "WATCH THE OTHERS", int(text::ink::RED));
        }
        else if(s.paused)
        {
            view.set_message("PAUSED", "START: RESUME", int(text::ink::SAND));
        }
        else if(stall > 30)
        {
            view.set_message((stall & 16) ? "WAITING..." : "WAITING.", stall > STALL_GIVE_UP ? "START: TITLE" : "",
                             int(text::ink::DIM));
        }
        else
        {
            view.set_message("", "", 0);
        }
    }
}

id game(const game_setup& setup)
{
    sim::state s;
    sim::init(s, setup.player_count, setup.seed);
    int my_slot = setup.my_id;
    court_view view(my_slot, true);

    if(setup.practice)
    {
        view.set_cpu_slot(1);
    }

    if(setup.practice)
    {
        link_io::deactivate();

        while(true)
        {
            uint8_t in[sim::MAX_PLAYERS] = {};
            in[0] = sample_keys(s, 0);
            in[1] = sim::ai_input(s, 1);
            sfx::play_events(sim::step(s, in));
            view.update(s);
            status_message(view, s, 0, true, 0);

            if(bn::keypad::select_held() && bn::keypad::start_pressed())
            {
                return id::TITLE;
            }

            sfx::update();
            bn::core::update();
        }
    }

    net::lockstep ls;
    ls.start(setup.my_id, setup.player_count);
    int stall = 0;
    int link_bad = 0;

#ifdef LV_DEBUG_DESYNC
    bn::vector<bn::sprite_ptr, 8> debug_tag;
    text::small(text::ink::RED).generate(-114, -52, "DEBUG", debug_tag);
#endif

    while(true)
    {
        link_io::sync();

        // Disconnect: the cable reports fewer players (or none) for a while.
        if(! link_io::connected() || link_io::player_count() != setup.player_count)
        {
            if(++link_bad > 20)
            {
                return link_lost(view);
            }
        }
        else
        {
            link_bad = 0;
        }

        // Host first, so a GO packet is always handled before game packets from the same transfer.
        for(int p = 0; p < setup.player_count; ++p)
        {
            while(link_io::can_read(p))
            {
                ls.receive(p, link_io::read(p));
            }
        }

        if(ls.state() == net::status::SEQUENCE_ERROR)
        {
            return freeze_screen(view, ls, false);
        }

        if(ls.ready() && link_io::can_send())
        {
            stall = 0;
            link_io::send(ls.local_input(sample_keys(s, my_slot)));

            uint8_t in[sim::MAX_PLAYERS];
            ls.inputs(in);
            sfx::play_events(sim::step(s, in));

#ifdef LV_DEBUG_DESYNC
            // Debug build only: Select + L nudges the ball on this console alone -> DESYNC within a few frames.
            if(bn::keypad::select_held() && bn::keypad::l_pressed())
            {
                s.ball.x += 1;
            }
#endif

            ls.finish_frame(sim::hash(s));

            if(ls.state() == net::status::DESYNC)
            {
                view.update(s);
                return freeze_screen(view, ls, true);
            }
        }
        else
        {
            ++stall;   // re-render the previous frame

            // Stuck waiting for 3 s (a console froze without the cable noticing): Start gives up.
            if(stall > STALL_GIVE_UP && bn::keypad::start_pressed())
            {
                return id::TITLE;
            }
        }

        view.update(s);
        status_message(view, s, stall, false, my_slot);
        sfx::update();
        bn::core::update();
    }
}

}
