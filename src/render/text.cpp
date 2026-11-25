#include "render/text.h"

#include "bn_optional.h"
#include "bn_sprite_font.h"
#include "bn_sprite_palette_item.h"
#include "bn_sprite_items_font_small.h"
#include "bn_sprite_items_font_big.h"
#include "bn_sprite_items_font_tiny.h"
#include "bn_sprite_items_font_big_fire.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_utf8_characters_map_ref.h"

#include "lv_font_widths.h"

namespace
{
    constexpr bn::color c(int r, int g, int b)
    {
        return bn::color(r >> 3, g >> 3, b >> 3);
    }

    #define LV_PAL(name, r, g, b) \
        constexpr bn::color name##_colors[16] = { c(255, 0, 255), c(r, g, b), c(0,0,0), c(0,0,0), c(0,0,0), \
            c(0,0,0), c(0,0,0), c(0,0,0), c(0,0,0), c(0,0,0), c(0,0,0), c(0,0,0), c(0,0,0), c(0,0,0), c(0,0,0), c(0,0,0) }; \
        constexpr bn::sprite_palette_item name##_palette(name##_colors, bn::bpp_mode::BPP_4);

    LV_PAL(cream, 0xCD, 0xF0, 0xEE)
    LV_PAL(p1, 0x5F, 0xD6, 0xCA)
    LV_PAL(p2, 0xFF, 0x8A, 0x5C)
    LV_PAL(p3, 0xF2, 0xCF, 0x8A)
    LV_PAL(p4, 0xB4, 0x9C, 0xFF)
    LV_PAL(sand, 0xF2, 0xCF, 0x8A)
    LV_PAL(dim, 0x4A, 0x55, 0x5A)
    LV_PAL(red, 0xFF, 0x4D, 0x5E)
    LV_PAL(mint, 0x8F, 0xD6, 0xC8)
    LV_PAL(white, 0xFF, 0xF3, 0xD6)

    const bn::sprite_palette_item& palette(text::ink colour)
    {
        switch(colour)
        {
        case text::ink::P1: return p1_palette;
        case text::ink::P2: return p2_palette;
        case text::ink::P3: return p3_palette;
        case text::ink::P4: return p4_palette;
        case text::ink::SAND: return sand_palette;
        case text::ink::DIM: return dim_palette;
        case text::ink::RED: return red_palette;
        case text::ink::MINT: return mint_palette;
        case text::ink::WHITE: return white_palette;
        default: return cream_palette;
        }
    }

    constexpr bn::sprite_font tiny_font(bn::sprite_items::font_tiny, bn::utf8_characters_map_ref(), font_tiny_widths);
    constexpr bn::sprite_font small_font(bn::sprite_items::font_small, bn::utf8_characters_map_ref(), font_small_widths);
    constexpr bn::sprite_font big_font(bn::sprite_items::font_big, bn::utf8_characters_map_ref(), font_big_widths);

    bn::optional<bn::sprite_text_generator> tiny_gen;
    bn::optional<bn::sprite_text_generator> small_gen;
    bn::optional<bn::sprite_text_generator> big_gen;
    constexpr bn::sprite_font fire_font(bn::sprite_items::font_big_fire, bn::utf8_characters_map_ref(), font_big_widths);
    bn::optional<bn::sprite_text_generator> fire_gen;
}

namespace text
{

ink player_ink(int slot)
{
    constexpr ink inks[] = { ink::P1, ink::P2, ink::P3, ink::P4 };
    return inks[slot & 3];
}

bn::sprite_text_generator& tiny(ink colour)
{
    if(! tiny_gen)
    {
        tiny_gen.emplace(tiny_font);
    }

    tiny_gen->set_center_alignment();
    tiny_gen->set_palette_item(palette(colour));
    return *tiny_gen;
}

bn::sprite_text_generator& small(ink colour)
{
    if(! small_gen)
    {
        small_gen.emplace(small_font);
    }

    small_gen->set_left_alignment();
    small_gen->set_palette_item(palette(colour));
    return *small_gen;
}

bn::sprite_text_generator& big(ink colour)
{
    if(! big_gen)
    {
        big_gen.emplace(big_font);
    }

    big_gen->set_center_alignment();
    big_gen->set_palette_item(palette(colour));
    return *big_gen;
}

void dec(char* out, unsigned value, int digits)
{
    for(int i = digits - 1; i >= 0; --i)
    {
        out[i] = char('0' + value % 10);
        value /= 10;
    }
}

void hex(char* out, unsigned value, int digits)
{
    for(int i = digits - 1; i >= 0; --i)
    {
        unsigned d = value & 15;
        out[i] = char(d < 10 ? '0' + d : 'A' + d - 10);
        value >>= 4;
    }
}

bn::sprite_text_generator& big_fire()
{
    if(! fire_gen)
    {
        fire_gen.emplace(fire_font);
        fire_gen->set_center_alignment();
    }
    return *fire_gen;
}

void fire_step(int frame)
{
    // Rotating the 15 fire colours moves the bands baked into the glyphs: flames rising.
    bn::sprite_palette_ptr pal = bn::sprite_items::font_big_fire.palette_item().create_palette();
    pal.set_rotate_count((frame / 4) % 15);
}

}
