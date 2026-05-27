/*
 * ScoreCalc.h — 日本麻將符數與番數計算模組
 *
 * 支援：
 *   - 一般型 (standard hand)：枚舉所有合法面子拆解，逐一計算符/番，取最大番數組合
 *   - 七對子：固定 25符、2番
 *   - 國士無雙：役滿，不計符/番
 *
 * 符數計算（一般型）：
 *   符底          20符
 *   門前清榮和    +10符   (num_melds==0 且非自摸)
 *   自摸          +2符
 *   嵌張/邊張/單騎聽牌 +2符
 *   面子加符：
 *     順子(Chi)   0符
 *     明刻(pon)   么九=4, 中張=2
 *     暗刻        么九=8, 中張=4
 *     明槓(open)  么九=16, 中張=8
 *     暗槓        么九=32, 中張=16
 *   雀頭加符：
 *     役牌(三元/場風/自風)  +2符
 *     其他                   0符
 *   最後無條件進位到10符倍數
 *     例外：門前清自摸固定2符、七對子固定25符不進位
 *
 * 番數計算（只計入手中已確定役，不含立直/一發等需外部資訊的役）：
 *   斷么九(Tanyao)         1番
 *   一盃口(Ippeiko)        1番  (門前清限定)
 *   役牌(Yakuhai)          1番/組 (白板/發財/中/場風/自風)
 *   門前清自摸和(Tsumo)    1番  (門前清限定)
 *   平和(Pinfu)            1番  (門前清限定，符底+聽牌符=0，全順子，非役牌雀頭)
 *   混全帶么九(Chanta)     2番(門前清) / 1番(副露)
 *   純全帶么九(Junchan)    3番(門前清) / 2番(副露)
 *   混一色(Honitsu)        3番(門前清) / 2番(副露)
 *   清一色(Chinitsu)       6番(門前清) / 5番(副露)
 *   七對子(Chiitoitsu)     2番, 25符
 *   役滿：
 *     國士無雙  kokushi
 *     大三元    daisangen
 *     綠一色    ryuuiisou
 *     字一色    tsuuiisou
 */

#ifndef SCORE_CALC_H
#define SCORE_CALC_H

#include "Meld.h"
#include "Ron.h"
#include <string.h>
#define DEBUG 0

/* ══════════════════════════════════════════════════════════════
   輔助：牌的性質判斷
══════════════════════════════════════════════════════════════ */

/* 是否么九牌（1/9/字牌） */
static int sc__is_yaochuuhai(int tile) {
    if (tile < 0) return 0;
    if (tile >= 27) return 1;       /* 字牌 */
    int n = tile % 9;
    return (n == 0 || n == 8);     /* 1m/9m, 1p/9p, 1s/9s */
}

/* 是否役牌：三元牌=31(白),32(発),33(中)，場風/自風需外部傳入 (0=東,1=南,2=西,3=北 → idx 27~30) */
static int sc__is_yakuhai(int tile, int round_wind, int seat_wind) {
    if (tile >= 31 && tile <= 33) return 1;      /* 三元牌 */
    if (tile == 27 + round_wind)  return 1;      /* 場風 */
    if (tile == 27 + seat_wind)   return 1;      /* 自風 */
    return 0;
}

/* 是否中張（2~8 of suits） */
static int sc__is_simples(int tile) {
    if (tile >= 27) return 0;
    int n = tile % 9;
    return (n >= 1 && n <= 7);
}

/* ══════════════════════════════════════════════════════════════
   面子/雀頭加符計算
══════════════════════════════════════════════════════════════ */

/* 面子加符 */
static int sc__meld_fu(MeldType type, int tile, int is_open_pon_from_closed) {
    /* 是否么九 */  
    int yao = sc__is_yaochuuhai(tile);
    switch (type) {
        case MELD_CHI:       return 0;
        case MELD_PON:       return yao ? 4 : 2;   /* 明刻 */
        case MELD_KAN_OPEN:  return yao ? 16 : 8;  /* 明槓 */
        case MELD_KAN_CLOSED:return yao ? 32 : 16;  /* 暗槓 */
        default:             return 0;
    }
    (void)is_open_pon_from_closed;
}

/* 暗刻加符（手牌中的刻子） */
static int sc__ankou_fu(int tile) {
    if(DEBUG)printf("ANPON Bug \n");
    return sc__is_yaochuuhai(tile) ? 8 : 4;   /* 暗刻 */ 
}

/* 雀頭加符 */
static int sc__pair_fu(int tile, int round_wind, int seat_wind) {
    return sc__is_yakuhai(tile, round_wind, seat_wind) ? 2 : 0;
}

/* ══════════════════════════════════════════════════════════════
   枚舉所有合法面子拆解 (DFS)
   用途：找到所有「雀頭 + 面子組合」，計算每種的符/番
══════════════════════════════════════════════════════════════ */

#define SC_MAX_DECOMP 64  /* 最多保存幾種拆解 */

typedef struct {
    int pair_tile;          /* 雀頭牌種 */
    int mentsu_tile[4];     /* 面子代表牌（順子取最小、刻子取該牌） */
    int mentsu_type[4];     /* 0=順子, 1=刻子 (手牌中的) */
    int n_mentsu;
} ScDecomp;

typedef struct {
    ScDecomp decomps[SC_MAX_DECOMP];
    int n;
} ScDecompList;

static void sc__enum_decomp(int h[TILE_TYPES], int idx, int need,
                             ScDecomp* cur, ScDecompList* out) {
    if (need == 0) {
        if (out->n < SC_MAX_DECOMP)
            out->decomps[out->n++] = *cur;
        return;
    }
    while (idx < TILE_TYPES && h[idx] == 0) idx++;
    if (idx >= TILE_TYPES) return;

    int suit = idx / 9;

    /* 嘗試刻子 */
    if (h[idx] >= 3) {
        h[idx] -= 3;
        cur->mentsu_tile[cur->n_mentsu] = idx;
        cur->mentsu_type[cur->n_mentsu] = 1;
        cur->n_mentsu++;
        sc__enum_decomp(h, idx, need - 1, cur, out);
        cur->n_mentsu--;
        h[idx] += 3;
    }
    /* 嘗試順子 */
    if (suit < 3 && idx + 2 < suit * 9 + 9 && h[idx+1] > 0 && h[idx+2] > 0) {
        h[idx]--; h[idx+1]--; h[idx+2]--;
        cur->mentsu_tile[cur->n_mentsu] = idx;
        cur->mentsu_type[cur->n_mentsu] = 0;
        cur->n_mentsu++;
        sc__enum_decomp(h, idx, need - 1, cur, out);
        cur->n_mentsu--;
        h[idx]++; h[idx+1]++; h[idx+2]++;
    }
    /* 跳過（不能不處理idx=0的情況，但這裡若刻子/順子都不成立就需跳到下一張） */
    /* 跳過當前牌（不用它）—— 實際上若刻子/順子都組不成，必須失敗 */
    /* 注意：這裡不加 skip，讓兩條路都走完即可 */
}

static void sc__enumerate(const PlayerHand* ph, ScDecompList* out) {
    out->n = 0;
    int h[TILE_TYPES];
    memcpy(h, ph->closed_hand, sizeof(int) * TILE_TYPES);
    int need = 4 - ph->num_melds;  /* 還需要幾個面子 */

    for (int p = 0; p < TILE_TYPES; p++) {
        if (h[p] < 2) continue;
        h[p] -= 2;
        ScDecomp cur;
        cur.pair_tile = p;
        cur.n_mentsu = 0;
        sc__enum_decomp(h, 0, need, &cur, out);
        h[p] += 2;
    }
}

/* ══════════════════════════════════════════════════════════════
   聽牌型態（符）：嵌張/邊張/單騎
   呼叫時傳入 13張手牌(打出1張後)的 ScDecomp，以及待牌 win_tile
══════════════════════════════════════════════════════════════ */
/* wait_fu：分析 win_tile 在此 decomp 中屬於哪種聽牌型態，回傳額外符數 */
static int sc__wait_fu(const ScDecomp* d, const Meld* open_melds, int n_open,
                       int win_tile) {
    /* 先重建 14 張面子（含副露）的完整面子清單 */
    /* 檢查 win_tile 完成的是哪個面子 / 雀頭 */

    /* 單騎（雀頭是 win_tile） */
    if (d->pair_tile == win_tile) {
        /* 確認 win_tile 確實沒出現在任何面子中（或說雀頭是最後聽的） */
        /* 簡單判定：若此 decomp 的所有面子都能不用 win_tile 組成，
           而雀頭需要 win_tile，那就是單騎 */
        return 2;  /* 單騎 +2符 */
    }

    /* 查找 win_tile 完成的面子 */
    for (int m = 0; m < d->n_mentsu; m++) {
        int t = d->mentsu_tile[m];
        int type = d->mentsu_type[m];
        if (type == 1) {
            /* 刻子：win_tile == t → 雙碰聽 0符 */
            if (t == win_tile) return 0;
        } else {
            /* 順子：t, t+1, t+2 */
            if (win_tile == t || win_tile == t+1 || win_tile == t+2) {
                /* 嵌張：win_tile == t+1（中間張） */
                if (win_tile == t + 1) return 2;
                /* 邊張：1-2-[3] 或 [7]-8-9 */
                int suit = t / 9;
                int suit_end = suit * 9 + 8; /* 最大索引 */
                int suit_start = suit * 9;
                if (win_tile == t + 2 && t == suit_start) return 2; /* 123邊張聽3 */
                if (win_tile == t     && t + 2 == suit_end) return 2; /* 789邊張聽7 */
                /* 其他兩面聽 0符 */
                return 0;
            }
        }
    }
    /* win_tile 沒有出現在手牌面子中 → 可能在副露，副露不算聽牌符 */
    return 0;
    (void)open_melds; (void)n_open;
}

/* ══════════════════════════════════════════════════════════════
   役種分析（用於符/番計算，需要面子拆解資訊）
══════════════════════════════════════════════════════════════ */

/* 平和判定：全順子 + 非役牌雀頭 + 兩面聽 */
static int sc__is_pinfu(const ScDecomp* d, const PlayerHand* ph,
                         int win_tile, int round_wind, int seat_wind) {
    if (ph->num_melds != 0) return 0;  /* 需門前清 */
    /* 雀頭不能是役牌 */
    if (sc__is_yakuhai(d->pair_tile, round_wind, seat_wind)) return 0;
    /* 全部面子必須是順子 */
    for (int m = 0; m < d->n_mentsu; m++)
        if (d->mentsu_type[m] != 0) return 0;
    /* 聽牌型態必須是兩面（wait_fu == 0 且非單騎） */
    if (d->pair_tile == win_tile) return 0;
    int wf = sc__wait_fu(d, ph->melds, ph->num_melds, win_tile);
    return (wf == 0);
}

/* 一盃口：門前清，兩組相同順子 */
static int sc__is_ippeiko(const ScDecomp* d, const PlayerHand* ph) {
    if (ph->num_melds != 0) return 0;
    /* 統計各順子出現次數 */
    int chi_count[TILE_TYPES] = {0};
    for (int m = 0; m < d->n_mentsu; m++)
        if (d->mentsu_type[m] == 0)
            chi_count[d->mentsu_tile[m]]++;
    for (int i = 0; i < TILE_TYPES; i++)
        if (chi_count[i] >= 2) return 1;
    return 0;
}

/* 役牌番數（三元＋場風＋自風，每組刻子算1番） */
static int sc__yakuhai_han(const ScDecomp* d, const PlayerHand* ph,
                            int round_wind, int seat_wind) {
    int han = 0;
    /* 手牌中的刻子 */
    for (int m = 0; m < d->n_mentsu; m++)
        if (d->mentsu_type[m] == 1 &&
            sc__is_yakuhai(d->mentsu_tile[m], round_wind, seat_wind))
            han++;
    /* 副露中的刻子/槓 */
    for (int i = 0; i < ph->num_melds; i++) {
        int t = ph->melds[i].tiles[0];
        if ((ph->melds[i].type == MELD_PON ||
             ph->melds[i].type == MELD_KAN_OPEN ||
             ph->melds[i].type == MELD_KAN_CLOSED) &&
            sc__is_yakuhai(t, round_wind, seat_wind))
            han++;
    }
    return han;
}

/* 混全帶么九(Chanta)：每個面子和雀頭都含么九 */
static int sc__is_chanta(const ScDecomp* d, const PlayerHand* ph) {
    /* 雀頭 */
    if (!sc__is_yaochuuhai(d->pair_tile)) return 0;
    /* 手牌面子 */
    for (int m = 0; m < d->n_mentsu; m++) {
        int t = d->mentsu_tile[m];
        int type = d->mentsu_type[m];
        if (type == 1) { /* 刻子 */
            if (!sc__is_yaochuuhai(t)) return 0;
        } else { /* 順子：首尾之一需是么九 */
            if (!sc__is_yaochuuhai(t) && !sc__is_yaochuuhai(t+2)) return 0;
        }
    }
    /* 副露 */
    for (int i = 0; i < ph->num_melds; i++) {
        int t = ph->melds[i].tiles[0];
        if (ph->melds[i].type == MELD_CHI) {
            /* 順子首或尾含么九 */
            if (!sc__is_yaochuuhai(t) && !sc__is_yaochuuhai(t+2)) return 0;
        } else {
            if (!sc__is_yaochuuhai(t)) return 0;
        }
    }
    /* 需至少有一個順子（否則可能是混老頭） */
    int has_chi = 0;
    for (int m = 0; m < d->n_mentsu; m++) if (d->mentsu_type[m]==0) { has_chi=1; break; }
    for (int i = 0; i < ph->num_melds; i++) if (ph->melds[i].type==MELD_CHI) { has_chi=1; break; }
    return has_chi;
}

/* 純全帶么九(Junchan)：Chanta 且全部面子/雀頭只含么九（無字牌） */
static int sc__is_junchan(const ScDecomp* d, const PlayerHand* ph) {
    if (!sc__is_chanta(d, ph)) return 0;
    /* 字牌只能在刻子，Junchan 不允許字牌 */
    if (d->pair_tile >= 27) return 0;
    for (int m = 0; m < d->n_mentsu; m++)
        if (d->mentsu_tile[m] >= 27) return 0;
    for (int i = 0; i < ph->num_melds; i++)
        if (ph->melds[i].tiles[0] >= 27) return 0;
    return 1;
}

/* 混一色(Honitsu)：只用一種數牌花色 + 字牌 */
static int sc__is_honitsu(const int all_tiles[TILE_TYPES]) {
    int suit_used[3] = {0};
    for (int i = 0; i < 27; i++)
        if (all_tiles[i] > 0) suit_used[i/9] = 1;
    int count = suit_used[0] + suit_used[1] + suit_used[2];
    if (count != 1) return 0;
    /* 需有字牌才算混，否則是清一色 */
    for (int i = 27; i < TILE_TYPES; i++)
        if (all_tiles[i] > 0) return 1;
    return 0;
}

/* 清一色(Chinitsu)：只用一種數牌花色，無字牌 */
static int sc__is_chinitsu(const int all_tiles[TILE_TYPES]) {
    int suit_used[3] = {0};
    for (int i = 0; i < 27; i++)
        if (all_tiles[i] > 0) suit_used[i/9] = 1;
    if (suit_used[0] + suit_used[1] + suit_used[2] != 1) return 0;
    for (int i = 27; i < TILE_TYPES; i++)
        if (all_tiles[i] > 0) return 0;
    return 1;
}

/* ══════════════════════════════════════════════════════════════
   計算結果結構
══════════════════════════════════════════════════════════════ */
#define SC_YAKU_MAX 20

typedef struct {
    char name[48];
    int  han;
} ScYaku;

typedef struct {
    int  fu;              /* 符 (進位後) */
    int  han;             /* 番 */
    int  is_yakuman;      /* 1 = 役滿 */
    ScYaku yaku[SC_YAKU_MAX];
    int  n_yaku;
    char yakuman_name[64];/* 役滿名稱 */
    int  basic_pts;       /* 閒家基本點 (符 × 2^(番+2)，上限8000) */
    int  dealer_basic_pts;/* 莊家基本點 = 閒家基本點 × 1.5，上限12000 */
    char limit_name[20];  /* 滿貫等級名稱，無則為空字串 */
} ScoreResult;

/* 前置宣告（定義在 score_calc 之後） */
static void sc__calc_basic_pts(ScoreResult* s);

/* ══════════════════════════════════════════════════════════════
   寶牌計算
   dora_indicators[TILE_TYPES]：各張牌作為「寶牌指示牌」的數量
   寶牌 = 指示牌的下一張（數牌循環 1→2→...→9→1，字牌東南西北→循環，白発中→循環）
══════════════════════════════════════════════════════════════ */
static int sc__dora_tile(int indicator) {
    if (indicator < 0 || indicator >= TILE_TYPES) return -1;
    if (indicator < 27) {
        /* 數牌：同花色，9→1循環 */
        int suit  = indicator / 9;
        int num   = indicator % 9;           /* 0-based: 0=1, 8=9 */
        int next  = (num + 1) % 9;
        return suit * 9 + next;
    } else if (indicator < 31) {
        /* 風牌：東(27)南(28)西(29)北(30)→循環 */
        return 27 + (indicator - 27 + 1) % 4;
    } else {
        /* 三元牌：白(31)発(32)中(33)→循環 */
        return 31 + (indicator - 31 + 1) % 3;
    }
}

/* 計算手牌中的寶牌總數（all_tiles 已含副露） */
static int sc__count_dora(const int all_tiles[TILE_TYPES],
                           const int dora_indicators[TILE_TYPES]) {
    int total = 0;
    for (int i = 0; i < TILE_TYPES; i++) {
        if (dora_indicators[i] == 0) continue;
        int dora = sc__dora_tile(i);
        if (dora < 0) continue;
        total += all_tiles[dora] * dora_indicators[i];
    }
    return total;
}

/* ══════════════════════════════════════════════════════════════
   主計算函式
   win_tile        : 和牌（14張中最後摸/榮的那張，-1表示不確定）
   is_tsumo        : 1=自摸, 0=榮和
   round_wind      : 0=東, 1=南, 2=西, 3=北
   seat_wind       : 0=東, 1=南, 2=西, 3=北
   dora_indicators : 各牌作為指示牌的張數（NULL = 不算寶牌）
══════════════════════════════════════════════════════════════ */
static void score_calc(const PlayerHand* ph, const RonResult* ron,
                        int win_tile, int is_tsumo,
                        int round_wind, int seat_wind,
                        const int dora_indicators[TILE_TYPES],
                        ScoreResult* out) {
    memset(out, 0, sizeof(ScoreResult));

    int all_tiles[TILE_TYPES] = {0};
    playerhand_get_all_tiles(ph, all_tiles);

    int is_menzen = 1;
    for (int i = 0; i < ph->num_melds; i++)
        if (ph->melds[i].type != MELD_KAN_CLOSED) { is_menzen = 0; break; }

    /* ── 役滿優先 ── */
    if (ron->kokushi) {
        out->is_yakuman = 1;
        strcpy(out->yakuman_name, "Kokushi Musou (Thirteen Orphans)");
        out->han = 13; out->fu = 0;
        sc__calc_basic_pts(out); return;
    }
    if (ron->daisangen) {
        out->is_yakuman = 1;
        strcpy(out->yakuman_name, "Daisangen (Big Three Dragons)");
        out->han = 13; out->fu = 0;
        sc__calc_basic_pts(out); return;
    }
    if (ron->ryuuiisou) {
        out->is_yakuman = 1;
        strcpy(out->yakuman_name, "Ryuu Iisou (All Green)");
        out->han = 13; out->fu = 0;
        sc__calc_basic_pts(out); return;
    }
    if (ron->tsuuiisou) {
        out->is_yakuman = 1;
        strcpy(out->yakuman_name, "Tsuuiisou (All Honors)");
        out->han = 13; out->fu = 0;
        sc__calc_basic_pts(out); return;
    }

    /* ── 七對子 ── */
    if (ron->chiitoitsu) {
        out->fu = 25; out->han = 2;
        ScYaku* y = &out->yaku[out->n_yaku++];
        strcpy(y->name, "Chiitoitsu (Seven Pairs)"); y->han = 2;
        /* 寶牌 */
        if (dora_indicators) {
            int dora_han = sc__count_dora(all_tiles, dora_indicators);
            if (dora_han > 0) {
                ScYaku* yd = &out->yaku[out->n_yaku++];
                sprintf(yd->name, "Dora (%d han)", dora_han); yd->han = dora_han;
                out->han += dora_han;
            }
        }
        sc__calc_basic_pts(out);
        return;
    }

    /* ── 一般型：枚舉所有拆解，取最佳符/番 ── */
    ScDecompList dl;
    sc__enumerate(ph, &dl);

    int best_han = -1, best_fu = 0;
    int best_decomp_idx = 0;

    int is_chinitsu = sc__is_chinitsu(all_tiles);
    int is_honitsu  = !is_chinitsu && sc__is_honitsu(all_tiles);

    for (int di = 0; di < dl.n; di++) {
        ScDecomp* d = &dl.decomps[di];

        /* ── 符計算 ── */
        int fu = 20; /* 符底 */
        /* 門前清榮和 */
        if (is_menzen && !is_tsumo) fu += 10;
        /* 自摸（平和自摸不加自摸符） */
        int pinfu = sc__is_pinfu(d, ph, win_tile, round_wind, seat_wind);
        if (is_tsumo && !pinfu) fu += 2;
        /* 聽牌符 */
        if (win_tile >= 0 && !pinfu)
            fu += sc__wait_fu(d, ph->melds, ph->num_melds, win_tile);
        /* 雀頭符 */
        fu += sc__pair_fu(d->pair_tile, round_wind, seat_wind);
        /* 手牌面子符 */
        for (int m = 0; m < d->n_mentsu; m++) {
            int t = d->mentsu_tile[m];
            if (d->mentsu_type[m] == 0) {
                /* 順子 0符 */
            } else {
                fu += sc__ankou_fu(t);
            }
        }
        /* 副露面子符 */
        for (int i = 0; i < ph->num_melds; i++) {
            int t = ph->melds[i].tiles[0];
            fu += sc__meld_fu(ph->melds[i].type, t, 0);
            
        }

        /* 平和自摸固定20符（不進位另加） */
        if (pinfu && is_tsumo) fu = 20;
       
        /* 進位到10倍數（非平和自摸的情況） */
        if (!(pinfu && is_tsumo)) {
            fu = ((fu + 9) / 10) * 10;
         
        }
        
        /* ── 番計算 ── */
        int han = 0;

        /* 平和 */
        if (pinfu) han += 1;

        /* 斷么九 */
        if (ron->tanyao) han += 1;

        /* 一盃口 */
        if (is_menzen && sc__is_ippeiko(d, ph)) han += 1;

        /* 役牌 */
        han += sc__yakuhai_han(d, ph, round_wind, seat_wind);

        /* 門前清自摸 */
        if (is_menzen && is_tsumo) han += 1;

        /* Chanta / Junchan（互斥，取較高） */
        if (sc__is_junchan(d, ph)) {
            han += is_menzen ? 3 : 2;
        } else if (sc__is_chanta(d, ph)) {
            han += is_menzen ? 2 : 1;
        }

        /* 混一色 / 清一色 */
        if (is_chinitsu) han += is_menzen ? 6 : 5;
        else if (is_honitsu) han += is_menzen ? 3 : 2;

        /* 選最佳（優先番數，番數相同取符數高者） */
        if (han > best_han || (han == best_han && fu > best_fu)) {
            best_han = han;
            best_fu  = fu;
            best_decomp_idx = di;
        }
    }

    if (best_han < 0) {
        /* 無法計算（通常不應發生） */
        out->fu = 30; out->han = 0;
        return;
    }

    out->fu  = best_fu;
    out->han = best_han;

    /* ── 填入役種清單（用最佳 decomp） ── */
    ScDecomp* bd = &dl.decomps[best_decomp_idx];

    int pinfu_f = sc__is_pinfu(bd, ph, win_tile, round_wind, seat_wind);
    if (pinfu_f) { ScYaku* y=&out->yaku[out->n_yaku++]; strcpy(y->name,"Pinfu"); y->han=1; }
    if (ron->tanyao) { ScYaku* y=&out->yaku[out->n_yaku++]; strcpy(y->name,"Tanyao (All Simples)"); y->han=1; }
    if (is_menzen && sc__is_ippeiko(bd, ph)) { ScYaku* y=&out->yaku[out->n_yaku++]; strcpy(y->name,"Ippeiko (Pure Double Sequence)"); y->han=1; }
    if (is_menzen && is_tsumo) { ScYaku* y=&out->yaku[out->n_yaku++]; strcpy(y->name,"Menzen Tsumo"); y->han=1; }

    int yh = sc__yakuhai_han(bd, ph, round_wind, seat_wind);
    if (yh > 0) {
        ScYaku* y=&out->yaku[out->n_yaku++];
        sprintf(y->name, "Yakuhai x%d", yh); y->han=yh;
    }

    if (sc__is_junchan(bd, ph)) {
        ScYaku* y=&out->yaku[out->n_yaku++];
        int h2 = is_menzen ? 3 : 2;
        strcpy(y->name,"Junchan (Pure Terminal Chanta)"); y->han=h2;
    } else if (sc__is_chanta(bd, ph)) {
        ScYaku* y=&out->yaku[out->n_yaku++];
        int h2 = is_menzen ? 2 : 1;
        strcpy(y->name,"Chanta (Mixed Terminal/Honor Chanta)"); y->han=h2;
    }

    if (is_chinitsu) {
        ScYaku* y=&out->yaku[out->n_yaku++];
        int h2 = is_menzen ? 6 : 5;
        strcpy(y->name,"Chinitsu (Full Flush)"); y->han=h2;
    } else if (is_honitsu) {
        ScYaku* y=&out->yaku[out->n_yaku++];
        int h2 = is_menzen ? 3 : 2;
        strcpy(y->name,"Honitsu (Half Flush)"); y->han=h2;
    }

    /* ── 寶牌（役滿外通用，加在最後） ── */
    if (dora_indicators) {
        int dora_han = sc__count_dora(all_tiles, dora_indicators);
        if (dora_han > 0) {
            ScYaku* y = &out->yaku[out->n_yaku++];
            sprintf(y->name, "Dora "); 
            y->han = dora_han;
            out->han += dora_han;
        }
    }

    sc__calc_basic_pts(out);
}

/* ══════════════════════════════════════════════════════════════
   基本點計算
   閒家基本點 = fu × 2^(han+2)，超過 7700 時依等級取固定值（上限 8000）
   莊家基本點 = 閒家基本點 × 1.5（小數無條件進位至整數）
   等級對照：
     滿貫   (Mangan)    : 閒家 8000   / 莊家 12000
     跳滿   (Haneman)   : 閒家 12000  / 莊家 18000
     倍滿   (Baiman)    : 閒家 16000  / 莊家 24000
     三倍滿 (Sanbaiman) : 閒家 24000  / 莊家 36000
     役滿   (Yakuman)   : 閒家 32000  / 莊家 48000
══════════════════════════════════════════════════════════════ */
static void sc__calc_basic_pts(ScoreResult* s) {
    s->limit_name[0] = '\0';

    if (s->is_yakuman) {
        s->basic_pts        = 32000;
        s->dealer_basic_pts = 48000;
        strcpy(s->limit_name, "Yakuman");
        return;
    }

    int han = s->han;
    int fu  = s->fu;

    /* 先依番數判斷是否直接進入固定等級 */
    if (han >= 13) {
        s->basic_pts = 32000; strcpy(s->limit_name, "Kazoe Yakuman");
    } else if (han >= 11) {
        s->basic_pts = 24000; strcpy(s->limit_name, "Sanbaiman");
    } else if (han >= 8) {
        s->basic_pts = 16000; strcpy(s->limit_name, "Baiman");
    } else if (han >= 6) {
        s->basic_pts = 12000; strcpy(s->limit_name, "Haneman");
    } else {
        /* 一般計算：fu × 2^(han+2) */
        int pts = fu;
        for (int i = 0; i < han + 2; i++) pts *= 2;
      
        /* 滿貫判定：超過 7700 或符合特定組合 */
        int is_mangan = (pts > 7900)
            || (han == 5)
            || (han == 4 && fu >= 30)
            || (han == 3 && fu >= 70);

        if (is_mangan) {
            s->basic_pts = 8000;
            strcpy(s->limit_name, "Mangan");
        } else {
            s->basic_pts = pts;
        }
    }

    /* 莊家基本點 = 閒家 × 1.5，無條件進位 */
    s->dealer_basic_pts = (s->basic_pts * 3 + 1) / 2;
}

/* ══════════════════════════════════════════════════════════════
   輔助：印出計算結果
══════════════════════════════════════════════════════════════ */
static void score_print(const ScoreResult* s) {
    if (s->is_yakuman) {
        printf("   [YAKUMAN] %s\n", s->yakuman_name);
        printf("   Basic pts: %d (dealer: %d)\n",
               s->basic_pts, s->dealer_basic_pts);
        return;
    }
    printf("   Fu: %d  Han: %d", s->fu, s->han);
    if (s->limit_name[0]) printf("  (%s)", s->limit_name);
    printf("\n");
    printf("   Basic pts: %d (dealer: %d)\n",
           (s->basic_pts) , (s->dealer_basic_pts) );
    for (int i = 0; i < s->n_yaku; i++)
        printf("       + %s (%d han)\n", s->yaku[i].name, s->yaku[i].han);
}

#endif /* SCORE_CALC_H */
