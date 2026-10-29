/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular One SMK-37: the screen (m1_ui.c, m1_draw.c) on the host, with a script of hands on the controls; every
 * 40 ms of simulated time the LCD as the firmware left it, to OUTDIR/fNNNNN.ppm (tests/m1_screen.py: a video).
 *   cc -O1 -w -DMODULAR_ONE=1 -DFELUCCA_SLICE=0 -Ibuild/gen -Ifirmware/src -Itests -o build/host/m1_screen tests/m1_screen.c -lm
 *   build/host/m1_screen OUTDIR
 * Checks: every clip plays to its end without leaving the grid, the band shows each control touched, the cat
 * falls asleep at night and wakes when a key is played. Exit 1 on a failure. */
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
static uint32_t host_pressed, host_released, host_notes;
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

#include "../firmware/src/m1_ui.c"
#include "../firmware/src/m1_draw.c"

/* ------------------------------------------------------------ the film */
static const char *dir;
static uint32_t nframe, fails;
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
        m1_input();
        m1_draw();
        if (cat.box[2] && (cat.box[0] + cat.box[2] > CAT_G || cat.box[1] + cat.box[3] > CAT_SCENE)) {
            printf("m1_screen: FAIL frame box off the grid (clip %u)\n", cat.clip);
            fails++;
        }
        fm1_ms += 15u;
        if (fm1_ms % 40u < 15u)
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

int main(int argc, char **argv)
{
    uint32_t k;
    dir = argc > 1 ? argv[1] : "build/m1_screen";
    host_tracks_init();
    fm1_ms = 1000;
    m1_init();
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
    run(14000);
    if (cat.clip != CAT_SLEEP) { printf("m1_screen: FAIL not asleep at night (clip %u)\n", cat.clip); fails++; }
    for (k = 0; k < 30u; k++) { fm1_in.fader[1] = (uint8_t)(k * 110 / 29); run(60); }     /* winter */
    run(4000);
    key(7, 1500);                                                           /* a key: he wakes */
    if (cat.clip == CAT_SLEEP) { printf("m1_screen: FAIL still asleep after a key\n"); fails++; }
    run(2500);
    for (k = 0; k < 30u; k++) { fm1_in.fader[0] = (uint8_t)(k * 127 / 29); fm1_in.fader[1] = (uint8_t)(110 - k * 110 / 29); run(60); }
    run(2000);
    printf("m1_screen: %u pictures, %.1f s, %.0f px to the LCD a second\n", nframe, nframe * 0.04,
           (double)host_blit_px / (fm1_ms / 1000.0));
    printf("m1_screen: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}
