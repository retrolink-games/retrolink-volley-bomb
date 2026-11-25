// Text helpers: Retro Link's brand pixel font (Silkscreen) at 1x, 2x and bold 4x.
#ifndef LV_TEXT_H
#define LV_TEXT_H

#include "bn_sprite_text_generator.h"

namespace text
{
    enum class ink
    {
        CREAM,
        P1,
        P2,
        P3,
        P4,
        SAND,
        DIM,
        RED
    };

    [[nodiscard]] ink player_ink(int slot);

    // Native 8 px size (8x8 cells), centre aligned: long lines.
    bn::sprite_text_generator& tiny(ink colour);

    // Returns the shared small (2x, 16x16 cell) generator set to `colour`, left aligned.
    bn::sprite_text_generator& small(ink colour);

    // Returns the shared big (bold 4x, 32x32 cell) generator set to `colour`, centre aligned.
    bn::sprite_text_generator& big(ink colour);

    // Formats `value` as `digits` zero-padded decimal digits / hex digits into `out` (no terminator handling needed).
    void dec(char* out, unsigned value, int digits);
    void hex(char* out, unsigned value, int digits);
}

#endif
