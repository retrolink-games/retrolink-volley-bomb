// Desktop test: N virtual consoles exchange inputs over a lossless, in-order link with random latency.
// Checks every console computes identical hashes, and that a forced desync is detected quickly.
#include "../src/sim/sim.h"
#include "../src/net/lockstep.h"
#include <cstdio>
#include <deque>
#include <vector>
#include <random>

struct msg { int deliver_at; int from; uint16_t pkt; };

struct console
{
    int id;
    sim::state s;
    net::lockstep ls;
    std::deque<msg> inbox;
    std::vector<uint32_t> hashes;
    int stalls = 0;
};

static int run(int players, int ticks, int perturb_frame, unsigned seed)
{
    std::mt19937 rng(seed);
    std::vector<console> c(players);
    for(int i = 0; i < players; ++i)
    {
        c[i].id = i;
        sim::init(c[i].s, players, 77);
        c[i].ls.start(i, players);
    }
    for(int t = 0; t < ticks; ++t)
    {
        for(auto& k : c)
        {
            while(! k.inbox.empty() && k.inbox.front().deliver_at <= t)
            {
                k.ls.receive(k.inbox.front().from, k.inbox.front().pkt);
                k.inbox.pop_front();
            }
            if(k.ls.state() != net::status::OK)
            {
                printf("  players=%d console %d status=%d at frame %u (bad frame %u, mine %08x slices %x %x %x %x)\n",
                       players, k.id, int(k.ls.state()), k.ls.frame(), k.ls.bad_frame, k.ls.my_hash,
                       k.ls.slices[0], k.ls.slices[1], k.ls.slices[2], k.ls.slices[3]);
                return int(k.ls.frame());
            }
            if(! k.ls.ready()) { ++k.stalls; continue; }
            // Local input: the AI plays for everyone (via its own slot), occasionally pauses.
            uint8_t b = sim::ai_input(k.s, k.id);
            if((k.ls.frame() % 3001) == 1500 + unsigned(k.id)) b |= sim::BTN_START;
            uint16_t pkt = k.ls.local_input(b);
            if(pkt == 0 || pkt == 0xFFFF) { printf("reserved packet!\n"); return -2; }
            int lat = int(rng() % 3);            // 0..2 frames of latency
            for(auto& o : c)
                if(o.id != k.id)
                {
                    int at = t + lat;
                    if(! o.inbox.empty() && o.inbox.back().deliver_at > at) at = o.inbox.back().deliver_at;
                    o.inbox.push_back({at, k.id, pkt});
                }
            uint8_t in[4];
            k.ls.inputs(in);
            sim::step(k.s, in);
            if(perturb_frame >= 0 && k.id == players - 1 && k.ls.frame() == unsigned(perturb_frame))
                k.s.ball.x += 1;
            uint32_t h = sim::hash(k.s);
            k.ls.finish_frame(h);
            k.hashes.push_back(h);
        }
    }
    size_t n = c[0].hashes.size();
    for(auto& k : c) if(k.hashes.size() < n) n = k.hashes.size();
    for(size_t f = 0; f < n; ++f)
        for(auto& k : c)
            if(k.hashes[f] != c[0].hashes[f]) { printf("  MISMATCH frame %zu\n", f); return -1; }
    printf("players=%d ticks=%d frames=%zu stalls=%d,%d lives=%d,%d,%d,%d OK\n", players, ticks, n, c[0].stalls,
           c[players - 1].stalls, c[0].s.lives[0], c[0].s.lives[1], c[0].s.lives[2], c[0].s.lives[3]);
    return 0;
}

int main()
{
    int fail = 0;
    for(int p = 2; p <= 4; ++p)
        fail |= run(p, 36000, -1, 1000 + p) != 0;   // 10 minutes each
    for(int p = 2; p <= 4; ++p)
    {
        int at = run(p, 4000, 500, 7);
        int latency = at - 500;
        printf("  forced desync at frame 500 detected at frame %d (%d frames later)\n", at, latency);
        if(at < 0 || latency > 60) fail = 1;
    }
    printf(fail ? "FAIL\n" : "ALL OK\n");
    return fail;
}
