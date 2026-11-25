#include "sound/sfx.h"

#include "sim/sim.h"

namespace
{
    // GBA sound registers.
    volatile uint16_t& reg16(uint32_t addr) { return *reinterpret_cast<volatile uint16_t*>(addr); }

    constexpr uint32_t SOUND1CNT_L = 0x04000060;
    constexpr uint32_t SOUND1CNT_H = 0x04000062;
    constexpr uint32_t SOUND1CNT_X = 0x04000064;
    constexpr uint32_t SOUND2CNT_L = 0x04000068;
    constexpr uint32_t SOUND2CNT_H = 0x0400006C;
    constexpr uint32_t SOUND4CNT_L = 0x04000078;
    constexpr uint32_t SOUND4CNT_H = 0x0400007C;
    constexpr uint32_t SOUNDCNT_L = 0x04000080;
    constexpr uint32_t SOUNDCNT_H = 0x04000082;
    constexpr uint32_t SOUNDCNT_X = 0x04000084;

    // rate = 131072 / (2048 - n)
    void square1(uint16_t sweep, uint16_t duty_env, uint16_t freq)
    {
        reg16(SOUND1CNT_L) = sweep;
        reg16(SOUND1CNT_H) = duty_env;
        reg16(SOUND1CNT_X) = uint16_t(0x8000 | (freq & 0x7FF));
    }

    void square2(uint16_t duty_env, uint16_t freq)
    {
        reg16(SOUND2CNT_L) = duty_env;
        reg16(SOUND2CNT_H) = uint16_t(0x8000 | (freq & 0x7FF));
    }

    void noise(uint16_t env, uint16_t poly)
    {
        reg16(SOUND4CNT_L) = env;
        reg16(SOUND4CNT_H) = uint16_t(0x8000 | poly);
    }

    // env: initial volume (0..15), step time (0..7), decreasing.
    constexpr uint16_t env(int vol, int step, int duty = 2)
    {
        return uint16_t((vol << 12) | (step << 8) | (duty << 6));
    }

    // Victory fanfare (original): frames to hold, melody (square 2), bass (square 1, 0 = none), drum hit.
    // Values are tone registers: n = 2048 - 131072 / Hz.
    constexpr uint16_t G4 = 1714, A4 = 1750, C5 = 1798, E5 = 1849, F5 = 1860, G5 = 1881, A5 = 1899, B5 = 1915,
                       C6 = 1923, C3 = 1046, F3 = 1297, G3 = 1379;
    constexpr uint16_t jingle[][4] = {
        {7, G4, C3, 1}, {7, C5, 0, 0}, {7, E5, 0, 0}, {14, G5, C3, 1}, {7, E5, 0, 0}, {21, G5, 0, 1},
        {7, A4, F3, 1}, {7, C5, 0, 0}, {7, F5, 0, 0}, {14, A5, F3, 1}, {7, F5, 0, 0}, {21, A5, 0, 1},
        {7, B5, G3, 1}, {7, B5, 0, 0}, {7, B5, 0, 1}, {60, C6, C3, 2}, {0, 0, 0, 0}
    };

    // The fanfare waits for the last bomb to finish and the title to slam down.
    constexpr int JINGLE_LEAD_IN = 54;

    int jingle_pos = -1;
    int jingle_wait = 0;
}

namespace sfx
{

void init()
{
    reg16(SOUNDCNT_X) = 0x0080;                                   // master on
    reg16(SOUNDCNT_L) = uint16_t(0x7 | (0x7 << 4) | (0xB << 8) | (0xB << 12));   // ch1, ch2, ch4 both sides
    reg16(SOUNDCNT_H) = uint16_t((reg16(SOUNDCNT_H) & ~3) | 2);   // DMG volume 100%, keep Direct Sound bits
}

void hit()
{
    square1(uint16_t((1 << 4) | 2), env(12, 1), 1750);           // short upward sweep
}

void smash()
{
    square1(uint16_t((1 << 4) | 1), env(15, 2), 1850);
    noise(env(10, 1), uint16_t((2 << 4) | 1));
}

void wall()
{
    square2(env(7, 1, 1), 1600);
}

void point()
{
    square1(uint16_t((7 << 4) | (1 << 3) | 1), env(15, 4), 1500);   // falling boom
    noise(env(15, 5), uint16_t((7 << 4) | 4));                        // long low rumble
}

void rocket()
{
    square1(uint16_t((2 << 4) | 3), env(13, 3, 1), 1500);   // rising whoosh
    noise(env(12, 3), uint16_t((1 << 4) | 2));              // hiss of the burn
}

void jump()
{
    square2(env(4, 1, 1), 1820);
}

void win()
{
    jingle_pos = 0;
    jingle_wait = JINGLE_LEAD_IN;
}

void update()
{
    if(jingle_pos < 0)
    {
        return;
    }

    if(jingle_wait > 0)
    {
        --jingle_wait;
        return;
    }

    if(! jingle[jingle_pos][0])
    {
        jingle_pos = -1;
        return;
    }

    const uint16_t* note = jingle[jingle_pos];
    bool last = ! jingle[jingle_pos + 1][0];
    square2(env(12, last ? 7 : 3, 2), note[1]);

    if(note[2])
    {
        square1(0x0008, env(10, last ? 7 : 4, 1), note[2]);   // no sweep
    }

    if(note[3] == 1)
    {
        noise(env(9, 1), uint16_t((3 << 4) | 1));            // snappy hit
    }
    else if(note[3] == 2)
    {
        noise(env(14, 4), uint16_t((5 << 4) | 3));           // final crash
    }

    jingle_wait = jingle[jingle_pos][0];
    ++jingle_pos;
}

void play_events(uint8_t ev)
{
    if(ev & sim::EV_WIN)
    {
        win();
    }

    if(ev & sim::EV_POINT)
    {
        point();   // the last bomb still goes off before the fanfare
    }

    if(ev & sim::EV_ROCKET)
    {
        rocket();
    }

    if(ev & sim::EV_SMASH)
    {
        smash();
    }
    else if(ev & sim::EV_HIT)
    {
        hit();
    }
    else if(ev & sim::EV_WALL)
    {
        wall();
    }
    else if(ev & sim::EV_JUMP)
    {
        jump();
    }
}

}
