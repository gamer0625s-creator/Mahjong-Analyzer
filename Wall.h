/*
 * Wall.h — 日本麻將牌山管理模組
 *
 * 職責：
 *   1. 從完整牌山（136張 = 每種牌4張）出發
 *   2. 扣除自家手牌
 *   3. 收集「場上已見牌」（寶牌指示牌、各家棄牌）後扣除
 *   4. 回傳剩餘牌山計數陣列，供 tenpai_calc() 使用
 *
 * 已見牌來源：
 *   - 寶牌指示牌 (Dora indicator)：場上可見，直接扣除
 *   - 各家棄牌 (Discards)：上家、對家、下家的所有出牌皆可見
 *   - 自家打出牌：若分析的是打出某張後的13張，需一併扣除
 *
 * 注意：
 *   - 本模組不做「某張牌已超過4張」的硬性錯誤處理，
 *     但提供 wall_validate() 供使用者確認輸入合法性。
 *   - 河底牌（最後一張棄牌）在規則上不影響牌山剩餘計數，
 *     僅影響「搶槓和」等特殊判斷，本模組不另行區分。
 *
 * 公開介面：
 *   void wall_init(Wall *w)
 *     初始化牌山為每種牌4張。
 *
 *   int  wall_remove(Wall *w, int tile_idx)
 *     從牌山移除一張牌。成功回傳1，牌山已無此牌回傳0。
 *
 *   void wall_remove_hand(Wall *w, const int hand[TILE_TYPES])
 *     批次移除自家手牌。
 *
 *   void wall_remove_seen(Wall *w, const int seen[TILE_TYPES])
 *     批次移除已見牌（各家棄牌、寶牌指示牌等）。
 *
 *   int  wall_validate(const Wall *w)
 *     確認牌山各項目 >= 0，回傳1=合法，0=有牌超用。
 *
 *   const int* wall_counts(const Wall *w)
 *     取得剩餘計數陣列指標，直接傳給 tenpai_calc()。
 */

#ifndef WALL_H
#define WALL_H

#include <string.h>

#ifndef TILE_TYPES
#define TILE_TYPES 34
#endif

/* ══════════════════════════════════════════════════════════════
   Wall 結構
══════════════════════════════════════════════════════════════ */
typedef struct {
    int counts[TILE_TYPES]; /* 牌山各牌剩餘張數 */
} Wall;

/* 初始化：每種牌4張，共136張 */
static void wall_init(Wall *w) {
    for (int i = 0; i < TILE_TYPES; i++)
        w->counts[i] = 4;
}

/* 移除一張牌；回傳1=成功，0=牌山已無此牌 */
static int wall_remove(Wall *w, int tile_idx)
    __attribute__((unused));
static int wall_remove(Wall *w, int tile_idx) {
    if (tile_idx < 0 || tile_idx >= TILE_TYPES) return 0;
    if (w->counts[tile_idx] <= 0) return 0;
    w->counts[tile_idx]--;
    return 1;
}

/* 批次移除自家手牌 */
static void wall_remove_hand(Wall *w, const int hand[TILE_TYPES]) {
    for (int i = 0; i < TILE_TYPES; i++) {
        w->counts[i] -= hand[i];
        if (w->counts[i] < 0) w->counts[i] = 0;
    }
}

/* 批次移除已見牌（棄牌、寶牌指示牌等） */
static void wall_remove_seen(Wall *w, const int seen[TILE_TYPES]) {
    for (int i = 0; i < TILE_TYPES; i++) {
        w->counts[i] -= seen[i];
        if (w->counts[i] < 0) w->counts[i] = 0;
    }
}

/* 合法性驗證：若有任何項目 < 0 回傳0 */
static int wall_validate(const Wall *w) {
    for (int i = 0; i < TILE_TYPES; i++)
        if (w->counts[i] < 0) return 0;
    return 1;
}

/* 取得剩餘計數陣列（唯讀指標） */
static const int *wall_counts(const Wall *w) {
    return w->counts;
}

/* ══════════════════════════════════════════════════════════════
   總計剩餘張數
══════════════════════════════════════════════════════════════ */
static int wall_total(const Wall *w) {
    int t = 0;
    for (int i = 0; i < TILE_TYPES; i++) t += w->counts[i];
    return t;
}

#endif /* WALL_H */
