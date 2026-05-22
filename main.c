#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TILE_TYPES 34
#define HAND_SIZE  14

#include "Meld.h"
#include "Ron.h"
#include "Tenpai.h"
#include "Wall.h"

/* ══════════════════════════════════════════
   Utility functions
══════════════════════════════════════════ */
static const char* tile_name(int idx) {
    static char bufs[8][12];
    static int which = 0;
    char* buf = bufs[which++ % 8];

    if (idx < 0) return "Unknown";
    if (idx < 9) sprintf(buf, "%d-Man", idx + 1);
    else if (idx < 18) sprintf(buf, "%d-Pin", idx - 9 + 1);
    else if (idx < 27) sprintf(buf, "%d-Sou", idx - 18 + 1);
    else {
        const char* honors[] = { "East","South","West","North","Haku","Hatsu","Chun" };
        sprintf(buf, "%s", honors[idx - 27]);
    }
    return buf;
}

static int parse_tile(const char* token) {
    if (!token || !token[0] || !token[1]) return -1;
    int num = token[0] - '0';
    char suit = (char)tolower((unsigned char)token[1]);

    if (suit == 'm' && num >= 1 && num <= 9) return num - 1;
    if (suit == 'p' && num >= 1 && num <= 9) return  9 + (num - 1);
    if (suit == 's' && num >= 1 && num <= 9) return 18 + (num - 1);
    if (suit == 'z' && num >= 1 && num <= 7) return 27 + (num - 1);
    return -1;
}

/* 解析副露格式 (例如 p5p, c234m, k1z, a1z) */
static int parse_meld(const char* token, PlayerHand* ph) {
    if (ph->num_melds >= 4) return 0;

    char type = (char)tolower((unsigned char)token[0]);
    if (type != 'p' && type != 'c' && type != 'k' && type != 'a') return 0;

    Meld m;
    m.tiles[0] = m.tiles[1] = m.tiles[2] = m.tiles[3] = -1;

    if (type == 'p') {
        int idx = parse_tile(token + 1);
        if (idx < 0) return 0;
        m.type = MELD_PON;
        m.tiles[0] = m.tiles[1] = m.tiles[2] = idx;
    }
    else if (type == 'k' || type == 'a') {
        int idx = parse_tile(token + 1);
        if (idx < 0) return 0;
        m.type = (type == 'a') ? MELD_KAN_CLOSED : MELD_KAN_OPEN;
        m.tiles[0] = m.tiles[1] = m.tiles[2] = m.tiles[3] = idx;
    }
    else if (type == 'c') {
        if (strlen(token) < 5) return 0;
        char suit = (char)tolower((unsigned char)token[4]);
        char t1[3] = { token[1], suit, 0 }, t2[3] = { token[2], suit, 0 }, t3[3] = { token[3], suit, 0 };
        int i1 = parse_tile(t1), i2 = parse_tile(t2), i3 = parse_tile(t3);
        if (i1 < 0 || i2 < 0 || i3 < 0) return 0;
        m.type = MELD_CHI;
        m.tiles[0] = i1; m.tiles[1] = i2; m.tiles[2] = i3;
    }
    ph->melds[ph->num_melds++] = m;
    return 1;
}

/* ══════════════════════════════════════════
   Hand display
══════════════════════════════════════════ */
static void print_hand(const PlayerHand* ph) {
    printf("Closed Hand: ");
    for (int i = 0; i < TILE_TYPES; i++)
        for (int j = 0; j < ph->closed_hand[i]; j++)
            printf("%s ", tile_name(i));
    printf("\n");

    if (ph->num_melds > 0) {
        printf("Melds (Open): ");
        for (int i = 0; i < ph->num_melds; i++) {
            printf("[");
            for (int j = 0; j < 4; j++) {
                if (ph->melds[i].tiles[j] != -1) {
                    printf("%s", tile_name(ph->melds[i].tiles[j]));
                    if (j < 3 && ph->melds[i].tiles[j + 1] != -1) printf(" ");
                }
            }
            printf("] ");
            if (ph->melds[i].type == MELD_KAN_CLOSED) printf("(Ankan) ");
        }
        printf("\n");
    }
}

/* ══════════════════════════════════════════
   Seen-tile input helpers
══════════════════════════════════════════ */
static int read_seen_line(const char* prompt, int seen[TILE_TYPES]) {
    printf("%s", prompt);
    fflush(stdout);

    char line[256];
    if (!fgets(line, sizeof(line), stdin)) return 0;

    char* p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\n' || *p == '\0' || *p == '-' ||
        (p[0] == 'n' && p[1] == 'o' && p[2] == 'n' && p[3] == 'e')) return 0;

    int count = 0, pos = 0;
    char token[8];
    while (*p) {
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            if (pos > 0) {
                token[pos] = '\0';
                int idx = parse_tile(token);
                if (idx >= 0) { seen[idx]++; count++; }
                pos = 0;
            }
        }
        else {
            if (pos < 7) token[pos++] = *p;
        }
        p++;
    }
    if (pos > 0) {
        token[pos] = '\0';
        int idx = parse_tile(token);
        if (idx >= 0) { seen[idx]++; count++; }
    }
    return count;
}

static void print_seen(const int seen[TILE_TYPES]) {
    int any = 0;
    for (int i = 0; i < TILE_TYPES; i++) {
        for (int j = 0; j < seen[i]; j++) {
            printf("%s ", tile_name(i));
            any = 1;
        }
    }
    if (!any) printf("(none)");
}

/* ══════════════════════════════════════════
   Main
══════════════════════════════════════════ */
int main(void) {
    char token[16];
    PlayerHand ph;
    memset(&ph, 0, sizeof(PlayerHand));

    printf("===========================================\n");
    printf("   Japanese Mahjong Hand Analyzer\n");
    printf("===========================================\n");
    printf("Format: Normal tiles (e.g. 3m, 7p, 1z)\n");
    printf("Melds: c234m (Chi), p5p (Pon), k1z (Minkan), a1z (Ankan)\n");
    printf("Example hand: 1m 2m 3m p4m 5p 6p 7p 1s 1s 1s 7z 7z\n");
    printf("-------------------------------------------\n");
    printf("Enter your hand (Space-separated):\n");

    /* 讀取整行確保不干擾後續輸入 */
    char line[256];
    if (fgets(line, sizeof(line), stdin)) {
        char* p = line;
        int pos = 0;
        while (*p) {
            if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
                if (pos > 0) {
                    token[pos] = '\0';
                    if (token[0] == 'p' || token[0] == 'c' || token[0] == 'k' || token[0] == 'a') {
                        parse_meld(token, &ph);
                    }
                    else {
                        int idx = parse_tile(token);
                        if (idx >= 0) { ph.closed_hand[idx]++; ph.closed_count++; }
                    }
                    pos = 0;
                }
            }
            else {
                if (pos < 15) token[pos++] = *p;
            }
            p++;
        }
    }

    printf("\n");
    print_hand(&ph);
    printf("-------------------------------------------\n");

    /* ── Win check ── */
    RonResult result;
    int can_win = ron_check(&ph, &result);

    if (can_win) {
        printf("** Winning hand! **\n");
        if (result.normal)     printf("   - Standard win (4 melds + 1 pair)\n");
        if (result.tanyao)     printf("       + Tanyao (All Simples)\n");
        if (result.chiitoitsu) printf("   - Chiitoitsu (Seven Pairs)\n");
        if (result.kokushi)    printf("   - Kokushi Musou (Thirteen Orphans)\n");
        if (result.ryuuiisou)  printf("   - Ryuu Iisou (All Green)\n");
        if (result.tsuuiisou)  printf("   - Tsuuiisou (All Honors)\n");
        if (result.daisangen)  printf("   - Daisangen (Big Three Dragons)\n");
        printf("===========================================\n");
        return 0;
    }

    printf("Not a winning hand.\n\n");
    printf("===========================================\n");
    printf("   Live Wall Setup\n");
    printf("===========================================\n");

    int seen_dora[TILE_TYPES] = { 0 }, seen_left[TILE_TYPES] = { 0 };
    int seen_opp[TILE_TYPES] = { 0 }, seen_right[TILE_TYPES] = { 0 };
    int own_discards[TILE_TYPES] = { 0 };

    read_seen_line("Dora indicator tile(s)      : ", seen_dora);
    read_seen_line("Kamicha (left)  discards    : ", seen_left);
    read_seen_line("Toimen  (across) discards   : ", seen_opp);
    read_seen_line("Shimocha (right) discards   : ", seen_right);
    printf("-------------------------------------------\n");
    read_seen_line("Your own discards           : ", own_discards);

    int seen_all[TILE_TYPES] = { 0 };
    for (int i = 0; i < TILE_TYPES; i++) {
        seen_all[i] = seen_dora[i] + seen_left[i] + seen_opp[i] + seen_right[i] + own_discards[i];
    }

    /* 將玩家包含副露在內的所有牌傳給 Wall 拔除 */
    int player_all_tiles[TILE_TYPES] = { 0 };
    playerhand_get_all_tiles(&ph, player_all_tiles);

    Wall wall;
    wall_init(&wall);
    wall_remove_hand(&wall, player_all_tiles);
    wall_remove_seen(&wall, seen_all);

    printf("\n");
    TenpaiResult tr;
    tenpai_calc(&ph, wall_counts(&wall), &tr);

    if (tr.shanten == 0) printf("Tenpai (1 tile away from winning)\n");
    else printf("%d tile(s) away from tenpai.\n", tr.shanten);

    if (tr.n_options == 0 && tr.shanten == 0) {
        printf("  No winning tile available in the remaining wall.\n");
    }
    else if (tr.shanten > 0) {
        printf("\n  Best discards to advance toward tenpai:\n");
        for (int i = 0; i < tr.n_options; i++) printf("  Discard %s\n", tile_name(tr.options[i].discard));
    }
    else {
        printf("\n");
        for (int i = 0; i < tr.n_options; i++) {
            TenpaiOption* opt = &tr.options[i];
            FuritenResult fur;
            furiten_check(opt, own_discards, &fur);

            PlayerHand h13 = ph;
            h13.closed_hand[opt->discard]--;
            h13.closed_count--;

            printf("  Discard %-8s -> ", tile_name(opt->discard));

            if (fur.ron_furiten) {
                printf("FURITEN (Ron blocked) | Tsumo only\n");
                printf("    Furiten cause  : ");
                for (int k = 0; k < fur.n_furiten; k++) {
                    printf("%s", tile_name(fur.furiten_tiles[k]));
                    if (k < fur.n_furiten - 1) printf(", ");
                }
                printf("\n    Waiting on     : ");
            }
            else {
                printf("waiting on: ");
            }

            for (int j = 0; j < opt->n_waits; j++) {
                int tile = opt->waits[j].tile;
                int rem = opt->waits[j].remaining;

                PlayerHand h14 = h13;
                h14.closed_hand[tile]++;
                h14.closed_count++;

                RonResult wr;
                ron_check(&h14, &wr);

                char note[64] = "";
                if (wr.tanyao) strncat(note, ", Tanyao", sizeof(note) - strlen(note) - 1);

                printf("%s (%d left%s)", tile_name(tile), rem, note);
                if (j < opt->n_waits - 1) printf(", ");
            }
            printf("  [total: %d]\n", opt->total_remaining);
        }
    }
    printf("===========================================\n");
    return 0;
}