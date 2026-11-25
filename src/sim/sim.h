// Link Volley deterministic simulation.
// Pure integer code: no floats, no hardware, no Butano. Compiles on desktop for tests.
//
// World: up to 2x2 GBA screens (480 x 320). Quadrants (= player slots):
//   slot 2 (P3) top-left    | slot 3 (P4) top-right      <- floor at y = 0, gravity up
//   slot 0 (P1) bottom-left | slot 1 (P2) bottom-right   <- floor at y = 320, gravity down
// Screen edges bounce the ball unless another occupied quadrant is on the other side.
// Ball gravity pulls toward the floor of the row it is in (down in the bottom row, up in the top row),
// so it flips when the ball crosses y = 160. Spin from the hit curves it sideways. Ball through your floor = you lose the point:
// the ball explodes and you lose one of your 3 lives. At 0 you are out and your floor becomes a wall.
// Last player standing wins.
#ifndef LV_SIM_H
#define LV_SIM_H

#include <stdint.h>

namespace sim
{

// Q8 fixed point (8 fractional bits).
typedef int32_t q8;
constexpr int Q = 8;
constexpr q8 ONE = 1 << Q;
constexpr q8 from_int(int v) { return v * ONE; }
constexpr int to_int(q8 v) { return v >> Q; }   // floor

constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 160;
constexpr int WORLD_W = SCREEN_W * 2;
constexpr int WORLD_H = SCREEN_H * 2;
constexpr int MAX_PLAYERS = 4;

constexpr int BLOB_R = 10;     // blob half-dome radius in px
constexpr int BALL_R = 4;      // ball radius in px
constexpr int START_LIVES = 3;
constexpr int START_ROCKETS = 3;
constexpr int FLOOR_INSET = 12;          // court strip height: floors are at y = 12 (top) and y = 308 (bottom)
constexpr int FLOOR_TOP = FLOOR_INSET;
constexpr int FLOOR_BOTTOM = WORLD_H - FLOOR_INSET;

// Input buttons (6 bits travel in the packet).
enum button : uint8_t
{
    BTN_LEFT = 1 << 0,
    BTN_RIGHT = 1 << 1,
    BTN_A = 1 << 2,       // jump
    BTN_B = 1 << 3,       // tap: rocket to the ball (3 per match); held while touching the ball: smash
    BTN_START = 1 << 4,   // pause
    BTN_SPARE = 1 << 5
};

enum phase : uint8_t
{
    PHASE_SERVE = 0,   // ball waits before moving
    PHASE_RALLY = 1,
    PHASE_GAME_OVER = 2
};

// Sound / effect events produced by a step (output only, not part of the state).
enum event : uint8_t
{
    EV_HIT = 1 << 0,
    EV_WALL = 1 << 1,
    EV_POINT = 1 << 2,    // the ball exploded on someone's floor
    EV_WIN = 1 << 3,
    EV_JUMP = 1 << 4,
    EV_SMASH = 1 << 5,
    EV_ROCKET = 1 << 6    // a blob fired its rocket
};

struct blob
{
    q8 x, y;        // x: centre; y: floor contact point (dome base)
    q8 vx, vy;
    uint8_t on_floor;
    uint8_t hit_cooldown;
    uint8_t rocket;       // frames of rocket flight left (straight line, no gravity)
    uint8_t flying;       // airborne after a rocket: no steering, bounces off screen edges until it lands
};

struct ball_state
{
    q8 x, y, vx, vy;
    int32_t spin;   // sideways curve: added to vx each frame (Q8 / 64), decays
};

struct state
{
    uint32_t frame;
    uint8_t player_count;    // 1..4 (1 = practice: slot 0 human + slot 2 AI)
    uint8_t occupied;        // bitmask of quadrants with a player
    uint8_t phase;
    uint8_t paused;
    uint8_t starting;        // match start: frozen until every player has pressed A (rematch_mask collects them)
    uint8_t winner;          // valid in PHASE_GAME_OVER
    uint8_t last_loser;      // slot that missed last
    uint8_t last_hitter;     // slot that touched the ball last (0xFF = nobody yet)
    uint8_t rematch_mask;    // slots that pressed A on the game-over screen
    uint8_t prev_buttons[MAX_PLAYERS];
    uint16_t timer;          // serve countdown / game-over guard
    uint16_t rally_hits;
    uint32_t rng;
    uint8_t lives[MAX_PLAYERS];
    uint8_t alive;           // bitmask of slots still in the game
    uint8_t rockets[MAX_PLAYERS];   // rocket boosts left this match
    int16_t boom_x, boom_y;  // where the last explosion happened (px)
    uint32_t boom_frame;     // frame of the last explosion (0 = none yet)
    blob blobs[MAX_PLAYERS];
    ball_state ball;
};

// Quadrant helpers.
// Layout (as the consoles sit on the table):  P2 P4 / P1 P3. P1 and P2 face each other in the left column.
constexpr bool slot_is_top(int slot) { return (slot & 1) != 0; }
constexpr bool slot_is_right(int slot) { return slot >= 2; }
constexpr int slot_left(int slot) { return slot_is_right(slot) ? SCREEN_W : 0; }
constexpr int slot_top(int slot) { return slot_is_top(slot) ? 0 : SCREEN_H; }

// Which quadrant slots are in use for a given player count.
uint8_t occupied_for(int player_count);

void init(state& s, int player_count, uint32_t seed);

// Advance one frame. inputs[slot] for every occupied slot. Returns EV_* flags.
uint8_t step(state& s, const uint8_t inputs[MAX_PLAYERS]);

// FNV-1a over the gameplay-relevant fields.
uint32_t hash(const state& s);

// Simple deterministic AI for practice mode.
uint8_t ai_input(const state& s, int slot);

// Deterministic integer square root.
uint32_t isqrt(uint32_t v);

}

#endif
