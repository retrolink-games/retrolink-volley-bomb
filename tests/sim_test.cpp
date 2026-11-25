// Desktop test: AI vs AI for every player count; prints rally stats and checks determinism.
#include "../src/sim/sim.h"
#include <cstdio>
#include <cstdlib>

static uint32_t run(int players, int frames, bool verbose)
{
    sim::state s;
    sim::init(s, players, 42);
    int points = 0, hits = 0, maxrally = 0, games = 0;
    for(int f = 0; f < frames; ++f)
    {
        uint8_t in[4] = {0, 0, 0, 0};
        for(int k = 0; k < 4; ++k) if((s.occupied >> k) & 1) in[k] = sim::ai_input(s, k);
        uint16_t before = s.rally_hits;
        uint8_t ev = sim::step(s, in);
        if(ev & sim::EV_HIT) ++hits;
        if(ev & sim::EV_POINT) { ++points; if(before > maxrally) maxrally = before; }
        if(ev & sim::EV_WIN) ++games;
    }
    if(verbose)
        printf("players=%d frames=%d points=%d hits=%d hits/point=%.1f maxrally=%d games=%d lives=%d,%d,%d,%d\n",
               players, frames, points, hits, points ? double(hits) / points : 0.0, maxrally, games,
               s.lives[0], s.lives[1], s.lives[2], s.lives[3]);
    return sim::hash(s);
}

int main()
{
    for(int p = 1; p <= 4; ++p)
    {
        uint32_t a = run(p, 36000, true);
        uint32_t b = run(p, 36000, false);
        if(a != b) { printf("NONDETERMINISTIC p=%d\n", p); return 1; }
    }
    for(uint32_t v = 0; v < 2000000; v += 7)
    {
        uint32_t r = sim::isqrt(v);
        if(r * r > v || (r + 1) * (r + 1) <= v) { printf("isqrt bad %u\n", v); return 1; }
    }
    printf("OK\n");
    return 0;
}
