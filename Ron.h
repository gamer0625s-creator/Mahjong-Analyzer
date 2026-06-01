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
    int ippeiko;     /* 1 = 一盃口 (門前清限定) */
    int riichi;      /* 1 = 立直 (門前清限定，由外部輸入設定) */
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

/* 傳入固定面子數，只需湊滿剩餘面子數個面子
   注意：MELD_ANKOU 的牌仍存在 closed_hand，需先扣除再計算 */
static int ron_check_normal(const PlayerHand* ph) {
    int h[TILE_TYPES];
    memcpy(h, ph->closed_hand, sizeof(int) * TILE_TYPES);

    /* 計算真正需要從 closed_hand 組出幾個面子（已確定的副露不計） */
    int fixed_melds = 0;
    for (int i = 0; i < ph->num_melds; i++) {
        MeldType t = ph->melds[i].type;
        if (t == MELD_ANKOU) {
            /* 暗刻的牌在 closed_hand，先扣除再當成已完成面子 */
            int tile = ph->melds[i].tiles[0];
            h[tile] -= 3;
            if (h[tile] < 0) return 0; /* 資料有誤 */
            fixed_melds++;
        } else {
            /* 碰/吃/槓：牌不在 closed_hand，直接算完成面子 */
            fixed_melds++;
        }
    }
    int target_tiles = (4 - fixed_melds) * 3;

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

/*
 * 一盃口：門前清，手牌中存在兩組相同的順子。
 * DFS 枚舉所有面子拆解，若任一拆解中某順子出現 >= 2 次即成立。
 */
static void ron__ippeiko_rec(int h[TILE_TYPES], int idx,
                              int chi[TILE_TYPES], int* found) {
    if (*found) return;
    while (idx < TILE_TYPES && h[idx] == 0) idx++;
    if (idx >= TILE_TYPES) {
        for (int i = 0; i < TILE_TYPES; i++)
            if (chi[i] >= 2) { *found = 1; return; }
        return;
    }
    int suit = idx / 9;
    /* 嘗試刻子 */
    if (h[idx] >= 3) {
        h[idx] -= 3;
        ron__ippeiko_rec(h, idx, chi, found);
        h[idx] += 3;
        if (*found) return;
    }
    /* 嘗試順子 */
    if (suit < 3 && idx + 2 < suit * 9 + 9 && h[idx+1] > 0 && h[idx+2] > 0) {
        h[idx]--; h[idx+1]--; h[idx+2]--;
        chi[idx]++;
        ron__ippeiko_rec(h, idx, chi, found);
        chi[idx]--;
        h[idx]++; h[idx+1]++; h[idx+2]++;
        if (*found) return;
    }
}

static int ron_check_ippeiko(const PlayerHand* ph) {
    /* 需門前清：不能有碰/吃/明槓 */
    for (int i = 0; i < ph->num_melds; i++) {
        MeldType t = ph->melds[i].type;
        if (t == MELD_PON || t == MELD_CHI || t == MELD_KAN_OPEN) return 0;
    }
    int h[TILE_TYPES];
    memcpy(h, ph->closed_hand, sizeof(int) * TILE_TYPES);

    for (int p = 0; p < TILE_TYPES; p++) {
        if (h[p] < 2) continue;
        h[p] -= 2;
        int chi[TILE_TYPES] = {0};
        int found = 0;
        ron__ippeiko_rec(h, 0, chi, &found);
        h[p] += 2;
        if (found) return 1;
    }
    return 0;
}

/* 主入口 */
static int ron_check(const PlayerHand* ph, RonResult* result) {
    /* 門前清：只允許暗刻(ANKOU)和暗槓(KAN_CLOSED)，碰/吃/明槓則非門前清 */
    int is_menzen = 1;
    int has_open = 0;
    for (int i = 0; i < ph->num_melds; i++) {
        MeldType t = ph->melds[i].type;
        if (t == MELD_PON || t == MELD_CHI || t == MELD_KAN_OPEN) {
            is_menzen = 0; has_open = 1; break;
        }
    }

    int normal = ron_check_normal(ph);
    /* 七對子/國士：不能有副露（暗刻/暗槓也視為已確定面子，不符七對子） */
    int chiitoitsu = (ph->num_melds == 0) ? ron_check_chiitoitsu(ph->closed_hand) : 0;
    int kokushi    = (ph->num_melds == 0) ? ron_check_kokushi(ph->closed_hand) : 0;

    int all_tiles[TILE_TYPES] = { 0 };
    playerhand_get_all_tiles(ph, all_tiles);

    int ryuuiisou = (normal || chiitoitsu) ? ron_check_ryuuiisou(all_tiles) : 0;
    int tsuuiisou = (normal || chiitoitsu) ? ron_check_tsuuiisou(all_tiles) : 0;
    int daisangen = normal ? ron_check_daisangen(all_tiles) : 0;
    int tanyao = normal ? ron_check_tanyao(all_tiles) : 0;
    int ippeiko = (normal && is_menzen) ? ron_check_ippeiko(ph) : 0;

    if (result) {
        result->normal = normal;
        result->chiitoitsu = chiitoitsu;
        result->kokushi = kokushi;
        result->ryuuiisou = ryuuiisou;
        result->tsuuiisou = tsuuiisou;
        result->daisangen = daisangen;
        result->tanyao = tanyao;
        result->ippeiko = ippeiko;
        result->riichi = 0;   /* 由外部（main）在詢問玩家後設定 */
    }
    (void)has_open;
    return (normal || chiitoitsu || kokushi);
}
#endif
