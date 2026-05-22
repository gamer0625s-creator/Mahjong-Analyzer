//#pragma once
/*
 * Meld.h — 日本麻將基礎資料結構 (支援副露)
 */
#ifndef MELD_H
#define MELD_H

#include <string.h>

#ifndef TILE_TYPES
#define TILE_TYPES 34
#endif

 /* 副露的種類 */
typedef enum {
    MELD_CHI,         /* 順子 (吃) */
    MELD_PON,         /* 刻子 (碰) */
    MELD_KAN_OPEN,    /* 明槓 (大明槓/加槓) */
    MELD_KAN_CLOSED   /* 暗槓 */
} MeldType;

/* 紀錄單一組副露 */
typedef struct {
    MeldType type;
    int tiles[4];     /* 記錄組成副露的具體牌索引，沒有的填 -1 */
} Meld;

/* 封裝玩家完整手牌狀態 */
typedef struct {
    int closed_hand[TILE_TYPES]; /* 門清狀態的純手牌 */
    Meld melds[4];               /* 最多 4 組副露 */
    int num_melds;               /* 目前有幾組副露 (0~4) */
    int closed_count;            /* 門清手牌總張數 */
} PlayerHand;

/* 輔助：將包含副露的完整手牌「攤平」為 34 格陣列（用於算綠一色、字一色、拔除牌山等） */
static void playerhand_get_all_tiles(const PlayerHand* ph, int all_tiles[TILE_TYPES]) {
    memcpy(all_tiles, ph->closed_hand, sizeof(int) * TILE_TYPES);
    for (int i = 0; i < ph->num_melds; i++) {
        for (int j = 0; j < 4; j++) {
            if (ph->melds[i].tiles[j] != -1) {
                all_tiles[ph->melds[i].tiles[j]]++;
            }
        }
    }
}

#endif /* MELD_H */