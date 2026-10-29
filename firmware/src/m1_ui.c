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
 *                1/16 repeat while held; FX the vintage (fx.c); GLOBE the settings: one knob, one setting (below)
 *   the tape     PLAY plays the sketches kept (and stops); REC held 0.6 s keeps the last phrase played (again at once:
 *                one phrase further back); < > the previous / next sketch (stopped: where PLAY starts); < and > held
 *                together 1.5 s erases the sketch shown (m1_tape.c, m1_book.c)
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
enum { M1T_NONE, M1T_KNOB, M1T_FADER, M1T_WAVE, M1T_BTN, M1T_PAD, M1T_VOL, M1T_TAPE, M1T_PITCH };
static volatile int16_t m1_pw_raw, m1_pw_centre;   /* the pitch wheel as read, its centre (main.c smk_wheels) */
enum { M1_B_LEFT = 2, M1_B_RIGHT = 3, M1_B_PLAY = 16, M1_B_REC = 17 };
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
    uint32_t active_ms;      /* the last hand on anything (the screen sleeps after MENU's time) */
    uint8_t grow;            /* GLOBE: the setting turned last (0xFF none) */
    char msg[24];            /* the tape's words (m1_say), shown in the band 2.5 s */
    uint32_t msg_ms, rec_ms, del_ms;   /* REC pressed; < and > both held since */
    uint8_t rec_done, del_done;
    uint32_t grow_ms;
} m1 = {.fader = {0xFF, 0xFF, 0xFF, 0xFF}, .grow = 0xFF};

/* GLOBE, the settings: K2 .. K7 one each. Kept with the settings, in bytes no engine uses: favorites.factory[13]
 * 24..31 (VILLE's favorites are bits of bytes 0..1; Modular One shows none). [0] 'M' once written. */
enum { G1_CH, G1_VEL, G1_TUNE, G1_PADS, G1_SCR, G1_CAT, G1_N };
static const char *const G1_NAME[G1_N] = {"CANAL MIDI", "VELOCITE", "ACCORDAGE", "PADS", "VEILLE", "LE CHAT"};
static const char *const G1_VEL_N[4] = {"NORMALE", "DOUCE", "DURE", "FIXE"};
static const char *const G1_PADS_N[4] = {"TAMISEE", "DOUCE", "CLAIRE", "VIVE"};
static const char *const G1_SCR_N[5] = {"JAMAIS", "5 MIN", "15 MIN", "30 MIN", "60 MIN"};
static const char *const G1_CAT_N[3] = {"VIVANT", "CALME", "SAGE"};
#define M1G (favorites.factory[13] + 24)
enum { M1G_MAGIC, M1G_VEL, M1G_PADS, M1G_CAT };

static uint32_t g1_n(uint32_t r)
{
    return r == G1_CH ? menu_n(MI_TCH1) : r == G1_VEL ? 4u : r == G1_TUNE ? menu_n(MI_TUNE) : r == G1_PADS ? 4u :
           r == G1_SCR ? 5u : 3u;
}
static uint32_t g1_get(uint32_t r)
{
    return r == G1_CH ? menu_get(MI_TCH1) : r == G1_VEL ? M1G[M1G_VEL] % 4u : r == G1_TUNE ? menu_get(MI_TUNE) :
           r == G1_PADS ? M1G[M1G_PADS] % 4u : r == G1_SCR ? scr_get() : M1G[M1G_CAT] % 3u;
}
/* the value's words (b: 12 bytes) */
static const char *g1_vname(uint32_t r, char *b)
{
    uint32_t v = g1_get(r);
    switch (r) {
    case G1_CH: return menu_vname(MI_TCH1, v);
    case G1_VEL: return G1_VEL_N[v];
    case G1_TUNE: str_cpy(b, menu_vname(MI_TUNE, v), 8); str_cpy(b + str_len(b), " CT", 4); return b;
    case G1_PADS: return G1_PADS_N[v];
    case G1_SCR: return G1_SCR_N[v % 5u];
    default: return G1_CAT_N[v];
    }
}
static void g1_put(uint32_t r, uint32_t v)
{
    switch (r) {
    case G1_CH: menu_put(MI_TCH1, v); break;
    case G1_VEL: M1G[M1G_VEL] = (uint8_t)v; smk_vel_curve = (uint8_t)v; break;
    case G1_TUNE: menu_put(MI_TUNE, v); break;
    case G1_PADS: M1G[M1G_PADS] = (uint8_t)v; break;
    case G1_SCR: scr_put(v); break;
    default: M1G[M1G_CAT] = (uint8_t)v; break;
    }
    M1G[M1G_MAGIC] = 'M';
    M1G[4] = (uint8_t)menu_get(MI_TUNE);                                     /* (the project's two: kept here too, */
    M1G[5] = (uint8_t)menu_get(MI_TCH1);                                     /*  Modular One has no project) */
    settings_save();                                                         /* (queued: settings_poll writes it) */
}
static void g1_turn(uint32_t r, int32_t s)
{
    int32_t n = (int32_t)g1_n(r), v = (int32_t)g1_get(r) + (s > 0 ? 1 : -1);
    if (r == G1_TUNE && (s > 1 || s < -1))
        v += s > 0 ? 4 : -4;                                                 /* (a fast turn: 5 cents) */
    g1_put(r, (uint32_t)clamp(v, 0, n - 1));
    m1.grow = (uint8_t)r;
    m1.grow_ms = fm1_ms;
    m1.dirty = 1;
}
static void g1_boot(void)                                                    /* the saved settings, at power-on */
{
    if (M1G[M1G_MAGIC] != 'M') {
        M1G[M1G_VEL] = 0;
        M1G[M1G_PADS] = 2;
        M1G[M1G_CAT] = 0;
        return;
    }
    smk_vel_curve = (uint8_t)(M1G[M1G_VEL] % 4u);
    if (M1G[4] < menu_n(MI_TUNE))
        menu_put(MI_TUNE, M1G[4]);
    if (M1G[5] < menu_n(MI_TCH1))
        menu_put(MI_TCH1, M1G[5]);
}

#define M1T (&trk[0])

static void m1_touch(uint32_t kind, uint32_t what, int32_t v)
{
    m1.touched = (uint8_t)kind;
    m1.what = (uint8_t)what;
    m1.value = (int16_t)v;
    m1.t_ms = m1.active_ms = fm1_ms;
    m1.dirty = 1;
}

static void m1_say(const char *s)
{
    str_cpy(m1.msg, s, sizeof m1.msg);
    m1.msg_ms = m1.active_ms = fm1_ms;
    m1.touched = M1T_TAPE;
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
    g1_boot();
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
        m1.grow = 0xFF;
        m1.active_ms = fm1_ms;
        m1.dirty = 1;
        break;
    case 14:                                                                 /* FX: the vintage */
        m1.vintage ^= 1u;
        m1_vintage = m1.vintage;
        m1_touch(M1T_BTN, id, m1.vintage);
        break;
    case M1_B_PLAY:                                                          /* the tape: play / stop */
        if (!pl_count && !demo_on) {
            m1_say("CARNET VIDE");
            break;
        }
        demo_req = demo_on ? 2u : 1u;
        m1_touch(M1T_TAPE, id, 0);
        break;
    case M1_B_REC:                                                           /* (held: m1_tape_held) */
        m1.rec_ms = fm1_ms;
        m1.rec_done = 0;
        break;
    case M1_B_LEFT: case M1_B_RIGHT:
        if (!pl_count)
            break;
        if (demo_on) {
            demo_req = id == M1_B_RIGHT ? 3u : 5u;                            /* next / previous */
        } else {
            pl_start = (uint8_t)((pl_start + (id == M1_B_RIGHT ? 1u : pl_count - 1u)) % pl_count);
            pl_cursor = pl_start;
        }
        m1_touch(M1T_TAPE, id, 0);
        break;
    default:
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
        if (s && m1.globe) {                                                 /* GLOBE: K2 .. K7 the settings */
            m1.active_ms = fm1_ms;
            if (k < G1_N)
                g1_turn(k, s);
        } else if (s) {
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
    {   /* the tape's held gestures */
        uint32_t b = fm1_in.buttons;
        if ((b >> M1_B_REC) & 1u) {
            if (!m1.rec_done && fm1_ms - m1.rec_ms >= 600u) {               /* REC held: keep the last phrase */
                m1.rec_done = 1;
                m1_keep_req = 1;
                m1_say("ON GARDE...");
            }
        }
        if (((b >> M1_B_LEFT) & 1u) && ((b >> M1_B_RIGHT) & 1u) && pl_count && !demo_on) {
            if (!m1.del_ms)
                m1.del_ms = fm1_ms ? fm1_ms : 1u, m1.del_done = 0;
            if (!m1.del_done && fm1_ms - m1.del_ms >= 1500u) {              /* < and > held: erase the sketch shown */
                m1.del_done = 1;
                m1_del_req = (uint8_t)(pl_start % pl_count + 1u);
            }
        } else {
            m1.del_ms = 0;
        }
    }
    if (fm1_in.frames > 200u)
        m1_faders_apply(m1.fader[0] == 0xFF);
    m1_modwheel();
    {   /* the pitch wheel: shown while it bends (cents, the bend range of 2 semitones) */
        static int16_t last;
        int32_t b = smk_bend14;
        if (b != last) {
            last = (int16_t)b;
            m1_touch(M1T_PITCH, 0, b * 200 / 8191);
        }
    }
    {   /* the cat listens: the last key */
        uint32_t n = fm1_input_note_edges();
        if (n) {
            uint32_t i;
            m1.notes_ms = m1.active_ms = fm1_ms;
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
    static const uint8_t LIT[4] = {20, 45, 75, 100}, DIM[4] = {4, 7, 11, 15};   /* GLOBE LUMIERE PADS, % */
    uint32_t b = M1G[M1G_PADS] % 4u;
    for (p = 0; p < 16u; p++) {
        const uint8_t *c = M1_PAD_RGB[p];
        uint32_t pc = p == m1.pad ? LIT[b] : DIM[b];
        padled_set(p, (uint8_t)(c[0] * pc / 100u), (uint8_t)(c[1] * pc / 100u), (uint8_t)(c[2] * pc / 100u));
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
    fm1_led_key(M1_B_PLAY, demo_on);
    fm1_led_key(M1_B_REC, ((fm1_in.buttons >> M1_B_REC) & 1u) || fm1_ms - m1.msg_ms < 400u);
}
