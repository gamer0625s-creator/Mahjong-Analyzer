/*
 * Ron.h — 日本麻將和牌判斷模組 (支援副露)
 */
#ifndef RON_H
#define RON_H

#include "Meld.h"

typedef struct {
    int normal;      /* 1 = 標準和牌 */
    int chiitoitsu;  /* 1 = 七對子 */
    int kokushi;     /* 1 = 國士無雙 */
    int ryuuiisou;   /* 1 = 綠一色 */
    int tsuuiisou;   /* 1 = 字一色 */
    int daisangen;   /* 1 = 大三元 */
    int tanyao;      /* 1 = 斷么九 */
} RonResult;

static int ron__first_tile(const int h[TILE_TYPES]) {
    for (int i = 0; i < TILE_TYPES; i++)
        if (h[i] > 0) return i;
    return -1;
}

static int ron__can_form_melds(int h[TILE_TYPES], int tiles_left) {
    if (tiles_left == 0) return 1;
    int i = ron__first_tile(h);
    if (i < 0) return 0;

    if (h[i] >= 3) {
        h[i] -= 3;
        if (ron__can_form_melds(h, tiles_left - 3)) { h[i] += 3; return 1; }
        h[i] += 3;
    }
    int suit = i / 9;
    if (suit < 3) {
        int j = i, k = i + 1, l = i + 2;
        if (l < suit * 9 + 9 && h[k] > 0 && h[l] > 0) {
            h[j]--; h[k]--; h[l]--;
            if (ron__can_form_melds(h, tiles_left - 3)) {
                h[j]++; h[k]++; h[l]++;
                return 1;
            }
            h[j]++; h[k]++; h[l]++;
        }
    }
    return 0;
}

/* 傳入固定面子數，只需湊滿 (4 - num_melds) 個面子 */
static int ron_check_normal(const PlayerHand* ph) {
    int h[TILE_TYPES];
    memcpy(h, ph->closed_hand, sizeof(int) * TILE_TYPES);
    int target_tiles = (4 - ph->num_melds) * 3;

    for (int i = 0; i < TILE_TYPES; i++) {
        if (h[i] >= 2) {
            h[i] -= 2;
            if (ron__can_form_melds(h, target_tiles)) { h[i] += 2; return 1; }
            h[i] += 2;
        }
    }
    return 0;
}

static int ron_check_chiitoitsu(const int hand[TILE_TYPES]) {
    int pairs = 0;
    for (int i = 0; i < TILE_TYPES; i++) {
        if (hand[i] == 2) pairs++;
        else if (hand[i] != 0) return 0;
    }
    return (pairs == 7);
}

static int ron_check_kokushi(const int hand[TILE_TYPES]) {
    static const int terminals[13] = { 0,8,9,17,18,26,27,28,29,30,31,32,33 };
    int pairs = 0;
    for (int t = 0; t < 13; t++) {
        int idx = terminals[t];
        if (hand[idx] == 0) return 0;
        if (hand[idx] >= 2) pairs++;
    }
    for (int i = 0; i < TILE_TYPES; i++) {
        int is_term = 0;
        for (int t = 0; t < 13; t++) if (terminals[t] == i) { is_term = 1; break; }
        if (!is_term && hand[i] > 0) return 0;
    }
    return (pairs == 1);
}

/* 役種判斷改吃「攤平後的所有牌」陣列 */
static int ron_check_ryuuiisou(const int all_tiles[TILE_TYPES]) {
    static const int green_tiles[] = { 19, 20, 21, 23, 25, 32 };
    for (int i = 0; i < TILE_TYPES; i++) {
        if (all_tiles[i] == 0) continue;
        int allowed = 0;
        for (int g = 0; g < 6; g++) if (green_tiles[g] == i) { allowed = 1; break; }
        if (!allowed) return 0;
    }
    return 1;
}

static int ron_check_tsuuiisou(const int all_tiles[TILE_TYPES]) {
    for (int i = 0; i < 27; i++)
        if (all_tiles[i] > 0) return 0;
    return 1;
}

static int ron_check_daisangen(const int all_tiles[TILE_TYPES]) {
    if (all_tiles[31] < 3 || all_tiles[32] < 3 || all_tiles[33] < 3) return 0;
    return 1;
}

static int ron_check_tanyao(const int all_tiles[TILE_TYPES]) {
    static const int terminals[] = { 0,8,9,17,18,26,27,28,29,30,31,32,33 };
    for (int t = 0; t < 13; t++)
        if (all_tiles[terminals[t]] > 0) return 0;
    return 1;
}

/* 主入口 */
static int ron_check(const PlayerHand* ph, RonResult* result) {
    int is_menzen = 1;
    for (int i = 0; i < ph->num_melds; i++) {
        if (ph->melds[i].type != MELD_KAN_CLOSED) { is_menzen = 0; break; }
    }

    int normal = ron_check_normal(ph);
    int chiitoitsu = (ph->num_melds == 0) ? ron_check_chiitoitsu(ph->closed_hand) : 0;
    int kokushi = (ph->num_melds == 0) ? ron_check_kokushi(ph->closed_hand) : 0;

    int all_tiles[TILE_TYPES] = { 0 };
    playerhand_get_all_tiles(ph, all_tiles);

    int ryuuiisou = (normal || chiitoitsu) ? ron_check_ryuuiisou(all_tiles) : 0;
    int tsuuiisou = (normal || chiitoitsu) ? ron_check_tsuuiisou(all_tiles) : 0;
    int daisangen = normal ? ron_check_daisangen(all_tiles) : 0;
    int tanyao = normal ? ron_check_tanyao(all_tiles) : 0;

    if (result) {
        result->normal = normal;
        result->chiitoitsu = chiitoitsu;
        result->kokushi = kokushi;
        result->ryuuiisou = ryuuiisou;
        result->tsuuiisou = tsuuiisou;
        result->daisangen = daisangen;
        result->tanyao = tanyao;
    }
    return (normal || chiitoitsu || kokushi);
}
#endif
