/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular One SMK-37: the screen (m1_ui.c, m1_draw.c) on the host, with a script of hands on the controls; every
 * 40 ms of simulated time the LCD as the firmware left it, to OUTDIR/fNNNNN.ppm (tests/m1_screen.py: a video).
 *   cc -O1 -w -DMODULAR_ONE=1 -DFELUCCA_SLICE=0 -Ibuild/gen -Ifirmware/src -Itests -o build/host/m1_screen tests/m1_screen.c -lm
 *   build/host/m1_screen OUTDIR
 * Checks: every clip plays to its end without leaving the grid, the band shows each control touched, the cat
 * falls asleep at night, sleeps through the music and wakes with the day. Exit 1 on a failure. */
#define main hostsim_main
#include "hostsim.c"
#undef main

/* ------------------------------------------------------------ LCD */
static uint16_t host_screen[240 * 240];
static uint64_t host_blit_px;
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            host_screen[(y + j) * 240u + x + i] = (uint16_t)((c >> 8) | (c << 8));
    host_blit_px += w * h;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            host_screen[(y + j) * 240u + x + i] = p[j * w + i];
    host_blit_px += w * h;
}
#include "../firmware/src/gfx.c"

/* ------------------------------------------------------------ the SMK-37's controls (smk_input.h), by hand */
#define FM1_NBTN 18u
#define SMK_KEY_F3 5u
enum { FM1_BTN_OCT_DOWN = 4, FM1_BTN_OCT_UP = 5 };
enum { SMK_BTN_KNOB_BANK = 0, SMK_BTN_FADER_BANK = 1 };
#define fm1_in m1h_in                            /* (hostsim.c has its own) */
static struct { uint8_t fader[4]; uint8_t key_vel[40]; uint32_t frames, buttons; } fm1_in = {{127, 0, 40, 30}, {0}, 1000, 0};
static uint32_t host_pressed, host_released, host_notes, host_btn;
static int32_t host_enc[8], host_pad = -1;
static uint32_t fm1_input_edges(uint32_t *rel) { uint32_t p = host_pressed; *rel = host_released; host_pressed = host_released = 0; return p; }
static uint32_t fm1_input_note_edges(void) { uint32_t n = host_notes; host_notes = 0; return n; }
static int32_t fm1_enc_take(uint32_t e) { int32_t s = host_enc[e & 7u]; host_enc[e & 7u] = 0; return s; }
static volatile uint8_t smk_pad_free;
static int32_t smk_padev_take(void) { int32_t p = host_pad; host_pad = -1; return p < 0 ? -1 : p << 8 | 100; }
static int32_t master_knob = 8000;
static void master_poll(void) {}
static void midi_take(void) {}
static void padled_set(uint32_t p, uint8_t r, uint8_t g, uint8_t b) { (void)p; (void)r; (void)g; (void)b; }
static void fm1_led_key(uint32_t k, int on) { (void)k; (void)on; }
static void apply_preset_to(track_t *t, uint32_t pi) { host_preset(t, ENGI_VILLE, pi); }
static volatile uint8_t smk_modw;
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static volatile uint8_t smk_vel_curve;
/* MENU's rows GLOBE uses (menu_items.c, ui.c), as plain values */
enum { MI_TUNE, MI_TCH1 };
static uint32_t host_menu[2] = {50, 0}, host_scr, host_saves, host_lcd_on = 1;
static uint32_t menu_n(uint32_t r) { return r == MI_TUNE ? 101u : 18u; }
static uint32_t menu_get(uint32_t r) { return host_menu[r & 1u]; }
static void menu_put(uint32_t r, uint32_t v) { host_menu[r & 1u] = v; }
static const char *menu_vname(uint32_t r, uint32_t v)
{
    static char b[8];
    if (r == MI_TCH1 && v >= 16u) return v == 16u ? "OMNI" : "OFF";
    b[0] = '+';
    fmt_int(b + (r == MI_TUNE && (int32_t)v > 50), r == MI_TUNE ? (int32_t)v - 50 : (int32_t)v + 1);
    return b;
}
static const uint8_t SCR_MIN[5] = {0, 5, 15, 30, 60};
static uint32_t scr_get(void) { return host_scr; }
static void scr_put(uint32_t v) { host_scr = v; }
static void settings_save(void) { host_saves++; }
static void lcd_power(uint32_t s) { host_lcd_on = s != 0u; }
#ifndef HOST_HAS_FAVORITES
static struct { uint8_t factory[16][32]; uint32_t user, filter; } favorites;
#endif

#include "../firmware/src/m1_ui.c"

/* the sketchbook (m1_book.c) on a fake flash: AND-writes, 4 KiB erases */
static uint8_t host_fl[SMP_USER_SIZE];
static int flash_ok = 1;
static uint32_t host_erases;
#define fm1_xip_ptr(off) ((const uint8_t *)host_fl + ((off) - SMP_USER_BASE))
static void fl_inval(uint32_t off, uint32_t n) { (void)off; (void)n; }
static void audio_silence(void) {}
static void fm1_wdt_feed(void) { if (demo_on) demo_block(CTL); }        /* (the audio goes on while it waits) */
static int fl_erase4k(uint32_t off, uint32_t *took) { *took = 0; host_erases++; memset(host_fl + (off - SMP_USER_BASE), 0xFF, 0x1000); return 0; }
static int fl_write(uint32_t off, const uint8_t *p, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        host_fl[off - SMP_USER_BASE + i] &= p[i];
    return 0;
}
static uint32_t st_crc32(const void *p, uint32_t n)
{
    const uint8_t *b = p;
    uint32_t c = 0xFFFFFFFFu, k;
    while (n--) {
        c ^= *b++;
        for (k = 0; k < 8u; k++)
            c = c >> 1 ^ (0xEDB88320u & -(c & 1u));
    }
    return ~c;
}
#include "../firmware/src/m1_book.c"
#include "../firmware/src/m1_draw.c"

/* ------------------------------------------------------------ the film */
static const char *dir;
static uint32_t nframe, fails, no_shots;
static void shot(void)
{
    char p[512];
    FILE *f;
    uint32_t i;
    snprintf(p, sizeof p, "%s/f%05u.ppm", dir, nframe++);
    if (!(f = fopen(p, "wb")))
        return;
    fprintf(f, "P6 240 240 255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t v = (uint16_t)((host_screen[i] >> 8) | (host_screen[i] << 8));
        uint8_t rgb[3] = {(uint8_t)((v >> 11) << 3), (uint8_t)(((v >> 5) & 63u) << 2), (uint8_t)((v & 31u) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

/* run for ms of simulated time: the UI loop every 15 ms, a picture every 40 ms */
static void run(uint32_t ms)
{
    uint32_t end = fm1_ms + ms;
    while ((int32_t)(fm1_ms - end) < 0) {
        fm1_in.buttons = host_btn;
        m1_input();
        m1_service();
        m1_draw();
        {   /* the audio's events, block by block (the tape's player, the sketch clock) */
            uint32_t b;
            for (b = 0; b < 15u * 44100u / 1000u / CTL; b++)
                events_block(CTL);
        }
        if (cat.box[2] && (cat.box[0] + cat.box[2] > CAT_G || cat.box[1] + cat.box[3] > CAT_SCENE)) {
            printf("m1_screen: FAIL frame box off the grid (clip %u)\n", cat.clip);
            fails++;
        }
        fm1_ms += 15u;
        if (fm1_ms % 40u < 15u && !no_shots)
            shot();
    }
}

static void key(uint32_t i, uint32_t ms)
{
    uint32_t t;
    for (t = 0; t < ms; t += 300u) {
        host_notes |= 1u << i;
        fm1_in.key_vel[SMK_KEY_F3 + i] = 90;
        run(300);
    }
}

static void life(uint32_t clip)
{
    uint32_t guard = 0;
    cat_play(clip);
    cat.quiet_since = fm1_ms;
    run(200);
    while (cat.clip == clip && guard++ < 2000u)
        run(15);
    if (guard >= 2000u) {
        printf("m1_screen: FAIL clip %u never ends\n", clip);
        fails++;
    }
}

#ifndef M1_SCREEN_NO_MAIN                               /* (tests/m1_shots.c: the same harness, its own scenes) */
int main(int argc, char **argv)
{
    uint32_t k;
    int16_t cut;
    dir = argc > 1 ? argv[1] : "build/m1_screen";
    host_tracks_init();
    fm1_ms = 1000;
    memset(host_fl, 0xFF, sizeof host_fl);
    m1_init();
    m1_boot();
    run(3000);                                                              /* IDLE */
    host_enc[1] = -3; run(200); host_enc[1] = -5; run(200); host_enc[1] = -4; run(1400);   /* CUTOFF down */
    if (cat.clip != CAT_LOOK_R) { printf("m1_screen: FAIL no look at the knob (clip %u)\n", cat.clip); fails++; }
    host_pad = 9; run(1500);                                                /* BRASS */
    key(12, 2400);                                                          /* playing */
    if (cat.clip != CAT_PLAY) { printf("m1_screen: FAIL not listening (clip %u)\n", cat.clip); fails++; }
    run(1500);
    host_pressed = 1u << SMK_BTN_KNOB_BANK; run(1200);                     /* (BRASS: no wave) */
    host_pad = 0; run(600);                                                 /* MODULAR */
    host_pressed = 1u << SMK_BTN_KNOB_BANK; run(1500);                     /* WAVE 1 */
    for (k = CAT_PETRIR; k < CAT_NCLIPS; k++)                               /* his whole life, in order */
        if (k != CAT_ENDORT)
            life(k);
    for (k = 0; k < 40u; k++) { fm1_in.fader[0] = (uint8_t)(127 - k * 127 / 39); run(60); }   /* nightfall */
    cat.next_life = fm1_ms;
    run(30000);
    if (cat.clip != CAT_SLEEP) { printf("m1_screen: FAIL not asleep at night (clip %u)\n", cat.clip); fails++; }
    for (k = 0; k < 30u; k++) { fm1_in.fader[1] = (uint8_t)(k * 110 / 29); run(60); }     /* winter */
    run(4000);
    key(7, 1500);                                                           /* the music: he sleeps on */
    if (cat.clip != CAT_SLEEP) { printf("m1_screen: FAIL woke up at night (clip %u)\n", cat.clip); fails++; }
    run(2500);
    for (k = 0; k < 30u; k++) { fm1_in.fader[0] = (uint8_t)(k * 127 / 29); fm1_in.fader[1] = (uint8_t)(110 - k * 110 / 29); run(60); }
    run(2000);                                                              /* the day: he wakes */
    if (cat.clip == CAT_SLEEP || cat.clip == CAT_ENDORT) { printf("m1_screen: FAIL still asleep in the day\n"); fails++; }
    /* ARP: a tap on / off; held, K2 its mode, K3 its speed, K4 the tempo (the press itself turns nothing); the arp
     * stays when the sound changes. CHORD: the A 440 on / off. REC: nothing (no tape) */
    {
        uint32_t bpm = (uint32_t)song.g[G_BPM];
        host_btn = 1u << 6; host_pressed = host_btn; run(200); host_released = host_btn; host_btn = 0; run(300);
        if (M1T->p[P_AMODE] != 1) { printf("m1_screen: FAIL ARP tap: mode %d\n", M1T->p[P_AMODE]); fails++; }
        host_btn = 1u << 6; host_pressed = host_btn; run(200);
        host_enc[1] = 1; run(300); host_enc[1] = 1; run(300);               /* MONTE -> DESCEND -> MONTE-DESC */
        host_enc[2] = -1; run(300);                                          /* 1/16 -> 1/8 T */
        host_enc[3] = 1; run(300);                                           /* tempo + 1 */
        host_released = host_btn; host_btn = 0; run(300);
        if (M1T->p[P_AMODE] != 3 || M1T->p[P_ARATE] != 4 || (uint32_t)song.g[G_BPM] != bpm + 1u) {
            printf("m1_screen: FAIL ARP held: mode %d rate %d bpm %d\n", M1T->p[P_AMODE], M1T->p[P_ARATE], song.g[G_BPM]); fails++;
        }
        host_pad = 9; run(500);                                              /* BRASS: the arp stays */
        if (M1T->p[P_AMODE] != 3 || M1T->p[P_CHRD] || M1T->p[P_QUANT]) { printf("m1_screen: FAIL ARP lost with the sound\n"); fails++; }
        host_pressed = 1u << 6; run(200); host_released = 1u << 6; run(300);
        if (M1T->p[P_AMODE]) { printf("m1_screen: FAIL ARP tap off\n"); fails++; }
        host_pressed = 1u << 9; run(300);
        if (!m1_la) { printf("m1_screen: FAIL CHORD: no A 440\n"); fails++; }
        host_pressed = 1u << 9; run(300);
        if (m1_la) { printf("m1_screen: FAIL CHORD again: the A 440 still on\n"); fails++; }
        host_btn = 1u << M1_B_REC; host_pressed = host_btn; run(900); host_released = host_btn; host_btn = 0; run(300);
        if (pl_count) { printf("m1_screen: FAIL REC kept a sketch\n"); fails++; }
        host_pad = 0; run(2600);
    }
    /* GLOBE: K2 .. K7, one setting each, saved; the cat back after */
    cut = M1T->p[P_E3];
    host_pressed = 1u << 10; run(800);
    host_enc[1] = 2; run(500);                                               /* CANAL MIDI 1 -> 3 (a fast turn: +1) */
    host_enc[2] = 1; run(500);                                               /* VELOCITE: DOUCE */
    host_enc[3] = -1; run(300); host_enc[3] = -3; run(500);                  /* ACCORDAGE -1, then -5 */
    host_enc[4] = 1; run(500);                                               /* LUMIERE PADS: VIVE */
    host_enc[5] = 1; run(500);                                               /* VEILLE ECRAN: 5 MIN */
    host_enc[6] = 1; run(1500);                                              /* LE CHAT: CALME */
    if (host_menu[MI_TCH1] != 1u || smk_vel_curve != 1u || host_menu[MI_TUNE] != 44u || M1G[M1G_PADS] != 3u ||
        host_scr != 1u || M1G[M1G_CAT] != 1u || M1G[M1G_MAGIC] != 'M' || !host_saves) {
        printf("m1_screen: FAIL GLOBE: ch %u vel %u tune %u pads %u scr %u cat %u saves %u\n", host_menu[MI_TCH1],
               smk_vel_curve, host_menu[MI_TUNE], M1G[M1G_PADS], host_scr, M1G[M1G_CAT], host_saves);
        fails++;
    }
    if (M1T->p[P_E3] != cut) { printf("m1_screen: FAIL GLOBE's knobs changed the sound\n"); fails++; }
    host_pressed = 1u << 10; run(2000);
    /* VEILLE ECRAN 5 MIN: dark after five minutes alone, back with a key */
    no_shots = 1;
    run(5u * 60000u + 1000u);
    no_shots = 0;
    if (host_lcd_on) { printf("m1_screen: FAIL the screen did not sleep\n"); fails++; }
    key(5, 600); run(600);
    if (!host_lcd_on) { printf("m1_screen: FAIL a key did not wake the screen\n"); fails++; }
    printf("m1_screen: %u pictures, %.1f s, %.0f px to the LCD a second\n", nframe, nframe * 0.04,
           (double)host_blit_px / (fm1_ms / 1000.0));
    printf("m1_screen: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}
#endif
