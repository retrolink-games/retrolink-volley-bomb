#ifndef LV_SCENES_H
#define LV_SCENES_H

#include <stdint.h>

namespace scenes
{
    enum class id
    {
        TITLE,
        LOBBY,
        GAME
    };

    struct game_setup
    {
        bool practice = true;
        int my_id = 0;
        int player_count = 1;
        uint32_t seed = 1;
    };

    id title();
    id lobby(game_setup& setup);
    id game(const game_setup& setup);
}

#endif
