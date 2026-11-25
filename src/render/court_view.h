// Draws one console's quadrant of the court: its background, every blob and the ball that are inside it,
// and the HUD. The world is shared; each console only differs in which quadrant it looks at.
#ifndef LV_COURT_VIEW_H
#define LV_COURT_VIEW_H

#include "bn_optional.h"
#include "bn_regular_bg_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_vector.h"

#include "sim/sim.h"

class court_view
{
public:
    court_view(int my_slot, bool show_hud);
    ~court_view();

    void update(const sim::state& s);

    // Centre message lines (empty string = hide). Regenerated only when the text changes.
    void set_message(const char* line1, const char* line2, int ink_line1);

    // Practice: the CPU's slot, so the victory screen says "CPU" instead of "P3".
    void set_cpu_slot(int slot) { _cpu_slot = slot; }

private:
    struct blob_sprite
    {
        bn::optional<bn::sprite_ptr> sprite;
        int frame = -1;
    };

    int _slot;
    int _left;
    int _top;
    bool _show_hud;
    bool _rotated;   // top-row consoles see their quarter rotated 180 degrees
    bn::regular_bg_ptr _bg;
    blob_sprite _blobs[sim::MAX_PLAYERS];
    bn::sprite_ptr _ball;
    bn::sprite_ptr _trail[3];
    bn::sprite_ptr _boom;
    bn::vector<bn::sprite_ptr, 8> _chain;   // extra explosions across the screen when the bomb hits my floor
    int _chain_frame[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    int _shake_x = 0;
    int _shake_y = 0;
    int _fade_key = -1;
    bn::optional<bn::sprite_ptr> _map;
    bn::optional<bn::sprite_ptr> _map_dots[1 + sim::MAX_PLAYERS];   // ball, then players
    int _hist_x[8] = {};
    int _hist_y[8] = {};
    int _hist_pos = 0;
    int _hist_len = 0;
    bn::vector<bn::sprite_ptr, 16> _scores;
    bn::vector<bn::sprite_ptr, 12> _counter;
    bn::vector<bn::sprite_ptr, 24> _msg1;
    bn::vector<bn::sprite_ptr, 40> _msg2;
    char _msg_cache[64] = {};
    uint8_t _score_cache[sim::MAX_PLAYERS] = {255, 255, 255, 255};
    int _boom_frame = -1;
    uint8_t _occupied_cache = 0;
    int _cpu_slot = -1;
    int _rally_cache = -1;

    // Rocket smoke (render-only): puffs left behind a blob's feet while its rocket burns.
    struct puff
    {
        int x = 0, y = 0, age = 99;
    };

    bn::vector<bn::sprite_ptr, 10> _smoke;
    puff _puffs[10];
    int _puff_next = 0;
    bn::vector<bn::sprite_ptr, 12> _pips;   // rockets left, as dots under each player's lives
    uint8_t _pip_cache[sim::MAX_PLAYERS] = {255, 255, 255, 255};

    // Victory screen (render-only): big title, hopping winner, confetti.
    uint32_t _win_frame = 0;
    bool _win_active = false;
    bn::vector<bn::sprite_ptr, 4> _title1;
    bn::vector<bn::sprite_ptr, 8> _title2;
    bn::vector<bn::sprite_ptr, 16> _confetti;

    void place(bn::sprite_ptr& sprite, int world_x, int world_y, int half);
    void update_hud(const sim::state& s);
    void update_minimap(const sim::state& s);
    void update_bomb(const sim::state& s);
    void update_victory(const sim::state& s);
    void update_smoke(const sim::state& s);
    [[nodiscard]] int winner_hop(const sim::state& s) const;
};

#endif
