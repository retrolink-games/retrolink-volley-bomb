#include "render/court_view.h"

#include "bn_regular_bg_items_bg_p1.h"
#include "bn_regular_bg_items_bg_p2.h"
#include "bn_regular_bg_items_bg_p3.h"
#include "bn_regular_bg_items_bg_p4.h"
#include "bn_sprite_items_ball.h"
#include "bn_sprite_items_boom.h"
#include "bn_sprite_items_confetti.h"
#include "bn_sprite_items_smoke.h"
#include "bn_bg_palettes.h"
#include "bn_sprite_palettes.h"
#include "bn_sprite_items_map_dot.h"
#include "bn_sprite_items_minimap.h"
#include "bn_sprite_items_blob_p1.h"
#include "bn_sprite_items_blob_p2.h"
#include "bn_sprite_items_blob_p3.h"
#include "bn_sprite_items_blob_p4.h"

#include "render/text.h"

namespace
{
    const bn::regular_bg_item& bg_item(int slot)
    {
        switch(slot)
        {
        case 1: return bn::regular_bg_items::bg_p2;
        case 2: return bn::regular_bg_items::bg_p3;
        case 3: return bn::regular_bg_items::bg_p4;
        default: return bn::regular_bg_items::bg_p1;
        }
    }

    const bn::sprite_item& blob_item(int slot)
    {
        switch(slot)
        {
        case 1: return bn::sprite_items::blob_p2;
        case 2: return bn::sprite_items::blob_p3;
        case 3: return bn::sprite_items::blob_p4;
        default: return bn::sprite_items::blob_p1;
        }
    }

    // The 256x256 map's top-left 240x160 is the screen: offset its centre from the screen centre.
    constexpr int BG_X = 128 - 120;
    constexpr int BG_Y = 128 - 80;

    constexpr uint32_t VICTORY_DELAY = 44;   // frames after game over: the last bomb plays out first
}

court_view::court_view(int my_slot, bool show_hud) :
    _slot(my_slot),
    _left(sim::slot_left(my_slot)),
    _top(sim::slot_top(my_slot)),
    _show_hud(show_hud),
    _rotated(sim::slot_is_top(my_slot)),
    _bg(bg_item(my_slot).create_bg(BG_X, BG_Y)),
    _ball(bn::sprite_items::ball.create_sprite(0, 0, 0)),
    _trail{ bn::sprite_items::ball.create_sprite(0, 0, 1), bn::sprite_items::ball.create_sprite(0, 0, 2),
            bn::sprite_items::ball.create_sprite(0, 0, 3) },
    _boom(bn::sprite_items::boom.create_sprite(0, 0, 0))
{
    _boom.set_visible(false);
    _boom.set_z_order(-3);

    for(int i = 0; i < 8; ++i)
    {
        _chain.push_back(bn::sprite_items::boom.create_sprite(0, 0, 0));
        _chain.back().set_visible(false);
        _chain.back().set_z_order(-3);
    }
    _bg.set_priority(3);
    _ball.set_z_order(-2);

    for(bn::sprite_ptr& t : _trail)
    {
        t.set_visible(false);
    }
}

void court_view::place(bn::sprite_ptr& sprite, int world_x, int world_y, int half)
{
    int sx = world_x - _left;
    int sy = world_y - _top;

    if(_rotated)
    {
        sx = sim::SCREEN_W - 1 - sx;
        sy = sim::SCREEN_H - 1 - sy;
    }

    sx -= 120 - _shake_x;
    sy -= 80 - _shake_y;
    bool on_screen = sx > -120 - half && sx < 120 + half && sy > -80 - half && sy < 80 + half;
    sprite.set_visible(on_screen);

    if(on_screen)
    {
        sprite.set_position(sx, sy);
    }
}

court_view::~court_view()
{
    bn::bg_palettes::set_fade_intensity(0);
    bn::sprite_palettes::set_fade_intensity(0);
}

void court_view::update_bomb(const sim::state& s)
{
    // Did the bomb go off on my screen? (Its quarter, not who lost: covers the practice CPU too.)
    uint32_t since = s.frame - s.boom_frame;
    int boom_slot = (s.boom_x >= sim::SCREEN_W ? 2 : 0) + (s.boom_y < sim::SCREEN_H ? 1 : 0);
    bool active = s.boom_frame != 0 && since < 60;
    bool mine = active && boom_slot == _slot;

    // Shake: hard and decaying on the exploding screen, a small jolt on everyone else's.
    int amp = 0;

    if(mine && since < 36)
    {
        amp = 9 - int(since) / 4;
    }
    else if(active && since < 12)
    {
        amp = 2;
    }

    if(amp > 0)
    {
        int span = amp * 2 + 1;
        _shake_x = int((since * 37 + 11) % uint32_t(span)) - amp;
        _shake_y = int((since * 53 + 29) % uint32_t(span)) - amp;
    }
    else
    {
        _shake_x = 0;
        _shake_y = 0;
    }

    _bg.set_position(128 - 120 + _shake_x, 128 - 80 + _shake_y);

    // Flash: white, then a fading red glow.
    int key = 0;
    bn::color fade_colour(31, 31, 31);
    bn::fixed intensity = 0;

    if(mine && since < 4)
    {
        key = 1 + int(since);
        intensity = bn::fixed(1) - bn::fixed(int(since)) / 5;
    }
    else if(mine && since < 44)
    {
        key = 10 + int(since);
        fade_colour = bn::color(31, 5, 3);
        intensity = bn::fixed(44 - int(since)) / 64;
    }

    if(key != _fade_key)
    {
        _fade_key = key;
        bn::bg_palettes::set_fade(fade_colour, intensity);
        bn::sprite_palettes::set_fade(fade_colour, intensity);
    }

    // Big central fireball.
    _boom.set_scale(mine ? bn::fixed(2) : bn::fixed(1));

    // A chain of explosions marching across the screen from the floor up.
    constexpr int chain_x[8] = { -96, 70, -40, 100, 10, -78, 46, -10 };
    constexpr int chain_y[8] = { 58, 50, 30, 20, 44, 0, -6, -30 };
    constexpr int chain_delay[8] = { 2, 4, 7, 9, 11, 14, 17, 20 };

    for(int i = 0; i < 8; ++i)
    {
        int local = int(since) - chain_delay[i];
        bn::sprite_ptr& c = _chain[i];

        if(mine && local >= 0 && local < 24)
        {
            int frame = local / 6;
            c.set_position(chain_x[i] + _shake_x, chain_y[i] + _shake_y);
            c.set_visible(true);

            if(frame != _chain_frame[i])
            {
                c.set_tiles(bn::sprite_items::boom.tiles_item(), frame);
                _chain_frame[i] = frame;
            }
        }
        else
        {
            c.set_visible(false);
        }
    }
}

int court_view::winner_hop(const sim::state& s) const
{
    // Victory hops, 20 px high, every 40 frames once the title is down.
    if(! _win_active || s.winner >= sim::MAX_PLAYERS)
    {
        return 0;
    }

    uint32_t since = s.frame - _win_frame;

    if(since < VICTORY_DELAY)
    {
        return 0;
    }

    int t = int((since - VICTORY_DELAY) % 40);
    return t * (40 - t) / 20;
}

void court_view::update_victory(const sim::state& s)
{
    if(s.phase != sim::PHASE_GAME_OVER)
    {
        if(_win_active)
        {
            _win_active = false;
            _title1.clear();
            _title2.clear();
            _confetti.clear();
        }

        return;
    }

    if(! _win_active)
    {
        _win_active = true;
        _win_frame = s.frame;
    }

    uint32_t since = s.frame - _win_frame;

    if(since < VICTORY_DELAY)
    {
        return;   // let the last bomb finish
    }

    uint32_t t = since - VICTORY_DELAY;
    int winner = s.winner < sim::MAX_PLAYERS ? s.winner : 0;

    // Title: "P2" / "WINS!" in the big font, dropping in from above and slamming down.
    if(_title1.empty())
    {
        char name[4] = { 'P', char('1' + winner), 0, 0 };

        if(winner == _cpu_slot)
        {
            name[0] = 'C';
            name[1] = 'P';
            name[2] = 'U';
        }

        text::big(text::player_ink(winner)).generate(0, 0, name, _title1);
        text::big(text::ink::CREAM).generate(0, 0, "WINS!", _title2);

        for(int i = 0; i < 16; ++i)
        {
            int colour = (i % 3 == 2) ? 4 : winner;
            _confetti.push_back(bn::sprite_items::confetti.create_sprite(0, 0, colour * 3));
            _confetti.back().set_z_order(-4);
        }
    }

    constexpr int TITLE1_Y = -38;
    constexpr int TITLE2_Y = -6;
    constexpr int DROP = 20;   // frames to fall
    int drop = t < DROP ? int(DROP - t) * int(DROP - t) / 3 : 0;   // ease-in fall from up to 133 px above

    // Little bounce after the slam, then a gentle bob.
    constexpr int bounce[20] = { -3, -6, -8, -9, -10, -10, -9, -8, -6, -3, 0, -1, -2, -3, -3, -3, -2, -1, 0, 0 };
    int bob = 0;

    if(t >= DROP && t < DROP + 20)
    {
        bob = bounce[t - DROP];
    }
    else if(t >= DROP + 20)
    {
        constexpr int wave[16] = { 0, -1, -2, -2, -3, -2, -2, -1, 0, 1, 2, 2, 3, 2, 2, 1 };
        bob = wave[((t - DROP - 20) / 6) & 15] / 2;
    }

    auto set_line = [](bn::vector<bn::sprite_ptr, 8>& line, int y) {
        for(bn::sprite_ptr& sp : line)
        {
            sp.set_y(y);
        }
    };

    for(bn::sprite_ptr& sp : _title1)
    {
        sp.set_y(TITLE1_Y - drop + bob);
    }

    set_line(_title2, TITLE2_Y - drop * 2 + bob);   // second line lands a moment later

    // Slam: winner-coloured flash and a hard shake on every console.
    if(t >= DROP && t < DROP + 20)
    {
        int k = int(t - DROP);
        int amp = 6 - k / 4;
        int span = amp * 2 + 1;
        _shake_x = int((uint32_t(k) * 37 + 5) % uint32_t(span)) - amp;
        _shake_y = int((uint32_t(k) * 53 + 17) % uint32_t(span)) - amp;
        _bg.set_position(128 - 120 + _shake_x, 128 - 80 + _shake_y);

        constexpr bn::color flash[4] = { bn::color(12, 27, 25), bn::color(31, 17, 11), bn::color(30, 25, 17),
                                         bn::color(22, 19, 31) };
        int key = 200 + k;

        if(key != _fade_key)
        {
            _fade_key = key;
            bn::fixed intensity = k < 12 ? bn::fixed(12 - k) / 15 : bn::fixed(0);
            bn::bg_palettes::set_fade(flash[winner], intensity);
            bn::sprite_palettes::set_fade(flash[winner], intensity);
        }
    }

    // Confetti: screen-space, falls from the top with a sway, wrapping around forever.
    for(int i = 0; i < _confetti.size(); ++i)
    {
        bn::sprite_ptr& c = _confetti[i];
        int start = i * 5;   // staggered release

        if(int(t) < start)
        {
            c.set_visible(false);
            continue;
        }

        int ct = int(t) - start;
        int speed = 1 + (i * 7) % 3;                       // 1..3 px/frame (in halves below)
        int x0 = ((i * 97 + 31) % 232) - 116;
        int y = -84 + ((ct * (speed + 1)) / 3) % 176;   // slow, floaty fall
        constexpr int sway[8] = { 0, 2, 3, 2, 0, -2, -3, -2 };
        int x = x0 + sway[((ct + i * 5) / 6) & 7];
        c.set_position(x, y);
        c.set_visible(true);

        int colour = (i % 3 == 2) ? 4 : winner;
        c.set_tiles(bn::sprite_items::confetti.tiles_item(), colour * 3 + ((ct + i) / 5) % 3);
    }
}

void court_view::update_smoke(const sim::state& s)
{
    if(_smoke.empty())
    {
        for(int i = 0; i < 10; ++i)
        {
            _smoke.push_back(bn::sprite_items::smoke.create_sprite(0, 0, 0));
            _smoke.back().set_visible(false);
            _smoke.back().set_z_order(-1);
        }
    }

    // A new puff at the feet of every burning rocket, every other frame (plus a big one at launch).
    if(! s.paused)
    {
        for(int slot = 0; slot < sim::MAX_PLAYERS; ++slot)
        {
            const sim::blob& bl = s.blobs[slot];

            if(((s.alive >> slot) & 1) && bl.rocket && (s.frame & 1) == 0)
            {
                puff& p = _puffs[_puff_next];
                _puff_next = (_puff_next + 1) % 10;
                p.x = sim::to_int(bl.x) + int((s.frame * 7) % 5) - 2;
                p.y = sim::to_int(bl.y) + (sim::slot_is_top(slot) ? -2 : 2);
                p.age = 0;
            }
        }
    }

    for(int i = 0; i < 10; ++i)
    {
        puff& p = _puffs[i];
        bn::sprite_ptr& sp = _smoke[i];

        if(p.age >= 18)
        {
            sp.set_visible(false);
            continue;
        }

        place(sp, p.x, p.y, 4);
        sp.set_tiles(bn::sprite_items::smoke.tiles_item(), p.age / 6);

        if(! s.paused)
        {
            ++p.age;
        }
    }
}

void court_view::update(const sim::state& s)
{
    update_bomb(s);
    update_victory(s);
    update_smoke(s);

    // Blobs.
    for(int slot = 0; slot < sim::MAX_PLAYERS; ++slot)
    {
        blob_sprite& b = _blobs[slot];

        if(! ((s.alive >> slot) & 1))
        {
            b.sprite.reset();   // out of lives (or no player in this slot)
            continue;
        }

        const sim::blob& bl = s.blobs[slot];
        int hop = (slot == s.winner && s.phase == sim::PHASE_GAME_OVER) ? winner_hop(s) : 0;
        int frame = bl.rocket ? 3 : (! bl.on_floor || hop > 0) ? 2 : (bl.hit_cooldown > 3 ? 1 : 0);

        if(! b.sprite)
        {
            b.sprite = blob_item(slot).create_sprite(0, 0, frame);
            b.frame = frame;
            // Right-side players face the middle; top-row players are upside down in the world,
            // and a rotated (top-row) console turns everything by 180 degrees.
            b.sprite->set_horizontal_flip(sim::slot_is_right(slot) != _rotated);
            b.sprite->set_vertical_flip(sim::slot_is_top(slot) != _rotated);
        }
        else if(b.frame != frame)
        {
            b.sprite->set_tiles(blob_item(slot).tiles_item(), frame);
            b.frame = frame;
        }

        // The 32x32 frame has the feet on its last row: centre is 16 px from the floor-side base.
        int bx = sim::to_int(bl.x);
        int by = sim::to_int(bl.y) + (sim::slot_is_top(slot) ? 16 : -16);
        by += sim::slot_is_top(slot) ? hop : -hop;
        place(*b.sprite, bx, by, 16);
    }

    // Ball and its trail (render-only history).
    int bx = sim::to_int(s.ball.x);
    int by = sim::to_int(s.ball.y);
    bool moving = s.phase == sim::PHASE_RALLY && ! s.paused;

    if(moving)
    {
        _hist_x[_hist_pos] = bx;
        _hist_y[_hist_pos] = by;
        _hist_pos = (_hist_pos + 1) & 7;

        if(_hist_len < 8)
        {
            ++_hist_len;
        }
    }
    else if(s.phase != sim::PHASE_RALLY)
    {
        _hist_len = 0;
    }

    bool ball_visible = s.phase != sim::PHASE_GAME_OVER &&
            (s.phase != sim::PHASE_SERVE || s.timer > 40 || ((s.timer >> 2) & 1) == 0);
    place(_ball, bx, by, 4);

    if(! ball_visible)
    {
        _ball.set_visible(false);
    }

    for(int i = 0; i < 3; ++i)
    {
        int back = (i + 1) * 2;

        if(_hist_len > back)
        {
            int idx = (_hist_pos - 1 - back) & 7;
            place(_trail[i], _hist_x[idx], _hist_y[idx], 4);
        }
        else
        {
            _trail[i].set_visible(false);
        }
    }

    // Explosion where the ball hit a floor: 4 frames, 6 game frames each.
    uint32_t since = s.frame - s.boom_frame;

    if(s.boom_frame && since < 24)
    {
        int frame = int(since / 6);
        place(_boom, s.boom_x, s.boom_y, 16);

        if(frame != _boom_frame)
        {
            _boom.set_tiles(bn::sprite_items::boom.tiles_item(), frame);
            _boom_frame = frame;
        }
    }
    else
    {
        _boom.set_visible(false);
    }

    if(_show_hud)
    {
        update_hud(s);
        update_minimap(s);
    }
}

void court_view::update_minimap(const sim::state& s)
{
    // Top-right corner, under the lives. Rotated with the view on P3/P4's consoles.
    constexpr int MAP_X = 100;
    constexpr int MAP_Y = -46;

    if(! _map)
    {
        _map = bn::sprite_items::minimap.create_sprite(MAP_X, MAP_Y, s.player_count - 1);
        _map->set_horizontal_flip(_rotated);
        _map->set_vertical_flip(_rotated);
        _map->set_z_order(1);

        for(int i = 0; i <= sim::MAX_PLAYERS; ++i)
        {
            _map_dots[i] = bn::sprite_items::map_dot.create_sprite(MAP_X, MAP_Y, i);
            _map_dots[i]->set_z_order(0);
        }
    }

    auto put = [this](bn::sprite_ptr& dot, int world_x, int world_y, bool visible) {
        // 1/16 scale; the map's 30x20 interior is centred on the sprite.
        int ox = world_x / 16 - 15;
        int oy = world_y / 16 - 10;

        if(_rotated)
        {
            ox = -ox - 1;
            oy = -oy - 1;
        }

        dot.set_position(MAP_X + ox, MAP_Y + oy);
        dot.set_visible(visible);
    };

    bool ball_on = s.phase != sim::PHASE_GAME_OVER && (s.phase != sim::PHASE_SERVE || ((s.timer >> 2) & 1) == 0);
    put(*_map_dots[0], sim::to_int(s.ball.x), sim::to_int(s.ball.y), ball_on);

    for(int slot = 0; slot < sim::MAX_PLAYERS; ++slot)
    {
        const sim::blob& bl = s.blobs[slot];
        int feet_to_centre = sim::slot_is_top(slot) ? 8 : -8;
        put(*_map_dots[1 + slot], sim::to_int(bl.x), sim::to_int(bl.y) + feet_to_centre, (s.alive >> slot) & 1);
    }
}

void court_view::update_hud(const sim::state& s)
{
    // Every console draws its floor at the bottom, so the HUD is always along the top.
    int y = -70;
    bool changed = s.occupied != _occupied_cache;

    for(int slot = 0; slot < sim::MAX_PLAYERS; ++slot)
    {
        changed |= s.lives[slot] != _score_cache[slot];
    }

    if(changed)
    {
        _scores.clear();
        _occupied_cache = s.occupied;

        // Lives, in each player's colour: left-column players in the left corner, right-column in the right.
        constexpr int xs[sim::MAX_PLAYERS] = { -114, -98, 106, 90 };

        for(int slot = 0; slot < sim::MAX_PLAYERS; ++slot)
        {
            _score_cache[slot] = s.lives[slot];

            if((s.occupied >> slot) & 1)
            {
                char buf[2] = {};
                text::dec(buf, s.lives[slot], 1);
                text::small(s.lives[slot] ? text::player_ink(slot) : text::ink::DIM).generate(xs[slot], y, buf, _scores);
            }
        }
    }

    // Rockets left: a dot per rocket under each player's lives, in their colour.
    bool pips_changed = false;

    for(int slot = 0; slot < sim::MAX_PLAYERS; ++slot)
    {
        pips_changed |= s.rockets[slot] != _pip_cache[slot];
    }

    if(pips_changed)
    {
        _pips.clear();
        constexpr int px[sim::MAX_PLAYERS] = { -114, -98, 106, 90 };

        for(int slot = 0; slot < sim::MAX_PLAYERS; ++slot)
        {
            _pip_cache[slot] = s.rockets[slot];

            for(int i = 0; i < s.rockets[slot] && ! _pips.full(); ++i)
            {
                _pips.push_back(bn::sprite_items::map_dot.create_sprite(px[slot] + 3 + i * 5, y + 12, 1 + slot));
            }
        }
    }

    // Rally counter in the middle: hits in the current rally (it resets on the next serve), sand from 10 up.
    int hits = s.rally_hits > 99 ? 99 : s.rally_hits;

    if(hits != _rally_cache)
    {
        _rally_cache = hits;
        char buf[3] = {};
        text::dec(buf, unsigned(hits), 2);
        _counter.clear();
        bn::sprite_text_generator& cg = text::small(hits >= 10 ? text::ink::SAND : text::ink::CREAM);
        cg.set_center_alignment();
        cg.generate(0, y, buf, _counter);
        text::tiny(text::ink::DIM).generate(0, y + 12, "RALLY", _counter);
    }
}

void court_view::set_message(const char* line1, const char* line2, int ink_line1)
{
    char key[64];
    int n = 0;

    for(const char* p = line1; *p && n < 30; ++p)
    {
        key[n++] = *p;
    }

    key[n++] = '|';

    for(const char* p = line2; *p && n < 60; ++p)
    {
        key[n++] = *p;
    }

    key[n++] = char('0' + ink_line1);
    key[n] = 0;

    bool same = true;

    for(int i = 0; i <= n; ++i)
    {
        if(key[i] != _msg_cache[i])
        {
            same = false;
            break;
        }
    }

    if(same)
    {
        return;
    }

    for(int i = 0; i <= n; ++i)
    {
        _msg_cache[i] = key[i];
    }

    _msg1.clear();
    _msg2.clear();

    bn::sprite_text_generator& gen1 = text::small(text::ink(ink_line1));
    gen1.set_center_alignment();
    gen1.generate(0, -8, line1, _msg1);

    // With no first line (the victory screen), the second sits under the big title.
    text::tiny(text::ink::CREAM).generate(0, *line1 ? 10 : 34, line2, _msg2);
}
