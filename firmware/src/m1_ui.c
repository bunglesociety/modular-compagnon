/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular One on the SMK-37: the controls (what you see is what you get: one control, one job, no bank, no menu).
 *   16 pads      the sounds (VILLE, eng_ville.c). Top row: MODULAR JUNO SUPERSAW PWM TAPE FM SOUFFLE CREPUSC;
 *                bottom row: FLUTE BRASS STRINGS CHOIR RAIN DEEP PULSE MEMORY. A pad loads its sound.
 *   K1           the volume (smk_input.h: the FM-1's MASTER pot, master.c)
 *   K2 .. K8     CUTOFF RESO ATTACK DECAY SUSTAIN RELEASE DETUNE
 *   faders       1 day (up) / night (down), 2 FM (the winter; on SOUFFLE / CREPUSC the Montee), 3 echo TIME (free,
 *                10 ms .. 0.75 s), 4 ECHO (the send and its repeats)
 *   KNOB BANK / FADER BANK   WAVE 1 / WAVE 2: the next wave of the vending machines (lit where a sound has them)
 *   pitch wheel  bend (seq.c, as Felucca); mod wheel: the cutoff waves, faster as it goes up (the track LFO)
 *   OCTAVE- / +  octaves; ARP the arpeggio; SCALE the scale (snap); CHORD a chord on each key; NOTE REPEAT a
 *                1/16 repeat while held; FX the vintage (fx.c); GLOBE the settings (m1_draw)
 *   37 keys      their notes (seq.c: the 27 middle keys, the 10 outer ones) on part 1
 * The screen: m1_draw.c. Only part 1 plays; the other three are silent. */

/* the pads: hardware pad -> VILLE sound; their colours (RGB, dim and lit) */
static const uint8_t M1_PAD_SOUND[16] = {0, 1, 2, 3, 4, 5, 14, 15, 6, 7, 8, 9, 10, 11, 12, 13};
static const uint8_t M1_PAD_RGB[16][3] = {
    {120, 160, 70}, {200, 110, 40}, {220, 160, 50}, {150, 120, 200}, {170, 120, 80}, {150, 200, 255},
    {240, 200, 140}, {230, 130, 90},
    {180, 220, 160}, {255, 70, 160}, {80, 220, 255}, {200, 120, 255}, {80, 140, 255}, {40, 80, 200},
    {255, 90, 60}, {200, 160, 230},
};
enum { M1_K_CUT, M1_K_RES, M1_K_ATK, M1_K_DEC, M1_K_SUS, M1_K_REL, M1_K_DTN, M1_NK };
static const uint8_t M1_K_PARAM[M1_NK] = {P_E3, P_E4, P_ATK, P_DEC, P_SUS, P_REL, P_E5};
static const char *const M1_K_NAME[M1_NK] = {"CUTOFF", "RESO", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "DETUNE"};

/* what the screen shows: the last control touched (m1_draw.c) */
enum { M1T_NONE, M1T_KNOB, M1T_FADER, M1T_WAVE, M1T_BTN, M1T_PAD, M1T_VOL };
static struct {
    uint8_t pad;             /* the sound's pad (hardware number) */
    uint8_t touched, what;   /* M1T_*, which control */
    int16_t value;           /* its value, 0..127 (or a wave notch) */
    uint32_t t_ms;           /* when */
    uint8_t vintage, globe;  /* FX on; the GLOBE screen open */
    uint8_t fader[4];        /* the faders as applied (0xFF: not read yet) */
    uint32_t notes_ms;       /* the last note played (the cat listens) */
    uint8_t vel;             /* its velocity */
    uint8_t dirty;
} m1 = {.fader = {0xFF, 0xFF, 0xFF, 0xFF}};

#define M1T (&trk[0])

static void m1_touch(uint32_t kind, uint32_t what, int32_t v)
{
    m1.touched = (uint8_t)kind;
    m1.what = (uint8_t)what;
    m1.value = (int16_t)v;
    m1.t_ms = fm1_ms;
    m1.dirty = 1;
}

/* the waves the sound has (MODULAR TAPE FM: both; JUNO: WAVE 1, the saw / pulse mix); 0 none */
static uint32_t m1_waves(void)
{
    uint32_t m = (uint32_t)M1T->p[P_E0];
    return m == VM_MODULAR || m == VM_TAPE || m == VM_FM || VM_TOWN(m) ? 3u : m == VM_JUNO ? 1u : 0u;
}

/* the faders onto the sound (absolute: where a fader stands is what plays). Fader 1 up = day */
static void m1_faders_apply(int force)
{
    uint32_t i;
    track_t *t = M1T;
    for (i = 0; i < 4u; i++) {
        uint32_t v = fm1_in.fader[i];
        if (!force && v == m1.fader[i])
            continue;
        if (!force && m1.fader[i] != 0xFF)
            m1_touch(M1T_FADER, i, (int32_t)v);
        m1.fader[i] = (uint8_t)v;
        switch (i) {
        case 0: t->p[P_E6] = (int16_t)(127 - v); break;                     /* NUIT */
        case 1: t->p[P_E7] = (int16_t)v; break;                             /* HIVER (the Montee on TOWN) */
        case 2: m1_dly_target = 441u + v * 257u; break;                      /* echo TIME: 10 ms .. 0.75 s */
        default:
            t->p[P_DLY] = (int16_t)v;                                        /* ECHO: the send .. */
            song.g[G_DFDBK] = (int16_t)(25 + v * 70 / 127);                  /* .. and the repeats */
            break;
        }
    }
}

static void m1_sound(uint32_t pad)
{
    track_t *t = M1T;
    m1.pad = (uint8_t)(pad % 16u);
    fm1_irq_off();
    t->eng_req = ENGI_VILLE;
    fm1_irq_on();
    apply_preset_to(t, M1_PAD_SOUND[m1.pad]);
    t->p[P_REV] = 0;                                                         /* no reverb (the city's echo only) */
    t->p[P_LWAVE] = 0;                                                       /* the mod wheel's LFO: a sine */
    m1_faders_apply(1);                                                      /* the faders stay what they say */
    m1_touch(M1T_PAD, m1.pad, 0);
}

/* the mod wheel: the cutoff waves, slow and shallow at the bottom, fast and deep at the top */
static void m1_modwheel(void)
{
    static uint8_t last = 0xFF;
    uint32_t w = smk_modw;
    track_t *t = M1T;
    if (w == last)
        return;
    last = (uint8_t)w;
    t->p[P_LD_FLT] = (int16_t)(w * 48 / 127);
    t->p[P_LRATE] = (int16_t)(w ? 50 + w * 50 / 127 : 60);
}

static void m1_init(void)
{
    uint32_t k;
    for (k = 1; k < NTRK; k++)                                               /* parts 2..4 silent */
        trk[k].p[P_LEVEL] = 0;
    song.sel = 0;
    song.g[G_RTYPE] = 0;
    song.g[G_DTIME] = 1;
    song.g[G_DMIX] = 100;
    smk_pad_free = 1;                                                        /* pads are ours: not notes */
    m1_sound(0);
}

static void m1_btn(uint32_t id, int down)
{
    track_t *t = M1T;
    if (!down) {
        if (id == 7u)
            perf_press(PF_R16, 0);
        return;
    }
    switch (id) {
    case SMK_BTN_KNOB_BANK: case SMK_BTN_FADER_BANK: {                       /* WAVE 1 / WAVE 2: the next notch */
        uint32_t w = id == SMK_BTN_KNOB_BANK ? 1u : 2u, p = id == SMK_BTN_KNOB_BANK ? P_E1 : P_E2;
        static const int16_t WV[4] = {0, 43, 64, 127};                       /* saw, between, triangle, square */
        uint32_t k;
        if (!(m1_waves() & w))
            break;
        for (k = 0; k < 4u && WV[k] <= t->p[p]; k++)
            ;
        t->p[p] = WV[k % 4u];
        m1_touch(M1T_WAVE, w, t->p[p]);
        break;
    }
    case FM1_BTN_OCT_DOWN: case FM1_BTN_OCT_UP:
        song.octave = (int8_t)clamp(song.octave + (id == FM1_BTN_OCT_UP ? 1 : -1), -3, 3);
        m1_touch(M1T_BTN, id, song.octave);
        break;
    case 6:                                                                  /* ARP */
        t->p[P_AMODE] = (int16_t)(t->p[P_AMODE] ? 0 : 1);
        m1_touch(M1T_BTN, id, t->p[P_AMODE]);
        break;
    case 7:                                                                  /* NOTE REPEAT: while held */
        perf_press(PF_R16, 1);
        m1_touch(M1T_BTN, id, 1);
        break;
    case 8:                                                                  /* SCALE: snap to the scale */
        t->p[P_QUANT] = (int16_t)(t->p[P_QUANT] ? QN_OFF : QN_SNAP);
        if (t->p[P_QUANT] && !t->p[P_SCALE])
            t->p[P_SCALE] = 2;                                               /* (minor, the film music default) */
        m1_touch(M1T_BTN, id, t->p[P_QUANT]);
        break;
    case 9:                                                                  /* CHORD: a triad of the scale */
        t->p[P_CHRD] = (int16_t)(t->p[P_CHRD] ? CH_OFF : CH_DIA3);
        m1_touch(M1T_BTN, id, t->p[P_CHRD]);
        break;
    case 10:                                                                 /* GLOBE: the settings */
        m1.globe ^= 1u;
        m1.dirty = 1;
        break;
    case 14:                                                                 /* FX: the vintage */
        m1.vintage ^= 1u;
        m1_vintage = m1.vintage;
        m1_touch(M1T_BTN, id, m1.vintage);
        break;
    default:                                                                 /* (the tape player: m1_tape.c) */
        break;
    }
}

static void m1_input(void)
{
    uint32_t released = 0, pressed = fm1_input_edges(&released), k;
    int32_t e;
    master_poll();
    midi_take();
    {   /* the volume (K1): only to show it */
        static int32_t last = -1;
        int32_t v = master_knob / 16;
        if (last >= 0 && (v - last > 6 || last - v > 6))
            m1_touch(M1T_VOL, 0, v * 127 / 1023);
        if (last < 0 || v - last > 6 || last - v > 6)
            last = v;
    }
    while ((e = smk_padev_take()) >= 0)                                      /* pads: a sound */
        if (e & 0x7F)
            m1_sound((uint32_t)e >> 8);
    for (k = 0; k < M1_NK; k++) {                                            /* K2 .. K8 */
        int32_t s = fm1_enc_take(k + 1u);
        if (s) {
            uint32_t id = M1_K_PARAM[k];
            int32_t lo = 0, hi = 127, v;
            if (s > 1 || s < -1)
                s *= 2;                                                      /* a fast turn goes further */
            v = clamp(M1T->p[id] + s, lo, hi);
            M1T->p[id] = (int16_t)v;
            m1_touch(M1T_KNOB, k, v);
        }
    }
    for (k = 0; k < FM1_NBTN; k++) {
        if ((pressed >> k) & 1u)
            m1_btn(k, 1);
        if ((released >> k) & 1u)
            m1_btn(k, 0);
    }
    if (fm1_in.frames > 200u)
        m1_faders_apply(m1.fader[0] == 0xFF);
    m1_modwheel();
    {   /* the cat listens: the last key */
        uint32_t n = fm1_input_note_edges();
        if (n) {
            uint32_t i;
            m1.notes_ms = fm1_ms;
            for (i = 0; i < 27u; i++)
                if ((n >> i) & 1u)
                    m1.vel = fm1_in.key_vel[SMK_KEY_F3 + i];
        }
    }
}

static void m1_leds(void)
{
    uint32_t p, w = m1_waves();
    track_t *t = M1T;
    for (p = 0; p < 16u; p++) {
        const uint8_t *c = M1_PAD_RGB[p];
        uint32_t lit = p == m1.pad, d = lit ? 1u : 7u;
        padled_set(p, (uint8_t)(c[0] / d), (uint8_t)(c[1] / d), (uint8_t)(c[2] / d));
    }
    fm1_led_key(SMK_BTN_KNOB_BANK, (w & 1u) != 0);
    fm1_led_key(SMK_BTN_FADER_BANK, (w & 2u) != 0);
    fm1_led_key(FM1_BTN_OCT_DOWN, song.octave < 0);
    fm1_led_key(FM1_BTN_OCT_UP, song.octave > 0);
    fm1_led_key(6, t->p[P_AMODE] != 0);
    fm1_led_key(7, (fm1_in.buttons >> 7) & 1u);
    fm1_led_key(8, t->p[P_QUANT] != 0);
    fm1_led_key(9, t->p[P_CHRD] != 0);
    fm1_led_key(10, m1.globe);
    fm1_led_key(14, m1.vintage);
}
