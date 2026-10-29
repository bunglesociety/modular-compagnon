/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular One on the SMK-37: the screen. A little black cat lives in it.
 *   the scene    240 x 204: an 80 x 68 grid of 3 x 3 px cells (tools/gen_cat.py draws him there). The wall and the
 *                floor follow fader 1 (day up, night down: stars come out), fader 2 brings the snow (the FM winter).
 *   the cat      clips of frames (cat_sprites.h): he sits and breathes, listens when you play (eyes ^^, notes), looks
 *                at a control you touch, and when you leave him alone he lives his life (griffes on the edge of the
 *                screen, toilette, roulade, la folie, chasse, mlem, ...). At night he falls asleep; playing wakes him.
 *   the band     240 x 36 at the bottom: the sound (in its pad's colour) and the control touched, with its value.
 * Only what changed goes to the LCD: the rows the cat moved in, the band when its text changes; the whole scene
 * when the sky changes or while snow falls. */
#include "cat_sprites.h"

#define M1_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define CAT_SKY_ROWS 56u                         /* the wall above, the floor below */
#define BAND_Y 204u

static uint8_t cat_buf[CAT_G * CAT_SCENE];
static struct {
    uint8_t clip, step, behaving;
    uint32_t t_next, quiet_since, next_life;
    uint8_t box[4];                               /* the frame on screen: x y w h */
    int16_t night, winter;                        /* the sky drawn (-1: never) */
    uint32_t snow_t, rnd;
    uint8_t full;                                 /* the whole scene to draw */
    uint32_t band_sig;
} cat = {.night = -1, .winter = -1, .rnd = 0x2545F491u, .full = 1, .band_sig = 0xFFFFFFFFu};

static struct { uint8_t x, y, dx; } snow[32];

static uint32_t cat_rand(uint32_t n)
{
    uint32_t x = cat.rnd;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    cat.rnd = x;
    return n ? x % n : x;
}

/* ------------------------------------------------------------ frames: a key, or a patch on a key */
static void cat_decode(const cat_frame_t *f)
{
    const uint8_t *d = CAT_DATA + f->off;
    uint32_t n = f->len, p = 0;
    while (n--) {
        uint32_t b = *d++;
        if (b & 0x80u) {
            p += (b & 0x7Fu) + 1u;
        } else {
            uint32_t k = (b >> 4) + 1u, v = b & 15u;
            while (k-- && p < sizeof cat_buf)
                cat_buf[p++] = (uint8_t)v;
        }
    }
}

static void cat_frame(uint32_t fi)
{
    const cat_frame_t *f = &CAT_FRAMES[fi];
    uint32_t i;
    for (i = 0; i < sizeof cat_buf; i++)
        cat_buf[i] = 0;
    if (f->base != 0xFFFFu)
        cat_decode(&CAT_FRAMES[f->base]);
    cat_decode(f);
}

/* ------------------------------------------------------------ the scene */
static const uint8_t STARS[18][2] = {
    {5, 6}, {14, 15}, {22, 4}, {31, 10}, {44, 3}, {52, 12}, {60, 5}, {70, 9}, {76, 20}, {9, 28},
    {66, 30}, {38, 22}, {3, 44}, {74, 40}, {27, 34}, {57, 24}, {48, 40}, {18, 46},
};

/* grid rows gy0 .. gy1-1 to the LCD (3 screen rows each), through the canvas, 41 grid rows at a time */
static void scene_rows(uint32_t gy0, uint32_t gy1)
{
    uint16_t pal[16], sky, floor_, shadow, star = 0, flake = swap16(M1_RGB(255, 255, 255));
    uint32_t i, sx0 = 0, sx1 = 0, nflakes = cat.winter > 16 ? (uint32_t)cat.winter / 4u : 0u;
    int32_t n = cat.night < 0 ? 0 : cat.night, w = cat.winter < 0 ? 0 : cat.winter;
    {   /* the day, a little whiter with the snow, then the night over it */
        uint16_t s = ux_mix(M1_RGB(0xf7, 0xec, 0xda), M1_RGB(0xe8, 0xed, 0xf3), w * 100 / 127);
        uint16_t f = ux_mix(M1_RGB(0xef, 0xe1, 0xca), M1_RGB(0xf7, 0xf9, 0xfc), w * 100 / 127);
        uint16_t h = ux_mix(M1_RGB(0xe4, 0xcf, 0xb0), M1_RGB(0xdf, 0xe6, 0xef), w * 100 / 127);
        int32_t pn = n * 100 / 127;
        sky = ux_mix(s, M1_RGB(0x22, 0x24, 0x44), pn);
        floor_ = ux_mix(f, M1_RGB(0x1c, 0x1d, 0x3a), pn);
        shadow = ux_mix(h, M1_RGB(0x14, 0x15, 0x2d), pn);
        if (n > 64)
            star = swap16(ux_mix(sky, M1_RGB(0xe8, 0xe4, 0xff), (n - 64) * 100 / 63));
        sky = swap16(sky);
        floor_ = swap16(floor_);
        shadow = swap16(shadow);
    }
    if (nflakes > 32u)
        nflakes = 32u;
    for (i = 0; i < 16u; i++)
        pal[i] = swap16(CAT_PAL[i]);
    if (cat.box[2] && cat.box[1] + cat.box[3] >= 58u) {                     /* his shadow, under him on the floor */
        sx0 = cat.box[0] + 2u;
        sx1 = cat.box[0] + cat.box[2] - 2u;
    }
    while (gy0 < gy1) {
        uint32_t nr = gy1 - gy0, r;
        if (nr > 41u)
            nr = 41u;                                                        /* 123 px: the canvas holds 124 rows */
        lcd_sync();                                                          /* (the last blit may still read it) */
        for (r = 0; r < nr; r++) {
            uint32_t gy = gy0 + r, gx, k;
            uint16_t *row = cv_px + r * 3u * 240u, bg = gy < CAT_SKY_ROWS ? sky : floor_;
            const uint8_t *c = cat_buf + gy * CAT_G;
            for (gx = 0; gx < CAT_G; gx++) {
                uint16_t v = bg;
                if ((gy == 62u || gy == 63u) && gx >= sx0 && gx < sx1)
                    v = shadow;
                if (c[gx])
                    v = pal[c[gx]];
                row[gx * 3u] = row[gx * 3u + 1u] = row[gx * 3u + 2u] = v;
            }
            for (k = 0; k < nflakes; k++)
                if (snow[k].y == gy && snow[k].x < CAT_G && !c[snow[k].x]) {
                    uint32_t x = snow[k].x * 3u;
                    row[x] = row[x + 1u] = row[x + 2u] = flake;
                }
            for (k = 0; k < 240u; k++)
                row[240u + k] = row[480u + k] = row[k];
            if (star && gy < 50u)                                            /* a star: one pixel in the middle */
                for (k = 0; k < 18u; k++)
                    if (STARS[k][1] == gy && !c[STARS[k][0]])
                        row[240u + STARS[k][0] * 3u + 1u] = star;
        }
        lcd_blit(0, gy0 * 3u, 240u, nr * 3u, cv_px);
        gy0 += nr;
    }
}

/* ------------------------------------------------------------ his life */
static void cat_play(uint32_t clip)
{
    cat.clip = (uint8_t)clip;
    cat.step = 0;
    cat.t_next = 0;                                                          /* its first frame: now */
    cat.behaving = clip >= CAT_PETRIR;
}

static const uint8_t CAT_DAY_LIFE[] = {CAT_PETRIR, CAT_MLEM, CAT_VIDE, CAT_QUEUE, CAT_CELLO, CAT_CHASSE, CAT_TAPOTE,
                                       CAT_TOILETTE, CAT_ROULADE, CAT_GRIFFES, CAT_FOLIE, CAT_TERRITOIRE, CAT_BAILLEMENT};
static const uint8_t CAT_NIGHT_LIFE[] = {CAT_ENDORT, CAT_PETRIR, CAT_BAILLEMENT, CAT_ENDORT};

static int cat_night(void) { return cat.night > 96; }

static void cat_think(void)
{
    uint32_t now = fm1_ms, c = cat.clip;
    int playing = m1.notes_ms && now - m1.notes_ms < 1200u;
    int touched = m1.touched != M1T_NONE && now - m1.t_ms < 1500u;
    if (playing || touched)
        cat.quiet_since = now;
    if (c == CAT_SLEEP || c == CAT_ENDORT) {                                 /* asleep: playing, a touch or the day */
        if (playing || touched || cat.night < 48) {
            cat_play(CAT_WAKE);
            cat.next_life = now + 20000u;
        }
        return;
    }
    if (c == CAT_WAKE)
        return;                                                              /* (he gets up first) */
    if (playing) {
        if (c != CAT_PLAY)
            cat_play(CAT_PLAY);                                              /* the music stops his business too */
        return;
    }
    if (touched && (c == CAT_IDLE || c == CAT_PLAY || c == CAT_LOOK_L || c == CAT_LOOK_R)) {
        uint32_t want = m1.touched == M1T_PAD || m1.touched == M1T_BTN ? CAT_LOOK_L : CAT_LOOK_R;
        if (c != want)
            cat_play(want);
        return;
    }
    if (cat.behaving)
        return;                                                              /* (his business: until the clip ends) */
    if (c != CAT_IDLE) {                                                     /* the music or the touch is over */
        cat_play(CAT_IDLE);
        return;
    }
    if (!cat.next_life)
        cat.next_life = now + 12000u;
    if ((int32_t)(now - cat.next_life) < 0)
        return;
    if (M1G[M1G_CAT] % 3u == 2u && !cat_night()) {                          /* SAGE: he only sits (and sleeps at night) */
        cat.next_life = now + 60000u;
        return;
    }
    if (cat_night())                                                         /* night: kneading, a yawn, then dodo */
        cat_play(CAT_NIGHT_LIFE[cat_rand(sizeof CAT_NIGHT_LIFE)]);
    else if (now - cat.quiet_since > 180000u)                                /* three minutes alone: sleepy */
        cat_play(cat_rand(2) ? CAT_BAILLEMENT : CAT_ENDORT);
    else
        cat_play(CAT_DAY_LIFE[cat_rand(sizeof CAT_DAY_LIFE)]);
    cat.next_life = M1G[M1G_CAT] % 3u == 1u ? now + 60000u + cat_rand(120000u)   /* CALME: every 1 .. 3 min */
                                            : now + 15000u + cat_rand(25000u);  /* VIVANT: 15 .. 40 s */
}

/* the next step of the clip when its time comes: decoded, and its rows drawn */
static void cat_tick(void)
{
    const cat_clip_t *cl = &CAT_CLIPS[cat.clip];
    const cat_step_t *s;
    const cat_frame_t *f;
    uint32_t now = fm1_ms;
    if (cat.t_next) {                                                        /* (0: the clip's first frame, now) */
        if ((int32_t)(now - cat.t_next) < 0)
            return;
        if (cat.step + 1u < cl->n) {
            cat.step++;
        } else if (cl->loop) {
            cat.step = 0;
        } else {                                                             /* the clip is over */
            uint32_t c = cat.clip;
            if (c == CAT_LOOK_L || c == CAT_LOOK_R) {
                cat.t_next = now + 100u;                                     /* he keeps looking */
                return;
            }
            cat_play(c == CAT_ENDORT || (c == CAT_PETRIR && cat_night()) ? CAT_SLEEP : CAT_IDLE);
            cl = &CAT_CLIPS[cat.clip];
        }
    }
    s = &CAT_STEPS[cl->start + cat.step];
    cat.t_next = now + s->d10 * 10u;
    if (!cat.t_next)
        cat.t_next = 1;
    cat_frame(s->f);
    f = &CAT_FRAMES[s->f];
    {
        uint32_t oy0 = cat.box[1], oy1 = cat.box[1] + cat.box[3], ny0 = f->y, ny1 = f->y + f->h, y0, y1;
        y0 = !cat.box[3] ? ny0 : !f->h ? oy0 : oy0 < ny0 ? oy0 : ny0;
        y1 = oy1 > ny1 ? oy1 : ny1;
        cat.box[0] = f->x; cat.box[1] = f->y; cat.box[2] = f->w; cat.box[3] = f->h;
        if (y1 >= 56u)
            y1 = 64u;                                                        /* (his shadow goes with him) */
        if (!cat.full && y1 > y0)
            scene_rows(y0, y1 > CAT_SCENE ? CAT_SCENE : y1);
    }
}

/* ------------------------------------------------------------ the band */
static const char *const M1_F_NAME[4] = {"JOUR / NUIT", "FM", "TEMPS", "ECHO"};
static const char *const M1_B_NAME[18] = {"ONDE 1", "ONDE 2", "", "", "OCTAVE", "OCTAVE", "ARP", "REPEAT", "GAMME",
                                          "ACCORD", "REGLAGES", "", "", "", "VINTAGE", "", "", ""};

static void m1_band(void)
{
    char lab[24], val[12];
    const uint8_t *rgb = M1_PAD_RGB[m1.pad];
    uint16_t acc = M1_RGB(rgb[0], rgb[1], rgb[2]), bg = M1_RGB(24, 20, 26), ink = M1_RGB(244, 236, 220);
    const char *snd = N_VILLE_MODE[M1T->p[P_E0] % VM_N];
    uint32_t sig, show = m1.touched != M1T_NONE && m1.touched != M1T_PAD && fm1_ms - m1.t_ms < 2500u;
    int32_t bar = -1;
    lab[0] = val[0] = 0;
    if (show) {
        switch (m1.touched) {
        case M1T_KNOB: str_cpy(lab, M1_K_NAME[m1.what % M1_NK], sizeof lab); bar = m1.value; break;
        case M1T_FADER:
            str_cpy(lab, m1.what == 1u && VM_TOWN((uint32_t)M1T->p[P_E0]) ? "MONTEE" : M1_F_NAME[m1.what & 3u], sizeof lab);
            bar = m1.value;
            break;
        case M1T_VOL: str_cpy(lab, "VOLUME", sizeof lab); bar = m1.value; break;
        case M1T_WAVE:
            str_cpy(lab, m1.what == 1u ? "ONDE 1" : "ONDE 2", sizeof lab);
            str_cpy(val, m1.value < 22 ? "SCIE" : m1.value < 54 ? "MIX" : m1.value < 96 ? "TRIANGLE" : "CARRE", sizeof val);
            break;
        default:
            str_cpy(lab, m1.what < 18u ? M1_B_NAME[m1.what] : "", sizeof lab);
            if (m1.what == FM1_BTN_OCT_DOWN || m1.what == FM1_BTN_OCT_UP) {
                val[0] = '+';
                fmt_int(val + (m1.value > 0), m1.value);
            } else {
                str_cpy(val, m1.value ? "ON" : "OFF", sizeof val);
            }
            break;
        }
    }
    sig = ((uint32_t)m1.pad << 26) ^ ((uint32_t)(show ? m1.touched : 0) << 22) ^ ((uint32_t)m1.what << 14) ^
          ((uint32_t)M1T->p[P_E0] << 8) ^ (uint32_t)(m1.value & 0xFF) ^ (show && !lab[0] ? 0x80000000u : 0u);
    if (sig == cat.band_sig)
        return;
    cat.band_sig = sig;
    cv_begin(240u, 36u, bg);
    if (!show || !lab[0]) {
        cv_text(10, 9, &AF_M, snd, acc);
    } else {
        cv_text(10, 4, &AF_M, lab, ink);
        cv_text_r(230, 4, &AF_M, val[0] ? val : snd, val[0] ? ink : acc, bg);
        if (bar >= 0) {
            cv_rect(10, 27, 220, 3, M1_RGB(70, 62, 66));
            cv_rect(10, 27, 220 * clamp(bar, 0, 127) / 127, 3, acc);
        }
    }
    cv_blit(0, BAND_Y);
}

/* ------------------------------------------------------------ GLOBE: the settings, one knob each */
#define G1_BG M1_RGB(24, 20, 26)
static void m1_globe_row(uint32_t r)
{
    char b[12], k[4] = {'K', (char)('2' + r), 0, 0};
    const uint8_t *rgb = M1_PAD_RGB[m1.pad];
    uint16_t acc = M1_RGB(rgb[0], rgb[1], rgb[2]), ink = M1_RGB(244, 236, 220), dim = M1_RGB(120, 110, 122);
    int hot = m1.grow == r && fm1_ms - m1.grow_ms < 3000u;
    cv_begin(240u, 28u, hot ? M1_RGB(44, 38, 48) : G1_BG);
    cv_text(10, 6, &AF_S, k, hot ? acc : dim);
    cv_text(38, 5, &AF_M, G1_NAME[r], hot ? ink : M1_RGB(200, 192, 200));
    cv_text_r(230, 5, &AF_M, g1_vname(r, b), hot ? acc : ink, hot ? M1_RGB(44, 38, 48) : G1_BG);
    cv_blit(0, 40u + r * 28u);
}

/* the whole page once, then a row when it changes (and the highlight going out after 3 s) */
static void m1_globe_draw(int all)
{
    static uint32_t sig[G1_N];
    uint32_t r;
    if (all) {
        lcd_fill(0, 0, 240, 240, G1_BG);
        cv_begin(240u, 36u, G1_BG);
        cv_text(10, 10, &AF_M, "REGLAGES", M1_RGB(244, 236, 220));
        cv_text_r(230, 12, &AF_S, "GLOBE: retour", M1_RGB(120, 110, 122), G1_BG);
        cv_blit(0, 2);
        cv_begin(240u, 30u, G1_BG);
        cv_text(10, 8, &AF_S, "MODULAR ONE  SMK-37", M1_RGB(120, 110, 122));
        cv_blit(0, 208);
    }
    for (r = 0; r < G1_N; r++) {
        uint32_t v = g1_get(r) | (m1.grow == r && fm1_ms - m1.grow_ms < 3000u ? 0x100u : 0u) | (uint32_t)m1.pad << 12;
        if (all || v != sig[r]) {
            sig[r] = v;
            m1_globe_row(r);
        }
    }
}

/* ------------------------------------------------------------ the screen asleep (GLOBE VEILLE ECRAN) */
static struct { uint8_t st; uint32_t t; } m1_scr;                            /* 0 on, 1 off, 2 waking (SLPOUT sent) */

/* 1 = draw; 0 = the panel is dark or waking */
static int m1_screen_awake(void)
{
    uint32_t lim = SCR_MIN[scr_get() % 5u] * 60000u, now = fm1_ms;
    int active = (int32_t)(now - m1.active_ms) < 1000;
    switch (m1_scr.st) {
    case 0:
        if (!lim || (int32_t)(now - m1.active_ms) < (int32_t)lim)
            return 1;
        lcd_power(0);                                                        /* DISPOFF, SLPIN */
        m1_scr.st = 1;
        m1_scr.t = now;
        return 0;
    case 1:
        if (active && now - m1_scr.t >= 120u) {
            lcd_power(1);                                                    /* SLPOUT */
            m1_scr.st = 2;
            m1_scr.t = now;
        }
        return 0;
    default:
        if (now - m1_scr.t < 120u)
            return 0;
        m1_scr.st = 0;
        cat.full = 1;                                                        /* everything again, then the panel on */
        cat.band_sig = 0xFFFFFFFFu;
        return 2;
    }
}

static void m1_draw(void)
{
    static uint8_t globe;
    uint32_t now = fm1_ms;
    int32_t night = m1.fader[0] == 0xFF ? 0 : 127 - m1.fader[0], winter = m1.fader[1] == 0xFF ? 0 : m1.fader[1];
    int wake = m1_screen_awake();
    if (!wake)
        return;
    if (m1.globe) {
        m1_globe_draw(!globe || wake == 2);
        globe = 1;
        if (wake == 2)
            lcd_power(2);
        return;
    }
    if (globe) {                                                             /* back from GLOBE: everything again */
        globe = 0;
        cat.full = 1;
        cat.band_sig = 0xFFFFFFFFu;
    }
    if (night - cat.night > 2 || cat.night - night > 2 || (night != cat.night && (night == 0 || night == 127))) {
        cat.night = (int16_t)night;
        cat.full = 1;
    }
    if (winter - cat.winter > 2 || cat.winter - winter > 2 || (winter != cat.winter && (winter == 0 || winter == 127))) {
        cat.winter = (int16_t)winter;
        cat.full = 1;
    }
    if (cat.winter > 16 && now - cat.snow_t >= 110u) {                       /* the snow falls */
        uint32_t k;
        cat.snow_t = now;
        for (k = 0; k < 32u; k++) {
            if (!snow[k].y || snow[k].y >= CAT_SCENE - 2u) {
                snow[k].x = (uint8_t)cat_rand(CAT_G);
                snow[k].y = (uint8_t)(snow[k].y ? 1u : 1u + cat_rand(60u));
                snow[k].dx = (uint8_t)cat_rand(3);
            }
            snow[k].y++;
            if (!cat_rand(3))
                snow[k].x = (uint8_t)((snow[k].x + CAT_G + snow[k].dx - 1u) % CAT_G);
        }
        cat.full = 1;
    }
    cat_think();
    cat_tick();
    if (cat.full) {
        cat.full = 0;
        scene_rows(0, CAT_SCENE);
    }
    m1_band();
    if (wake == 2)
        lcd_power(2);                                                        /* DISPON: the picture is there */
    m1.dirty = 0;
}
