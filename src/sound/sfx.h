// Original sound effects on the GBA's DMG (Game Boy style) channels, written straight to the sound registers.
#ifndef LV_SFX_H
#define LV_SFX_H

#include <stdint.h>

namespace sfx
{
    void init();
    void hit();
    void smash();
    void wall();
    void point();     // the bomb: ball exploding on a floor
    void jump();
    void rocket();
    void win();       // starts the jingle
    void update();    // call once per frame (plays the jingle)
    void play_events(uint8_t sim_events);
}

#endif
