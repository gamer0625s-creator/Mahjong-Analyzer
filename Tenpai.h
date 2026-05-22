/*
 * Tenpai.h — 日本麻將向聽數與聽牌分析模組 (支援副露)
 */
#ifndef TENPAI_H
#define TENPAI_H

#include "Meld.h"

 /* 13張/未滿足和牌張數時的向聽數計算 (內部演算法完全不變，僅傳入起點 melds) */
static void tenpai__rec(int h[TILE_TYPES], int idx, int melds, int partials, int has_pair, int* best) {
    if (*best == -1) return;
    int pa = partials;
    if (melds + pa > 4) pa = 4 - melds;
    if (pa < 0) pa = 0;
    int s = 8 - 2 * melds - pa - has_pair;
    if (s < *best) *best = s;
    if (*best == -1) return;

    while (idx < TILE_TYPES && h[idx] == 0) idx++;
    if (idx >= TILE_TYPES) return;

    int suit = idx / 9;
    int end = suit * 9 + 9;

    if (h[idx] >= 3) {
        h[idx] -= 3;
        tenpai__rec(h, idx, melds + 1, partials, has_pair, best);
        h[idx] += 3;
    }
    if (suit < 3 && idx + 2 < end && h[idx + 1] > 0 && h[idx + 2] > 0) {
        h[idx]--; h[idx + 1]--; h[idx + 2]--;
        tenpai__rec(h, idx, melds + 1, partials, has_pair, best);
        h[idx]++; h[idx + 1]++; h[idx + 2]++;
    }
    if (!has_pair && h[idx] >= 2) {
        h[idx] -= 2;
        tenpai__rec(h, idx, melds, partials, 1, best);
        h[idx] += 2;
    }
    if (h[idx] >= 2) {
        h[idx] -= 2;
        tenpai__rec(h, idx, melds, partials + 1, has_pair, best);
        h[idx] += 2;
    }
    if (suit < 3 && idx + 1 < end && h[idx + 1] > 0) {
        h[idx]--; h[idx + 1]--;
        tenpai__rec(h, idx, melds, partials + 1, has_pair, best);
        h[idx]++; h[idx + 1]++;
    }
    if (suit < 3 && idx + 2 < end && h[idx + 2] > 0) {
        h[idx]--; h[idx + 2]--;
        tenpai__rec(h, idx, melds, partials + 1, has_pair, best);
        h[idx]++; h[idx + 2]++;
    }
    tenpai__rec(h, idx + 1, melds, partials, has_pair, best);
}

static int tenpai__s13_normal(const PlayerHand* ph) {
    int h[TILE_TYPES];
    memcpy(h, ph->closed_hand, sizeof(int) * TILE_TYPES);
    int best = 8;
    tenpai__rec(h, 0, ph->num_melds, 0, 0, &best);
    return best;
}

static int tenpai_shanten(const PlayerHand* ph) {
    int s = tenpai__s13_normal(ph);
    if (ph->num_melds == 0) {
        int pairs = 0, kinds = 0, kokushi_have = 0, kokushi_pair = 0;
        static const int T[13] = { 0,8,9,17,18,26,27,28,29,30,31,32,33 };

        for (int i = 0; i < TILE_TYPES; i++) {
            if (ph->closed_hand[i] >= 1) kinds++;
            if (ph->closed_hand[i] >= 2) pairs++;
        }
        for (int t = 0; t < 13; t++) {
            if (ph->closed_hand[T[t]] >= 1) kokushi_have++;
            if (ph->closed_hand[T[t]] >= 2) kokushi_pair = 1;
        }

        int nk = (kinds < 7) ? (7 - kinds) : 0;
        int np = 7 - pairs;
        int sc = (np > nk ? np : nk) - 1;
        int sk = 13 - kokushi_have - kokushi_pair;

        if (sc < s) s = sc;
        if (sk < s) s = sk;
    }
    return s;
}

#define TENPAI_MAX_WAITS   34
#define TENPAI_MAX_OPTIONS 34

typedef struct { int tile; int remaining; } TenpaiWait;
typedef struct {
    int discard; int n_waits;
    TenpaiWait waits[TENPAI_MAX_WAITS];
    int total_remaining;
} TenpaiOption;

typedef struct {
    int shanten; int n_options;
    TenpaiOption options[TENPAI_MAX_OPTIONS];
} TenpaiResult;

static void tenpai__sort_waits(TenpaiWait* w, int n) {
    for (int i = 1; i < n; i++) {
        TenpaiWait k = w[i]; int j = i - 1;
        while (j >= 0 && w[j].remaining < k.remaining) { w[j + 1] = w[j]; j--; }
        w[j + 1] = k;
    }
}
static void tenpai__sort_options(TenpaiOption* o, int n) {
    for (int i = 1; i < n; i++) {
        TenpaiOption k = o[i]; int j = i - 1;
        while (j >= 0 && o[j].total_remaining < k.total_remaining) { o[j + 1] = o[j]; j--; }
        o[j + 1] = k;
    }
}

static void tenpai_calc(const PlayerHand* ph, const int wall[TILE_TYPES], TenpaiResult* out) {
    out->n_options = 0;
    int best_s13 = 99;

    /* Step 1：枚舉打出每一張門清牌，找最佳向聽數 */
    for (int d = 0; d < TILE_TYPES; d++) {
        if (ph->closed_hand[d] == 0) continue;
        PlayerHand temp_ph = *ph;
        temp_ph.closed_hand[d]--;
        temp_ph.closed_count--;
        int s = tenpai_shanten(&temp_ph);
        if (s < best_s13) best_s13 = s;
    }
    out->shanten = best_s13;

    /* Step 2：對每種最佳打法做聽牌枚舉 */
    for (int d = 0; d < TILE_TYPES; d++) {
        if (ph->closed_hand[d] == 0) continue;
        PlayerHand temp_ph = *ph;
        temp_ph.closed_hand[d]--;
        temp_ph.closed_count--;

        if (tenpai_shanten(&temp_ph) != best_s13) continue;

        TenpaiOption opt;
        opt.discard = d;
        opt.n_waits = 0;
        opt.total_remaining = 0;

        if (best_s13 == 0) {
            for (int w = 0; w < TILE_TYPES; w++) {
                int rem;
                if (wall) rem = wall[w];
                else {
                    int all_tiles[TILE_TYPES] = { 0 };
                    playerhand_get_all_tiles(&temp_ph, all_tiles);
                    rem = 4 - all_tiles[w];
                    if (rem < 0) rem = 0;
                }
                if (rem <= 0) continue;

                PlayerHand check_ph = temp_ph;
                check_ph.closed_hand[w]++;
                check_ph.closed_count++;

                if (!ron_check(&check_ph, NULL)) continue;

                opt.waits[opt.n_waits].tile = w;
                opt.waits[opt.n_waits].remaining = rem;
                opt.total_remaining += rem;
                opt.n_waits++;
            }
            if (opt.n_waits == 0) continue;
        }
        tenpai__sort_waits(opt.waits, opt.n_waits);
        if (out->n_options < TENPAI_MAX_OPTIONS) out->options[out->n_options++] = opt;
    }
    tenpai__sort_options(out->options, out->n_options);
}

typedef struct {
    int ron_furiten;
    int furiten_tiles[TILE_TYPES];
    int n_furiten;
} FuritenResult;

static void furiten_check(const TenpaiOption* opt, const int own_discards[TILE_TYPES], FuritenResult* out) {
    out->ron_furiten = 0;
    out->n_furiten = 0;
    for (int j = 0; j < opt->n_waits; j++) {
        int tile = opt->waits[j].tile;
        if (own_discards[tile] > 0) {
            out->furiten_tiles[out->n_furiten++] = tile;
            out->ron_furiten = 1;
        }
    }
}
#endif