/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* VILLE: the synth voice of Modular Town V3 (the Amorph patch, processor IndustrieSynth), for Modular One on the
 * SMK-37. One voice, many records: MODE picks the oscillators, everything then goes through the same filter.
 *   MODE   0 MODULAR  the vending machines' wave blend (saw > triangle > square), two oscillators
 *          1 JUNO     saw + a breathing pulse (WAVE 1 sets the mix), a square sub-octave; chorus (the part's FX)
 *          2 SUPERSAW six slightly detuned saws
 *          3 PWM      two pulses breathing each at its own rate
 *          4 TAPE     MODULAR on a tired tape: wow and flutter, soft saturation, dull highs
 *          5 FM       MODULAR in winter: the FM voice (WINTER held at full)
 *          6 FLUTE    the kiosk's Mellotron flutes: a full flute, its breath and chiff, a deep player's vibrato, tape
 *          7 BRASS .. 13 MEMORY  Neon Town: three detuned saws (BRASS STRINGS) or its other colours (CHOIR RAIN DEEP
 *                     PULSE MEMORY), a delayed film-brass vibrato, the brass swell on the filter
 *   WAVE 1 / WAVE 2  the two vending machines (MODULAR TAPE FM; JUNO: saw / pulse)
 *   CUT RES          the filter (two state-variable low-passes, 24 dB, the resonance on the second, a soft input)
 *   DTN              oscillator 2, +-12 semitones (64 = unison), as the patch's Detune
 *   NUIT             day (0) to night (127): darker filter, more resonance, the two waves drift apart, softer FM
 *   HIVER            the fade to the FM voice (the FM fader): cutoff -> index, resonance -> feedback
 *          14 SOUFFLE, 15 CREPUSC  the composer's own voice (Modular One on the FM-1, its TOWN engine): FM with
 *                     CUT the brightness, RES the grain, WAVE 1 the ratios (12 notches), DTN the bell detune, NUIT the
 *                     night, and HIVER the Montee (the index opens, a vibrato) 
 * Integer only: the patch's curves are approximated once per block (CTL samples). */

enum { VM_MODULAR, VM_JUNO, VM_SUPERSAW, VM_PWM, VM_TAPE, VM_FM, VM_FLUTE,
       VM_BRASS, VM_STRINGS, VM_CHOIR, VM_RAIN, VM_DEEP, VM_PULSE, VM_MEMORY, VM_SOUFFLE, VM_CREPUS, VM_N };
static const char *const N_VILLE_MODE[VM_N] = {"MODULAR", "JUNO", "SUPERSAW", "PWM STR", "TAPE", "FM", "FLUTE",
                                               "BRASS", "STRINGS", "CHOIR", "RAIN", "DEEP", "PULSE", "MEMORY",
                                               "SOUFFLE", "CREPUSC"};
#define VM_NEON(m) ((m) >= VM_BRASS && (m) <= VM_MEMORY)
#define VM_TOWN(m) ((m) >= VM_SOUFFLE)

/* per voice, in the part's engine memory: the extra phases and the filters a mode needs */
typedef struct {
    uint32_t ph[5];          /* SUPERSAW S1..S4; Neon's two detuned saws (0, 1); FM modulator 2 (4) */
    int32_t f[4];            /* CHOIR's two formant band-passes; FLUTE's breath band-pass (0, 1) and chiff (2) */
    int32_t tp;              /* TAPE / MEMORY / FLUTE: the dull one-pole */
} vl_voice_t;
typedef struct {
    vl_voice_t v[NVOICE];
    int32_t night;           /* Q15, glides ~0.75 s */
    int32_t winter;          /* Q15, glides ~25 ms */
    uint32_t pwm, w1, w2, w3, nz;   /* the slow PWM, the tape wow phases (per block), noise */
} vl_part_t;
ENG_MEM_FITS("VILLE", sizeof(vl_part_t));
static inline vl_part_t *vl_part(track_t *t) { return (vl_part_t *)ENG_MEM((uint32_t)(t - trk)); }

/* block-rate constants (CTL = 32 samples at 44.1 kHz: 1378 blocks a second) */
#define VL_BPS 1378
#define VL_BLK_INC(hz) ((uint32_t)((hz) * CTL * 4294967296.0 / FS))
#define VL_CUT_OCT 3588      /* one octave of cutoff, in the 0..127 << 8 units (30 Hz .. 16 kHz over 127) */
#define VL_Q20(x) ((int32_t)((x) * 1048576.0))

/* the vending machines' blend: saw > triangle (w 0..16384) > square (16384..32767), band-limited */
static inline int32_t vl_blend(uint32_t ph, uint32_t inc, int32_t w)
{
    int32_t tri = osc_tri(ph);
    if (w < 16384) {
        int32_t saw = osc_saw(ph, inc);
        return saw + (((tri - saw) * (w << 1)) >> 15);
    }
    return tri + (((osc_pulse(ph, inc, 0x80000000u) * 2 - tri) * ((w - 16384) << 1)) >> 15);
}

static inline uint32_t vl_scale(uint32_t inc, uint32_t q16) { return (uint32_t)(((uint64_t)inc * q16) >> 16); }

static void vl_note_on(track_t *t, voice_t *v)
{
    vl_part_t *pp = vl_part(t);
    vl_voice_t *x = &pp->v[(uint32_t)(v - t->v) % NVOICE];
    uint32_t k;
    v->ph[1] = VM_TOWN((uint32_t)t->p[P_E0]) ? 0 : v->ph[0] + 0x40000000u;
    v->ph[2] = 0;
    if (VM_TOWN((uint32_t)t->p[P_E0]))
        v->ph[0] = 0;
    v->s[0] = v->s[1] = v->s[2] = v->s[3] = 0;          /* the two filters */
    v->s[4] = v->s[5] = 0;                              /* FM low-pass and feedback */
    v->s[6] = 0;                                        /* the note's age, in blocks */
    for (k = 0; k < 5; k++)
        x->ph[k] = v->ph[0] + k * 0x2F1A0000u;          /* spread starts: no comb at the attack */
    x->f[0] = x->f[1] = x->f[3] = 0;
    x->f[2] = 32767;                                    /* FLUTE: the chiff starts full */
    x->tp = 0;
}

/* once per block and part: the night and winter glides, the slow LFOs */
static void vl_block(track_t *t)
{
    vl_part_t *pp = vl_part(t);
    int32_t nt = t->p[P_E6] * 258, wt = t->p[P_E0] == VM_FM ? 32767 : t->p[P_E7] * 258;
    pp->night += (nt - pp->night) >> 10;
    pp->winter += (wt - pp->winter) >> 5;
    pp->pwm += VL_BLK_INC(0.31);
    pp->w1 += VL_BLK_INC(0.55);
    pp->w2 += VL_BLK_INC(6.3);
    pp->w3 += VL_BLK_INC(0.21);
}

/* the FM ratios of the vending machines' notches (the patch's fmRatio), Q8 */
static const uint16_t VL_FMR[7] = {256, 512, 768, 128, 896, 1280, 1792};
/* Neon Town per colour: vibrato depth (Q20), the detune of the two extra saws (Q16), the swell's amount (Q8) */
static const int32_t VL_NDEP[7] = {VL_Q20(0.0045), VL_Q20(0.003), VL_Q20(0.0035), VL_Q20(0.008), 0, 0, VL_Q20(0.002)};
static const uint32_t VL_NDA[7] = {65805, 66001, 65805, 65805, 65805, 65805, 65805};
static const uint32_t VL_NDB[7] = {65287, 65077, 65287, 65287, 32768, 65287, 65287};
static const uint8_t VL_NSWA[7] = {255, 89, 51, 128, 77, 153, 128};
static const uint32_t VL_SS[4] = {64780, 66276, 64041, 67052};      /* supersaw 0.9885 1.0113 0.9772 1.0231 */

#include "mt_tables.h"
static const uint16_t MT_R1_Q8[12] = {256, 256, 256, 512, 512, 768, 256, 128, 256, 512, 256, 256};
static const uint16_t MT_R2_Q8[12] = {256, 512, 768, 256, 768, 256, 128, 256, 896, 1280, 1280, 1792};
static inline uint32_t vl_ratio(uint32_t inc, uint32_t r)   /* inc x r (Q8), kept below Nyquist */
{
    uint32_t lim = 0x73000000u / r;
    return (inc >> 8) > lim ? 0x73000000u : (inc >> 8) * r;
}
static inline int32_t mt_lerp(const uint16_t *tab, int32_t x, uint32_t last)   /* x: index << 8 */
{
    uint32_t i = (uint32_t)x >> 8;
    int32_t f = x & 255;
    if (i >= last)
        return tab[last];
    return tab[i] + (((tab[i + 1] - tab[i]) * f) >> 8);
}

/* SOUFFLE / CREPUSC: the TOWN engine of Modular One (FM-1), the same code; per voice ph[0..2] the carrier and the
 * two modulators, s[0] feedback, s[1] low-pass, s[2..5] the block glides, s[6] the vibrato phase of the part copy */
static void vl_town(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m, int32_t night)
{
    const int16_t *p = t->p;
    vl_part_t *pp = vl_part(t);
    int32_t mont = pp->winter;                           /* the FM fader is the Montee here */
    int32_t cut = clamp(((((int32_t)p[P_E3] << 8) + m->cutoff) * 129 >> 7) - mulq15(night, 5243), 0, 32767);
    int32_t res = clamp((((int32_t)p[P_E4] << 8) * 129 >> 7) + mulq15(night, 3277), 0, 32767);
    uint32_t tim = (uint32_t)(p[P_E1] * 12 >> 7) % 12u, i;
    int32_t idx, idx2, fb, g, lpg, vib;
    uint32_t inc1, inc2, im1, im2, ph, pm1, pm2;
    int32_t fbk = v->s[0], lp = v->s[1];
    int32_t idx0, idx20, g0, didx, didx2, dg;
    idx = mt_lerp(MT_IDX_Q12, cut, 128);
    idx = mulq15(idx, 32767 - mulq15(night, 20316));
    idx += mulq15(idx, mont);
    idx = mulq15(idx, 9830 + mulq15(m->envq15, 22937));
    idx = mulq15(idx, MT_KF_Q15[(m->pitch16 >> 4) & 127]);
    idx2 = mulq15(idx, 14746 - mulq15(night, 11797));
    fb = mulq15((res * 14418) >> 15, 32767 - mulq15(night, 27853));
    {
        int32_t fcn16 = 360 + ((cut * 1824) >> 15);
        int32_t dn = clamp(m->pitch16 - fcn16 + (64 << 4), 0, 128 << 4);
        int32_t trk = mt_lerp(MT_TRK_Q15, dn << 4, 128);
        if (trk < 19661)
            trk = 19661 + ((trk - 19661) >> 3);
        g = mulq15(trk, mt_lerp(MT_RESG_Q15, res, 128));
        g += mulq15(g, night >> 2);
        g = mulq15(g, 29491);
    }
    lpg = mt_lerp(MT_FMLP_Q15, night >> 3, 16);
    vib = (mulq15(sine_i(pp->pwm * 17u), mont) * 47) >> 15;   /* ~5.2 Hz, +-20 cents at full Montee */
    inc1 = m->inc + (uint32_t)((int32_t)(m->inc >> 12) * vib);
    inc2 = (uint32_t)(((uint64_t)m->inc * MT_DET_Q16[clamp((p[P_E5] - 64) * 3 + 192, 0, 384)]) >> 16);
    inc2 += (uint32_t)((int32_t)(inc2 >> 12) * vib);
    im1 = vl_ratio(inc1, MT_R1_Q8[tim]);
    im2 = vl_ratio(inc2, MT_R2_Q8[tim]);
    if (!v->s[5]) {
        v->s[2] = idx;
        v->s[3] = idx2;
        v->s[4] = g;
        v->s[5] = 1;
    }
    idx0 = v->s[2]; idx20 = v->s[3]; g0 = v->s[4];
    didx = idx - idx0; didx2 = idx2 - idx20; dg = g - g0;
    ph = v->ph[0]; pm1 = v->ph[1]; pm2 = v->ph[2];
    for (i = 0; i < n; i++) {
        int32_t m1, m2, car, s, ki = idx0 + ((didx * (int32_t)i) >> CTL_LOG2);
        int32_t ki2 = idx20 + ((didx2 * (int32_t)i) >> CTL_LOG2), kg = g0 + ((dg * (int32_t)i) >> CTL_LOG2);
        m1 = sine_i(pm1 + ((uint32_t)(fb * fbk) << 1));
        fbk = (fbk + m1) >> 1;
        m2 = sine_i(pm2);
        car = sine_i(ph + ((uint32_t)(ki * m1) << 5) + ((uint32_t)(ki2 * m2) << 5));
        lp += mulq15(car - lp, lpg);
        s = mulq15(lp, kg);
        out[i] += mulq15(mulq15(s, amp_at(m, i)), VOICE_FS) * 2;
        ph += inc1; pm1 += im1; pm2 += im2;
    }
    v->ph[0] = ph; v->ph[1] = pm1; v->ph[2] = pm2;
    v->s[0] = fbk; v->s[1] = lp;
    v->s[2] = idx; v->s[3] = idx2; v->s[4] = g;
}

/* FLUTE: the kiosk's Mellotron (the patch's ModularTownMellotron), as the patch plays it: no synth filter. A full
 * flute (the fundamental and four harmonics), its breath tuned near the note and the mouth noise, the chiff at the
 * attack, a deep player's vibrato with its tremolo, each key its own tape (tuning, level, brightness), then the tape
 * chain: low cut, a presence in the mids, soft saturation, dull highs, hiss. CUT darkens the tape, RES is the breath.
 * Per voice: s[0..1] the low cuts, s[2..3] the presence band-pass, s[4..5] the tape low-passes; f[0..1] the breath's
 * band-pass, f[2] the chiff. */
static void vl_flute(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m, uint32_t inc1)
{
    const int16_t *p = t->p;
    vl_part_t *pp = vl_part(t);
    vl_voice_t *x = &pp->v[(uint32_t)(v - t->v) % NVOICE];
    uint32_t note = v->note, i, ph = v->ph[0];
    int32_t det = ((int32_t)((note * 37u) % 13u) - 6) * 419;                  /* +-4 cents, Q20 */
    int32_t amp = 26870 + (int32_t)((note * 53u) % 9u) * 1475;               /* 0.82 .. 1.18 of the level, Q15 */
    int32_t br = 26214 + (int32_t)((note * 29u) % 7u) * 2294;                /* brightness 0.8 .. 1.22 */
    int32_t brk = clamp(p[P_E4] * 516, 0, 65535);                            /* RES: the breath, 64 = the patch's */
    int32_t lp2 = 2000 + p[P_E3] * 140;                                       /* CUT: the tape's top, ~470 Hz .. 6.5 kHz */
    int32_t chiff = x->f[2];
    int32_t fg = clamp((int32_t)(inc1 >> 13), 3267, 12950);                  /* the breath's band: ~2.2 x f */
    int32_t h1 = v->s[0], h2 = v->s[1], q1 = v->s[2], q2 = v->s[3], t1 = v->s[4], t2 = v->s[5];
    int32_t nz = pp->nz ? (int32_t)pp->nz : 0x2545F491;
    inc1 = (uint32_t)((int64_t)inc1 + (((int64_t)inc1 * det) >> 20));
    for (i = 0; i < n; i++) {
        int32_t s1 = sine_i(ph);
        int32_t harm = (sine_i(ph * 2u) * 8847 + sine_i(ph * 3u) * 3277 + sine_i(ph * 4u) * 1147 + sine_i(ph * 5u) * 393) >> 15;
        int32_t tone = s1 + ((harm * br) >> 15);
        int32_t no = (int32_t)(noise32(&nz) >> 17) - 16384;
        int32_t hp = no - x->f[1] - ((x->f[0] * 36045) >> 15);
        x->f[0] += (fg * hp) >> 15;
        x->f[1] += (fg * x->f[0]) >> 15;
        int32_t breath = ((x->f[0] * (2294 + ((11796 * chiff) >> 15))) >> 15) + (((no * (16384 + (s1 >> 1))) >> 15) * 1638 >> 15);
        int32_t y = ((tone * (32767 - ((9830 * chiff) >> 15))) >> 15) + ((breath * brk) >> 15);
        y = (((y * amp) >> 15) * 5) >> 3;                                    /* the level of the other sounds */
        /* the tape: two low cuts at 110 Hz, the presence at 1.3 kHz, saturation, 4.3 kHz then CUT, hiss */
        h1 += ((y - h1) * 512) >> 15; y -= h1;
        h2 += ((y - h2) * 512) >> 15; y -= h2;
        {
            int32_t bp = y - q2 - ((q1 * 39322) >> 15);
            q1 += (3031 * bp) >> 15;
            q2 += (3031 * q1) >> 15;
            y += (q1 * 7209) >> 15;
        }
        t1 += (((softclip((y * 7) >> 2) * 20316) >> 15) - t1) * 15000 >> 15;
        t2 += ((t1 - t2) * lp2) >> 15;
        y = t2 + (((int32_t)(noise32(&nz) >> 23) - 256) / 4);
        ph += inc1;
        out[i] += voice_amp(y, m, i) << 1;
    }
    x->f[2] = (chiff * 32178) >> 15;                                         /* the chiff: exp(-t / 40 ms) */
    pp->nz = (uint32_t)nz;
    v->ph[0] = ph;
    v->s[0] = h1; v->s[1] = h2; v->s[2] = q1; v->s[3] = q2; v->s[4] = t1; v->s[5] = t2;
}

static void vl_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    if (VM_TOWN((uint32_t)t->p[P_E0])) {
        vl_town(t, v, out, n, m, vl_part(t)->night);
        return;
    }
    {
    const int16_t *p = t->p;
    vl_part_t *pp = vl_part(t);
    uint32_t vi = (uint32_t)(v - t->v) % NVOICE;
    vl_voice_t *x = &pp->v[vi];
    uint32_t mode = (uint32_t)p[P_E0] < VM_N ? (uint32_t)p[P_E0] : 0u, i;
    int32_t night = pp->night, win = pp->winter;
    int32_t age = v->s[6];                              /* blocks since the note began */
    int32_t w1 = clamp(p[P_E1] * 258 + ((night * 10486) >> 15), 0, 32767);   /* + 0.32 night */
    int32_t w2 = clamp(p[P_E2] * 258 - ((night * 10486) >> 15), 0, 32767);
    int32_t res = clamp(p[P_E4] + ((night * 13) >> 15), 0, 127);              /* + 0.10 night */
    int32_t cut = (p[P_E3] << 8) + m->cutoff - ((night * 5450) >> 15)         /* - 0.16 night (21 units) */
                  + (((int32_t)v->mvel - 100) * 24);                          /* a hard key is a little brighter */
    uint32_t inc1 = m->inc, inc2;
    int32_t vib = 0;                                    /* pitch factor - 1, Q20 */
    tsvf_t f1c, f2c;

    /* pitch: the vibratos and the tape, then oscillator 2 (DTN, +-12 semitones) */
    if (VM_NEON(mode)) {
        int32_t on = clamp((age - (mode == VM_RAIN ? VL_BPS * 15 / 100 : VL_BPS * 35 / 100)) * 32767 / (VL_BPS * 6 / 10),
                           0, 32767);
        uint32_t vp = (uint32_t)age * VL_BLK_INC(5.4) + vi * 0x28000000u;
        vib += (((VL_NDEP[mode - VM_BRASS] * on) >> 15) * sine_i(vp)) >> 15;
        if (mode == VM_MEMORY)                          /* the tape that wavers */
            vib += (VL_Q20(0.0035) * sine_i(pp->pwm * 2u)) >> 15;
    } else if (mode == VM_FLUTE) {
        int32_t on = clamp((age - VL_BPS * 12 / 100) * 32767 / (VL_BPS * 35 / 100), 0, 32767);
        uint32_t vp = (uint32_t)age * VL_BLK_INC(5.7) + vi * 0x0F5C0000u;
        vib += (((VL_Q20(0.0045) * on) >> 15) * sine_i(vp)) >> 15;
    }
    if (mode == VM_TAPE || mode == VM_FLUTE) {          /* wow and flutter */
        vib += (VL_Q20(0.0026) * sine_i(pp->w1) + VL_Q20(0.0007) * sine_i(pp->w2)) >> 15;
        if (mode == VM_FLUTE)
            vib += (VL_Q20(0.0011) * sine_i(pp->w3)) >> 15;
    }
    if (vib)
        inc1 = (uint32_t)((int64_t)inc1 + (((int64_t)inc1 * vib) >> 20));
    if (mode == VM_FLUTE) {
        vl_flute(t, v, out, n, m, inc1);
        if (age < 0x7FFF0000)
            v->s[6] = age + 1;
        return;
    }
    if (mode == VM_JUNO) {
        inc2 = inc1 >> 1;                                /* OSC 2: the sub-octave */
    } else {
        int32_t d16 = (p[P_E5] - 64) * 3;               /* DTN: 3/16 semitone a step, +-12 semitones */
        inc2 = d16 ? pitch_inc(clamp(m->pitch16 + d16, 0, 2047)) : inc1;
        if (d16 && vib)
            inc2 = (uint32_t)((int64_t)inc2 + (((int64_t)inc2 * vib) >> 20));
    }

    /* Neon Town's brass swell: the filter opens in ~0.1 s, overshoots, settles */
    if (VM_NEON(mode)) {
        int32_t rise = clamp(age * 32767 / (VL_BPS * 11 / 100), 0, 32767);
        int32_t over = age < VL_BPS ? (14745 * (32767 - age * 32767 / VL_BPS)) >> 15 : 0;   /* +0.45, gone in 1 s */
        int32_t sw = ((11469 + ((21299 * rise) >> 15)) * (32767 + over)) >> 15;
        int32_t d = ((sw * 37683) >> 15) - 32767;                               /* swell x 1.15 - 1 */
        cut += (((d * VL_NSWA[mode - VM_BRASS]) >> 8) * VL_CUT_OCT) >> 15;
    }
    tsvf_coef(&f1c, cut, 0);
    tsvf_coef(&f2c, cut, res);

    /* the extra oscillators' increments (once a block) */
    uint32_t ss[4] = {0, 0, 0, 0}, ia = 0, ib = 0;
    if (mode == VM_SUPERSAW) {
        uint32_t k;
        for (k = 0; k < 4; k++)
            ss[k] = vl_scale(inc1, VL_SS[k]);
    } else if (VM_NEON(mode)) {
        ia = vl_scale(inc1, VL_NDA[mode - VM_BRASS]);
        ib = vl_scale(inc1, VL_NDB[mode - VM_BRASS]);
    }

    /* winter: the FM voice's settings from the same knobs (cutoff -> index, resonance -> feedback, waves -> ratios) */
    int32_t fm_idx = 0, fm_fb = 0, fm_i2 = 0, fm_lp = 0;
    uint32_t fi1 = 0, fi2 = 0;
    if (win > 16) {
        int32_t cn = clamp(cut, 0, 127 << 8) >> 7;     /* 0..254: cutoff 0..1 */
        fm_idx = (((cn * cn) >> 2) * (32767 - ((night * 20316) >> 15))) >> 15; /* cut^2 (Q14 cycles), softer at night */
        fm_idx = (fm_idx * (9830 + ((((v->env >> 9) * 22937)) >> 15))) >> 15;  /* x (0.3 + 0.7 env) */
        if (inc1 > 146100000u)                           /* above ~1.5 kHz the index falls (sqrt(1500 / f)) */
            fm_idx = (fm_idx * (int32_t)(146100000u / (inc1 >> 15))) >> 15;
        fm_fb = (res * (32767 - ((night * 27852) >> 15))) >> 15;               /* 0..127: 0..0.06 cycles */
        fm_i2 = 3932 - ((night * 3146) >> 15);                                  /* 0.12, less at night */
        fi1 = (uint32_t)(((uint64_t)inc1 * VL_FMR[(p[P_E1] * 7) >> 7]) >> 8);
        fi2 = (uint32_t)(((uint64_t)inc2 * VL_FMR[(clamp(w2 + ((night * 10486) >> 15), 0, 32767) * 7) >> 15]) >> 8);
        fm_lp = clamp(2265 + cn * 55 - ((night * cn * 33) >> 15), 1000, 30000); /* the FM low-pass follows CUT */
    }

    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], pm1 = v->ph[2];
    int32_t a1 = v->s[0], a2 = v->s[1], b1 = v->s[2], b2 = v->s[3], flp = v->s[4], fbk = v->s[5];
    uint32_t pwa = (uint32_t)(16384 + ((sine_i(pp->pwm + vi * 0x1C000000u) * 11796) >> 15)) << 17;            /* 0.5 +- 0.36 */
    uint32_t pwb = (uint32_t)(16384 + ((sine_i(pp->pwm + (pp->pwm >> 1) + 0x40000000u + vi * 0x36000000u) * 13107) >> 15)) << 17;
    int32_t nz = pp->nz ? (int32_t)pp->nz : 0x2545F491;
    int32_t tpk = mode == VM_MEMORY ? 11400 : 17138;   /* 3.0 / 5.2 kHz */
    for (i = 0; i < n; i++) {
        int32_t s;
        switch (mode) {
        case VM_JUNO: {
            int32_t saw = osc_saw(ph0, inc1), pul = osc_pulse(ph0, inc1, pwa) * 2;
            int32_t k = (w1 * 22937) >> 15;              /* 0.7 x WAVE 1 */
            s = ((((saw * (32767 - k)) >> 15) + ((pul * (9830 + k)) >> 15)) * 19661 >> 15)
                + ((osc_pulse(ph1, inc2, 0x80000000u) * 2 * 11141) >> 15);
            break;
        }
        case VM_SUPERSAW: {
            int32_t acc = osc_saw(ph0, inc1) + osc_saw(ph1, inc2)
                          + osc_saw(x->ph[0], ss[0]) + osc_saw(x->ph[1], ss[1])
                          + ((3 * (osc_saw(x->ph[2], ss[2]) + osc_saw(x->ph[3], ss[3]))) >> 2);
            x->ph[0] += ss[0]; x->ph[1] += ss[1]; x->ph[2] += ss[2]; x->ph[3] += ss[3];
            s = (acc * 7864) >> 15;                      /* x 0.24 */
            break;
        }
        case VM_PWM:
            s = (((osc_pulse(ph0, inc1, pwa) + osc_pulse(ph1, inc2, pwb)) * 2) * 13763) >> 15;
            break;
        case VM_BRASS: case VM_STRINGS: case VM_MEMORY:   /* three saws and a little square */
            s = (((osc_saw(ph0, inc1) + ((osc_saw(x->ph[0], ia) * 27852) >> 15) + ((osc_saw(x->ph[1], ib) * 27852) >> 15))
                  * 11141) >> 15) + ((osc_pulse(ph1, inc2, 0x80000000u) * 2 * 3932) >> 15);
            x->ph[0] += ia; x->ph[1] += ib;
            break;
        case VM_CHOIR:
            s = (((sine_i(ph0) + sine_i(x->ph[0]) + sine_i(x->ph[1])) * 9830) >> 15) + ((osc_saw(ph0, inc1) * 5898) >> 15);
            x->ph[0] += ia; x->ph[1] += ib;
            break;
        case VM_RAIN:
            s = ((osc_saw(ph0, inc1) * 31129) >> 15) + ((osc_pulse(ph1, inc2, 0x80000000u) * 2 * 15729) >> 15);
            break;
        case VM_DEEP:
            s = ((osc_saw(ph0, inc1) * 13107) >> 15) + ((sine_i(x->ph[1]) * 21299) >> 15);
            x->ph[1] += ib;
            break;
        case VM_PULSE:
            s = ((osc_pulse(ph0, inc1, 0x40000000u) * 2 * 14090) >> 15) + ((osc_pulse(x->ph[0], ia, 0x40000000u) * 2 * 8847) >> 15);
            x->ph[0] += ia;
            break;
        default:                                         /* MODULAR TAPE FM: the two vending machines */
            s = (vl_blend(ph0, inc1, w1) + vl_blend(ph1, inc2, w2)) >> 1;
            break;
        }
        if (mode == VM_CHOIR) {                          /* two vowel formants, a dark "ah" (650 and 1080 Hz) */
            int32_t h1 = s - x->f[1] - ((x->f[0] * 11469) >> 15);
            x->f[0] += (3032 * h1) >> 15;
            x->f[1] += (3032 * x->f[0]) >> 15;
            int32_t h2 = s - x->f[3] - ((x->f[2] * 13107) >> 15);
            x->f[2] += (5034 * h2) >> 15;
            x->f[3] += (5034 * x->f[2]) >> 15;
            s += ((((x->f[0] * 47514) >> 15) + ((x->f[2] * 31130) >> 15) - s) * 22938) >> 15;
        }
        /* the filter: a soft input, then two low-passes (the resonance on the second) */
        s = softclip(s);
        s = tsvf_lp(&f1c, s >> 1, &a1, &a2);
        s = soft_knee(tsvf_lp(&f2c, s, &b1, &b2), 16000) << 1;

        if (win > 16) {                                  /* winter: carrier + modulator 1 (feedback) + modulator 2 */
            int32_t mo1 = sine_i(pm1 + (uint32_t)(fm_fb * fbk) * 62u);
            fbk = (fbk + mo1) >> 1;
            int32_t mo2 = sine_i(x->ph[4]);
            uint32_t off = (uint32_t)(fm_idx * mo1) * 6u + (uint32_t)(((fm_idx * fm_i2) >> 15) * mo2) * 6u;
            int32_t car = sine_i(ph0 + off);
            flp += ((car - flp) * fm_lp) >> 15;
            s += (((((flp * 17039) >> 15) - s) * win) >> 15);   /* the FM at 0.52 */
            pm1 += fi1;
            x->ph[4] += fi2;
        }
        if (mode == VM_TAPE || mode == VM_MEMORY) {   /* a tired tape: saturation, dull highs */
            int32_t sat = (softclip((s * 7) >> 2) * 20316) >> 15;          /* tanh(1.7 x) x 0.62 */
            x->tp += ((sat - x->tp) * tpk) >> 15;
            s = x->tp + (((int32_t)(noise32(&nz) >> 22) - 512) / 6);
        }
        ph0 += inc1;
        ph1 += inc2;
        out[i] += voice_amp(s, m, i) << 1;
    }
    pp->nz = (uint32_t)nz;
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = pm1;
    v->s[0] = a1; v->s[1] = a2; v->s[2] = b1; v->s[3] = b2;
    v->s[4] = flp; v->s[5] = fbk;
    if (age < 0x7FFF0000)
        v->s[6] = age + 1;
    }
}

/* the 14 sounds (the pads of the SMK-37 map load them) */
static const preset_t VILLE_PRESETS[] = {
    /*            MODE         W1  W2  CUT RES DTN NUIT HIVER    ATK DEC SUS REL */
    {"MODULAR",  {VM_MODULAR,   0,  0,  74, 15, 64,  0,  0}, {2, 40, 38, 45}, 0, 0, FX(0, 0, 20, 0), 0},
    {"JUNO",     {VM_JUNO,     40,  0,  70, 20, 64,  0,  0}, {6, 50, 70, 55}, 0, 0, FX(0, 70, 15, 0), 0},
    {"SUPERSAW", {VM_SUPERSAW,  0,  0,  80, 10, 64,  0,  0}, {10, 60, 80, 60}, 0, 0, FX(0, 20, 20, 0), 0},
    {"PWM STR",  {VM_PWM,       0,  0,  68, 10, 64,  0,  0}, {45, 60, 85, 70}, 0, 0, FX(0, 70, 15, 0), 0},
    {"TAPE",     {VM_TAPE,     20, 60,  66, 12, 64,  0,  0}, {8, 55, 60, 55}, 0, 0, FX(0, 0, 25, 0), 0},
    {"FM",       {VM_FM,       20, 45,  70, 20, 64,  0, 127}, {2, 60, 40, 60}, 0, 0, FX(0, 0, 20, 0), 0},
    {"FLUTE",    {VM_FLUTE,     0,  0, 127, 64, 64,  0,  0}, {56, 30, 127, 86}, 0, 0, FX(0, 0, 0, 0), 0},
    {"BRASS",    {VM_BRASS,     0,  0,  62, 12, 64,  0,  0}, {14, 60, 90, 50}, 0, 0, FX(0, 55, 15, 0), 0},
    {"STRINGS",  {VM_STRINGS,   0,  0,  70,  8, 64,  0,  0}, {45, 60, 100, 70}, 0, 0, FX(0, 60, 15, 0), 0},
    {"CHOIR",    {VM_CHOIR,     0,  0,  76,  6, 64,  0,  0}, {40, 60, 100, 70}, 0, 0, FX(0, 55, 15, 0), 0},
    {"RAIN",     {VM_RAIN,      0,  0,  72, 14, 64,  0,  0}, {6, 60, 70, 60}, 0, 0, FX(0, 50, 30, 0), 0},
    {"DEEP",     {VM_DEEP,      0,  0,  58, 12, 64,  0,  0}, {4, 50, 90, 45}, 1, 0, FX(0, 40, 10, 0), 0},
    {"PULSE",    {VM_PULSE,     0,  0,  70, 14, 64,  0,  0}, {2, 45, 60, 45}, 0, 0, FX(0, 45, 25, 0), 0},
    {"MEMORY",   {VM_MEMORY,    0,  0,  64, 10, 64,  0,  0}, {20, 60, 90, 65}, 0, 0, FX(0, 50, 20, 0), 0},
    /* the composer's voice: TIMB 7 (.5:1) = WAVE 1 75, 6 (1:.5) = 64; the FM-1 TOWN presets' brightness and grain */
    {"SOUFFLE",  {VM_SOUFFLE,  75,  0,  40, 60, 64,  0,  0}, {110, 95, 127, 115}, 0, 0, FX(0, 25, 10, 0), 0},
    {"CREPUSC",  {VM_CREPUS,   64,  0,  52, 48, 64,  0,  0}, {106, 95, 122, 118}, 0, 0, FX(0, 25, 20, 0), 0},
};

static const engine_t ENG_VILLE = {
    .name = "VILLE",
    .page_title = {"VILLE", "SAISON"},
    .edit = {
        {"MODE", F_ENUM, 0, VM_N - 1, 0, N_VILLE_MODE, 0},
        {"WAVE 1", F_PCT, 0, 127, 0, 0, 0},
        {"WAVE 2", F_PCT, 0, 127, 0, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 74, 0, 0},
        {"RES", F_PCT, 0, 127, 15, 0, 0},
        {"DTN", F_INT, 0, 127, 64, 0, 0},
        {"NUIT", F_PCT, 0, 127, 0, 0, 0},
        {"HIVER", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = VILLE_PRESETS,
    .npresets = (uint8_t)NELEM(VILLE_PRESETS),
    .knob = {3, 4, 1, 6},
    .keep = 0x0Fu,
    .note_on = vl_note_on,
    .render = vl_render,
    .block = vl_block,
    .mem = sizeof(vl_part_t),
};
