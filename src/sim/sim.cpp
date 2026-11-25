// Link Volley deterministic simulation. Integer / Q8 fixed point only.
#include "sim.h"

namespace sim
{

namespace
{
    // Tuning (Q8 unless noted).
    constexpr q8 BLOB_GRAVITY = 56;           // 0.22 px/f^2
    constexpr q8 BLOB_JUMP = 1152;            // 4.5 px/f -> ~46 px jump
    constexpr q8 BLOB_SPEED = 512;            // 2 px/f
    constexpr q8 BALL_GRAVITY = 20;           // 0.078 px/f^2, toward the floor of the current row
    constexpr q8 HIT_SPEED = 1280;            // 5 px/f after a normal hit
    constexpr q8 SMASH_SPEED = 1664;          // 6.5 px/f with B held
    constexpr q8 DEAD_FLOOR_KICK = 1408;      // 5.5 px/f: clears the 148 px to the middle line against gravity
    constexpr q8 ROCKET_SPEED = 1536;         // 6 px/f: the rocket's speed limit (3x walking)
    constexpr int ROCKET_HORIZON = 120;       // frames ahead the rocket looks for an intercept
    constexpr int ROCKET_AIM = 8;             // aim the dome's base this far below the ball, so it hits the top
    constexpr q8 MAX_SPEED = 1792;            // 7 px/f cap
    constexpr q8 MIN_AWAY = 512;              // after a hit, at least 2 px/f away from the hitter's floor
    constexpr int SERVE_FRAMES = 60;          // ~1 s before the ball drops
    constexpr int GAME_OVER_GUARD = 300;       // 5 s of victory screen (title, fanfare, confetti) before A counts
    constexpr int HIT_COOLDOWN = 8;
    constexpr int SERVE_OFFSET = 30;          // px from the middle line, inside the server's row

    constexpr int abs_i(int v) { return v < 0 ? -v : v; }

    uint32_t next_rng(uint32_t& r)
    {
        r ^= r << 13;
        r ^= r >> 17;
        r ^= r << 5;
        return r;
    }

    bool occ(const state& s, int col, int row)  // row 0 = top, 1 = bottom
    {
        if(col < 0 || col > 1 || row < 0 || row > 1)
        {
            return false;
        }

        int slot = col * 2 + (row == 0 ? 1 : 0);
        return (s.occupied >> slot) & 1;
    }

    int slot_at(int col, int row)
    {
        return col * 2 + (row == 0 ? 1 : 0);
    }

    uint8_t step_ball(state& s);

    void place_serve(state& s, int slot)
    {
        ball_state& b = s.ball;
        b.x = from_int(slot_left(slot) + SCREEN_W / 2);
        b.y = from_int(slot_is_top(slot) ? SCREEN_H - SERVE_OFFSET : SCREEN_H + SERVE_OFFSET);
        b.vx = 0;
        b.vy = 0;
        b.spin = 0;
        s.phase = PHASE_SERVE;
        s.timer = SERVE_FRAMES;
        s.rally_hits = 0;
        s.last_hitter = 0xFF;
    }

    void reset_blobs(state& s)
    {
        for(int slot = 0; slot < MAX_PLAYERS; ++slot)
        {
            blob& bl = s.blobs[slot];
            bl.x = from_int(slot_left(slot) + SCREEN_W / 2 + (slot_is_right(slot) ? 30 : -30));
            bl.y = from_int(slot_is_top(slot) ? FLOOR_TOP : FLOOR_BOTTOM);
            bl.vx = 0;
            bl.vy = 0;
            bl.on_floor = 1;
            bl.hit_cooldown = 0;
            bl.rocket = 0;
            bl.flying = 0;
        }
    }

    // Scales (vx, vy) to magnitude `target`.
    void set_speed(q8& vx, q8& vy, q8 target)
    {
        uint32_t len = isqrt(uint32_t(vx * vx + vy * vy));

        if(len == 0)
        {
            vy = -target;
            return;
        }

        vx = vx * target / int32_t(len);
        vy = vy * target / int32_t(len);
    }

    // Fires slot's rocket: the earliest moment the ball, forward-simulated, can be met inside my screen
    // at no more than ROCKET_SPEED. If there is none (too late), it still flies at full speed toward where
    // the ball will be, and misses.
    void fire_rocket(state& s, int slot)
    {
        blob& bl = s.blobs[slot];
        int down = slot_is_top(slot) ? -1 : 1;
        int left = slot_left(slot);
        int top = slot_top(slot);
        state p = s;
        p.phase = PHASE_RALLY;
        q8 tx = 0, ty = 0;
        int frames = 0;
        bool found = false;

        for(int t = 1; t <= ROCKET_HORIZON && p.phase == PHASE_RALLY; ++t)
        {
            step_ball(p);
            int px = to_int(p.ball.x);
            int py = to_int(p.ball.y) + down * ROCKET_AIM;   // where my base must be
            int floor_px = slot_is_top(slot) ? FLOOR_TOP : FLOOR_BOTTOM;

            if(px < left + BLOB_R || px >= left + SCREEN_W - BLOB_R || down * (py - floor_px) > 0 ||
               py < top + 2 * BLOB_R || py >= top + SCREEN_H - 2 * BLOB_R)
            {
                continue;   // not reachable inside my screen at that moment
            }

            q8 dx = from_int(px) - bl.x;
            q8 dy = from_int(py) - bl.y;
            uint32_t dist = isqrt(uint32_t(int64_t(dx) * dx / ONE + int64_t(dy) * dy / ONE)) * 16;   // Q8 (via Q0 sqrt of Q8^2/Q8)
            tx = dx;
            ty = dy;
            frames = t;

            if(dist <= uint32_t(ROCKET_SPEED) * uint32_t(t))
            {
                found = true;
                break;
            }
        }

        if(! frames)
        {
            // Nothing in my screen at all: launch straight up at full speed.
            tx = 0;
            ty = -down * ONE;
            frames = 30;
        }

        bl.vx = tx;
        bl.vy = ty;

        if(found)
        {
            bl.vx = tx / frames;
            bl.vy = ty / frames;
        }
        else
        {
            set_speed(bl.vx, bl.vy, ROCKET_SPEED);
        }

        bl.rocket = uint8_t(frames);
        bl.flying = 1;
        bl.on_floor = 0;
    }

    uint8_t step_blobs(state& s, const uint8_t inputs[MAX_PLAYERS], const uint8_t pressed[MAX_PLAYERS])
    {
        uint8_t events = 0;

        for(int slot = 0; slot < MAX_PLAYERS; ++slot)
        {
            if(! ((s.alive >> slot) & 1))
            {
                continue;
            }

            blob& bl = s.blobs[slot];
            uint8_t in = inputs[slot];
            int down = slot_is_top(slot) ? -1 : 1;
            q8 floor_y = from_int(slot_is_top(slot) ? FLOOR_TOP : FLOOR_BOTTOM);

            // B tap during a rally (and not touching the ball): rocket to it.
            if((pressed[slot] & BTN_B) && s.phase == PHASE_RALLY && s.rockets[slot] && ! bl.flying)
            {
                q8 bdx = s.ball.x - bl.x;
                q8 bdy = s.ball.y - bl.y;

                if(abs_i(bdx) > from_int(2 * (BLOB_R + BALL_R)) || abs_i(bdy) > from_int(2 * (BLOB_R + BALL_R)))
                {
                    --s.rockets[slot];
                    fire_rocket(s, slot);
                    events |= EV_ROCKET;
                }
            }

            if(bl.rocket || bl.flying)
            {
                // Rocket flight: a straight line at fixed speed, then a free fall (no steering) that bounces
                // off the screen's side edges and the far edge until it lands.
                if(bl.rocket)
                {
                    --bl.rocket;
                }
                else
                {
                    bl.vy += down * BLOB_GRAVITY;
                }

                bl.x += bl.vx;
                bl.y += bl.vy;

                q8 lo_x = from_int(slot_left(slot) + BLOB_R);
                q8 hi_x = from_int(slot_left(slot) + SCREEN_W - BLOB_R);

                if(bl.x < lo_x)
                {
                    bl.x = lo_x;
                    bl.vx = abs_i(bl.vx);
                }
                else if(bl.x > hi_x)
                {
                    bl.x = hi_x;
                    bl.vx = -abs_i(bl.vx);
                }

                // The far edge of my screen (the middle line or the outer edge): the dome's top bounces off it.
                q8 far_y = from_int(slot_is_top(slot) ? SCREEN_H - 2 * BLOB_R : SCREEN_H + 2 * BLOB_R);

                if(down * (bl.y - far_y) < 0)
                {
                    bl.y = far_y;
                    bl.vy = down * abs_i(bl.vy);
                    bl.rocket = 0;
                }

                if(down * (bl.y - floor_y) >= 0)
                {
                    bl.y = floor_y;
                    bl.vx = 0;
                    bl.vy = 0;
                    bl.on_floor = 1;
                    bl.rocket = 0;
                    bl.flying = 0;
                }

                if(bl.hit_cooldown)
                {
                    --bl.hit_cooldown;
                }

                continue;
            }

            bl.vx = 0;

            if(in & BTN_LEFT)
            {
                bl.vx -= BLOB_SPEED;
            }

            if(in & BTN_RIGHT)
            {
                bl.vx += BLOB_SPEED;
            }

            if((in & BTN_A) && bl.on_floor)
            {
                bl.vy = -down * BLOB_JUMP;
                bl.on_floor = 0;
                events |= EV_JUMP;
            }

            bl.vy += down * BLOB_GRAVITY;
            bl.x += bl.vx;
            bl.y += bl.vy;

            if(down * (bl.y - floor_y) >= 0)
            {
                bl.y = floor_y;
                bl.vy = 0;
                bl.on_floor = 1;
            }

            q8 min_x = from_int(slot_left(slot) + BLOB_R);
            q8 max_x = from_int(slot_left(slot) + SCREEN_W - BLOB_R);

            if(bl.x < min_x)
            {
                bl.x = min_x;
            }
            else if(bl.x > max_x)
            {
                bl.x = max_x;
            }

            if(bl.hit_cooldown)
            {
                --bl.hit_cooldown;
            }
        }

        return events;
    }

    uint8_t collide_blobs(state& s, const uint8_t inputs[MAX_PLAYERS])
    {
        uint8_t events = 0;
        ball_state& b = s.ball;
        constexpr int reach = BLOB_R + BALL_R;

        for(int slot = 0; slot < MAX_PLAYERS; ++slot)
        {
            if(! ((s.alive >> slot) & 1))
            {
                continue;
            }

            blob& bl = s.blobs[slot];

            if(bl.hit_cooldown)
            {
                continue;
            }

            q8 dx = b.x - bl.x;
            q8 dy = b.y - bl.y;

            if(abs_i(dx) >= from_int(reach) || abs_i(dy) >= from_int(reach))
            {
                continue;
            }

            int32_t dist = int32_t(isqrt(uint32_t(dx * dx + dy * dy)));

            if(dist >= from_int(reach))
            {
                continue;
            }

            int down = slot_is_top(slot) ? -1 : 1;

            // Contact normal (Q8 unit vector). A semicircle: below the base, push straight out of the floor side.
            q8 nx, ny;

            if(dist == 0 || down * dy > 0)
            {
                nx = dx > 0 ? ONE : (dx < 0 ? -ONE : 0);
                ny = -down * ONE;

                if(nx)
                {
                    set_speed(nx, ny, ONE);
                }
            }
            else
            {
                nx = dx * ONE / dist;
                ny = dy * ONE / dist;
            }

            // Reflect along the normal if moving into the blob, then add the blob's motion.
            q8 vn = (b.vx * nx + b.vy * ny) >> Q;

            if(vn < 0)
            {
                b.vx -= (2 * vn * nx) >> Q;
                b.vy -= (2 * vn * ny) >> Q;
            }

            b.vx += bl.vx / 2;
            b.vy += bl.vy / 2;

            // Racquet: always send it away from the hitter's floor.
            if(down * b.vy > -MIN_AWAY)
            {
                b.vy = -down * MIN_AWAY;
            }

            bool smash = inputs[slot] & BTN_B;
            set_speed(b.vx, b.vy, smash ? SMASH_SPEED : HIT_SPEED);

            // Spin: moving while you hit curves the ball the way you moved; smashing doubles it.
            b.spin = bl.vx / 2 + dx / 16;

            if(smash)
            {
                b.spin *= 2;
            }

            // Push the ball out of the blob.
            b.x = bl.x + ((nx * (reach + 1)));
            b.y = bl.y + ((ny * (reach + 1)));

            bl.hit_cooldown = HIT_COOLDOWN;
            s.last_hitter = uint8_t(slot);
            ++s.rally_hits;
            events |= smash ? (EV_HIT | EV_SMASH) : EV_HIT;
        }

        return events;
    }

    uint8_t lose_point(state& s, int loser)
    {
        uint8_t events = EV_POINT;
        s.last_loser = uint8_t(loser);

        // Boom: the loser drops a life; at zero they are out.
        s.boom_x = int16_t(to_int(s.ball.x));
        s.boom_y = int16_t(to_int(s.ball.y));
        s.boom_frame = s.frame;

        if(s.lives[loser])
        {
            --s.lives[loser];
        }

        if(! s.lives[loser])
        {
            s.alive &= uint8_t(~(1 << loser));
        }

        int alive_count = 0;
        int best = 0;

        for(int slot = 0; slot < MAX_PLAYERS; ++slot)
        {
            if((s.alive >> slot) & 1)
            {
                ++alive_count;
                best = slot;
            }
        }

        if(alive_count <= 1)
        {
            s.phase = PHASE_GAME_OVER;
            s.winner = uint8_t(best);
            s.timer = GAME_OVER_GUARD;
            s.rematch_mask = 0;
            s.ball.vx = 0;
            s.ball.vy = 0;
            events |= EV_WIN;
        }
        else
        {
            // Serve to the loser, or to the next player still in.
            int server = loser;

            while(! ((s.alive >> server) & 1))
            {
                server = (server + 1) & 3;
            }

            place_serve(s, server);
        }

        return events;
    }

    uint8_t step_ball(state& s)
    {
        uint8_t events = 0;
        ball_state& b = s.ball;
        constexpr q8 R = from_int(BALL_R);
        constexpr q8 MID_X = from_int(SCREEN_W);
        constexpr q8 MID_Y = from_int(SCREEN_H);

        // Gravity toward the floor of the row the ball is in.
        b.vy += b.y < MID_Y ? -BALL_GRAVITY : BALL_GRAVITY;

        // Spin curves the ball sideways and decays.
        b.vx += b.spin / 16;
        b.spin -= b.spin / 32;

        uint32_t len = isqrt(uint32_t(b.vx * b.vx + b.vy * b.vy));

        if(len > uint32_t(MAX_SPEED))
        {
            set_speed(b.vx, b.vy, MAX_SPEED);
        }

        // Horizontal move: outer walls, and the middle line when the neighbouring screen is empty.
        int row = b.y < MID_Y ? 0 : 1;
        q8 nx = b.x + b.vx;

        if(nx - R < 0)
        {
            nx = R;
            b.vx = -b.vx;
            b.spin = -b.spin / 2;
            events |= EV_WALL;
        }
        else if(nx + R > from_int(WORLD_W))
        {
            nx = from_int(WORLD_W) - R;
            b.vx = -b.vx;
            b.spin = -b.spin / 2;
            events |= EV_WALL;
        }
        else if(b.x < MID_X && nx + R > MID_X && ! occ(s, 1, row))
        {
            nx = MID_X - R;
            b.vx = -b.vx;
            b.spin = -b.spin / 2;
            events |= EV_WALL;
        }
        else if(b.x >= MID_X && nx - R < MID_X && ! occ(s, 0, row))
        {
            nx = MID_X + R;
            b.vx = -b.vx;
            b.spin = -b.spin / 2;
            events |= EV_WALL;
        }

        b.x = nx;

        // Vertical move: floors score, the middle line bounces when the screen across is empty.
        int col = b.x < MID_X ? 0 : 1;
        q8 ny = b.y + b.vy;

        if(ny + R >= from_int(FLOOR_BOTTOM))
        {
            int slot = slot_at(col, 1);
            b.y = from_int(FLOOR_BOTTOM) - R;

            if((s.alive >> slot) & 1)
            {
                return events | lose_point(s, slot);
            }

            // An eliminated player's floor is a trampoline: strong enough to send the ball back over the
            // middle line, so a row whose players are both out can't trap the ball.
            b.vy = -(abs_i(b.vy) > DEAD_FLOOR_KICK ? abs_i(b.vy) : DEAD_FLOOR_KICK);
            return events | EV_WALL;
        }

        if(ny - R <= from_int(FLOOR_TOP))
        {
            int slot = slot_at(col, 0);
            b.y = from_int(FLOOR_TOP) + R;

            if((s.alive >> slot) & 1)
            {
                return events | lose_point(s, slot);
            }

            b.vy = abs_i(b.vy) > DEAD_FLOOR_KICK ? abs_i(b.vy) : DEAD_FLOOR_KICK;
            return events | EV_WALL;
        }

        if(b.y >= MID_Y && ny - R < MID_Y && ! occ(s, col, 0))
        {
            ny = MID_Y + R;
            b.vy = -b.vy;
            events |= EV_WALL;
        }
        else if(b.y < MID_Y && ny + R >= MID_Y && ! occ(s, col, 1))
        {
            ny = MID_Y - R;
            b.vy = -b.vy;
            events |= EV_WALL;
        }

        b.y = ny;
        return events;
    }

    void feed(uint32_t& h, uint32_t v)
    {
        for(int i = 0; i < 4; ++i)
        {
            h ^= (v >> (i * 8)) & 0xFF;
            h *= 16777619u;
        }
    }
}

uint32_t isqrt(uint32_t v)
{
    uint32_t res = 0;
    uint32_t bit = 1u << 30;

    while(bit > v)
    {
        bit >>= 2;
    }

    while(bit)
    {
        if(v >= res + bit)
        {
            v -= res + bit;
            res = (res >> 1) + bit;
        }
        else
        {
            res >>= 1;
        }

        bit >>= 2;
    }

    return res;
}

uint8_t occupied_for(int player_count)
{
    switch(player_count)
    {
    case 1:
        return 0b0011;   // P1 + practice AI in the P2 slot (top-left, head to head)
    case 2:
        return 0b0011;
    case 3:
        return 0b0111;
    default:
        return 0b1111;
    }
}

void init(state& s, int player_count, uint32_t seed)
{
    s = state();
    s.player_count = uint8_t(player_count);
    s.occupied = occupied_for(player_count);
    s.alive = s.occupied;

    for(int slot = 0; slot < MAX_PLAYERS; ++slot)
    {
        s.lives[slot] = ((s.occupied >> slot) & 1) ? START_LIVES : 0;
        s.rockets[slot] = ((s.occupied >> slot) & 1) ? START_ROCKETS : 0;
    }
    s.rng = seed ? seed : 0x1234567u;
    reset_blobs(s);

    // First server: a random occupied slot.
    int first;

    do
    {
        first = int(next_rng(s.rng) & 3);
    }
    while(! ((s.occupied >> first) & 1));

    place_serve(s, first);
    s.starting = 1;
}

uint8_t step(state& s, const uint8_t inputs[MAX_PLAYERS])
{
    uint8_t events = 0;
    uint8_t pressed[MAX_PLAYERS];
    bool start_pressed = false;

    for(int slot = 0; slot < MAX_PLAYERS; ++slot)
    {
        uint8_t in = ((s.occupied >> slot) & 1) ? inputs[slot] : 0;
        pressed[slot] = in & ~s.prev_buttons[slot];
        s.prev_buttons[slot] = in;
        start_pressed |= (pressed[slot] & BTN_START) != 0;
    }

    ++s.frame;

    if(s.phase == PHASE_GAME_OVER)
    {
        if(s.timer)
        {
            --s.timer;
            return events;
        }

        for(int slot = 0; slot < MAX_PLAYERS; ++slot)
        {
            if(pressed[slot] & BTN_A)
            {
                s.rematch_mask |= uint8_t(1 << slot);
            }
        }

        if((s.rematch_mask & s.occupied) == s.occupied)
        {
            uint32_t seed = next_rng(s.rng);
            uint32_t frame = s.frame;
            init(s, s.player_count, seed);
            s.frame = frame;
            s.starting = 0;   // everyone already pressed A for the rematch
        }

        return events;
    }

    if(s.starting)
    {
        for(int slot = 0; slot < MAX_PLAYERS; ++slot)
        {
            if(pressed[slot] & BTN_A)
            {
                s.rematch_mask |= uint8_t(1 << slot);
            }
        }

        if((s.rematch_mask & s.occupied) == s.occupied)
        {
            s.starting = 0;
            s.rematch_mask = 0;
        }

        return events;
    }

    if(start_pressed)
    {
        s.paused = ! s.paused;
    }

    if(s.paused)
    {
        return events;
    }

    uint8_t held[MAX_PLAYERS];

    for(int slot = 0; slot < MAX_PLAYERS; ++slot)
    {
        held[slot] = s.prev_buttons[slot];
    }

    events |= step_blobs(s, held, pressed);

    if(s.phase == PHASE_SERVE)
    {
        if(s.timer)
        {
            --s.timer;
        }
        else
        {
            s.phase = PHASE_RALLY;
        }

        return events;
    }

    events |= collide_blobs(s, held);
    events |= step_ball(s);
    return events;
}

uint32_t hash(const state& s)
{
    uint32_t h = 2166136261u;
    feed(h, s.frame);
    feed(h, uint32_t(s.ball.x));
    feed(h, uint32_t(s.ball.y));
    feed(h, uint32_t(s.ball.vx));
    feed(h, uint32_t(s.ball.vy));
    feed(h, uint32_t(s.ball.spin));

    for(int slot = 0; slot < MAX_PLAYERS; ++slot)
    {
        const blob& bl = s.blobs[slot];
        feed(h, uint32_t(bl.x));
        feed(h, uint32_t(bl.y));
        feed(h, uint32_t(bl.vx));
        feed(h, uint32_t(bl.vy));
        feed(h, s.lives[slot] | (uint32_t(s.rockets[slot]) << 8) | (uint32_t(bl.rocket) << 16) | (uint32_t(bl.flying) << 24));
    }

    feed(h, uint32_t(s.phase) | (uint32_t(s.paused) << 8) | (uint32_t(s.starting) << 9) | (uint32_t(s.rematch_mask) << 16));
    feed(h, s.rng);
    feed(h, s.last_hitter);
    feed(h, s.alive);
    feed(h, s.boom_frame);
    return h;
}

uint8_t ai_input(const state& s, int slot)
{
    if(s.phase == PHASE_GAME_OVER || s.starting)
    {
        return (s.frame & 8) ? BTN_A : 0;
    }

    const blob& bl = s.blobs[slot];
    const ball_state& b = s.ball;
    int down = slot_is_top(slot) ? -1 : 1;
    int left = slot_left(slot);
    int bx = to_int(b.x);
    int by = to_int(b.y);
    int me_x = to_int(bl.x);
    int me_y = to_int(bl.y);
    bool in_my_row = slot_is_top(slot) ? by < SCREEN_H : by >= SCREEN_H;

    // Predict where the ball comes down to my hitting height (ball-only forward simulation).
    int target = left + SCREEN_W / 2;
    int hit_line = slot_is_top(slot) ? FLOOR_TOP + 22 : FLOOR_BOTTOM - 22;

    if(s.phase == PHASE_RALLY || s.phase == PHASE_SERVE)
    {
        state p = s;
        p.phase = PHASE_RALLY;

        for(int i = 0; i < 180 && p.phase == PHASE_RALLY; ++i)
        {
            step_ball(p);
            int py = to_int(p.ball.y);
            int px = to_int(p.ball.x);
            bool toward = down * p.ball.vy > 0;

            if(toward && px >= left && px < left + SCREEN_W && down * (py - hit_line) >= 0)
            {
                target = px;
                break;
            }
        }
    }

    // Meet it slightly on the outer side so the return heads back toward the middle,
    // with a per-rally aim error so it is beatable (|error| > reach = a miss).
    target += (target < left + SCREEN_W / 2) ? -3 : 3;
    uint32_t e = (uint32_t(s.rally_hits) * 2654435761u) ^ (uint32_t(slot) * 40503u) ^ (s.rng & 0xFFFF) ^ (uint32_t(s.lives[0] + s.lives[1] * 3 + s.lives[2] * 5 + s.lives[3] * 7) * 2246822519u);
    target += int((e >> 7) % 37) - 18;

    uint8_t out = 0;

    if(bl.flying)
    {
        return 0;
    }

    // Too far to walk: fire the rocket (the ball must be on its way down into my screen).
    bool incoming = down * b.vy > 0 && bx >= left && bx < left + SCREEN_W;

    if(s.phase == PHASE_RALLY && s.rockets[slot] && incoming && abs_i(target - me_x) > 70 && ! (s.prev_buttons[slot] & BTN_B))
    {
        return BTN_B;
    }

    if(target < me_x - 3)
    {
        out |= BTN_LEFT;
    }
    else if(target > me_x + 3)
    {
        out |= BTN_RIGHT;
    }

    // Jump when the ball is falling toward me and close.
    int height = down * (me_y - by);   // px above my floor-side base
    bool coming = down * b.vy > 0;

    if(in_my_row && coming && height > 16 && height < 44 && abs_i(bx - me_x) < 18)
    {
        out |= BTN_A;
    }

    return out;
}

}
