/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* VILLE (Modular One on the SMK-37): every sound to a WAV, health numbers, and its cost against ANALOG.
 *   cc -O2 -w -DMODULAR_ONE=1 -DFELUCCA_SLICE=0 -Ibuild/gen -Ifirmware/src -o build/host/ville_demo tests/ville_demo.c -lm
 *   build/host/ville_demo OUTDIR */
#define main hostsim_main
#include "hostsim.c"
#undef main

static const uint8_t CHORD[4] = {48, 55, 60, 64};
static const uint8_t MEL[8] = {67, 69, 72, 71, 67, 64, 62, 60};

static int render(const char *dir, const char *name, uint32_t pi, int night, int winter, double *peak_db)
{
    char path[512];
    const uint32_t frames = 6u * FS, B = FS / 4u / CTL * CTL;
    FILE *w;
    uint32_t f, i;
    int32_t pk = 0;
    track_t *t = &trk[0];
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    if (!(w = fopen(path, "wb")))
        return 1;
    host_tracks_init();
    host_preset(t, ENGI_VILLE, pi);
    if (night >= 0) t->p[P_E6] = (int16_t)night;
    if (winter >= 0) t->p[P_E7] = (int16_t)winter;
    t->p[P_LEVEL] = 100;
    wav_hdr(w, frames);
    for (f = 0; f < frames; f += CTL) {
        int32_t o[2 * CTL];
        if (f == 0) for (i = 0; i < 4; i++) trk_note_on(t, CHORD[i], 90);
        if (f == 8u * B) for (i = 0; i < 4; i++) trk_note_off(t, CHORD[i]);
        for (i = 0; i < 8; i++) {
            if (f == (9u + i) * B) trk_note_on(t, MEL[i], (uint8_t)(70 + i * 7));
            if (f == (10u + i) * B - CTL) trk_note_off(t, MEL[i]);
        }
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t a = abs(o[2 * i]) > abs(o[2 * i + 1]) ? abs(o[2 * i]) : abs(o[2 * i + 1]);
            pk = a > pk ? a : pk;
            wav_put(w, o[2 * i], o[2 * i + 1]);
        }
    }
    fclose(w);
    *peak_db = 20.0 * log10((pk + 1) / 32768.0);
    return 0;
}

/* ns per sample with 8 notes held (one part), after 0.2 s of warm-up */
static double cost(uint32_t e, uint32_t pi)
{
    track_t *t = &trk[0];
    uint32_t f, i;
    uint64_t t0, t1;
    host_tracks_init();
    host_preset(t, e, pi);
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_VOICE] = V_POLY;
    for (i = 0; i < 8; i++) trk_note_on(t, (uint8_t)(48 + i * 3), 100);
    for (f = 0; f < FS / 5u; f += CTL) { int32_t o[2 * CTL]; mix_block(o, CTL); }
    t0 = now_ns();
    for (f = 0; f < 4u * FS; f += CTL) { int32_t o[2 * CTL]; mix_block(o, CTL); }
    t1 = now_ns();
    return (double)(t1 - t0) / (4.0 * FS);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/ville_demo";
    uint32_t pi, n = ENG_VILLE.npresets;
    double base, worst = 0, db;
    int bad = 0;
    char name[64];
    for (pi = 0; pi < n; pi++) {
        snprintf(name, sizeof name, "%02u_%s", pi + 1, ENG_VILLE.presets[pi].name);
        for (char *c = name; *c; c++) if (*c == ' ') *c = '_';
        bad |= render(dir, name, pi, -1, -1, &db);
        printf("ville: %-12s peak %6.1f dBFS%s\n", ENG_VILLE.presets[pi].name, db, db > -0.5 ? "  CLIP?" : db < -40 ? "  SILENT?" : "");
        bad |= db < -40;
    }
    bad |= render(dir, "night_MODULAR", 0, 127, -1, &db); printf("ville: MODULAR at night peak %.1f dBFS\n", db);
    bad |= render(dir, "winter_MODULAR", 0, -1, 127, &db); printf("ville: MODULAR in winter peak %.1f dBFS\n", db);
    bad |= render(dir, "winter_night_BRASS", 7, 127, 100, &db); printf("ville: BRASS night + winter peak %.1f dBFS\n", db);
    bad |= render(dir, "montee_SOUFFLE", 14, -1, 127, &db); printf("ville: SOUFFLE fader 2 up peak %.1f dBFS\n", db);
    bad |= render(dir, "montee_CREPUSC", 15, -1, 127, &db); printf("ville: CREPUSC fader 2 up peak %.1f dBFS\n", db);
    m1_vintage = 1;                                                          /* FX: the vintage (fx.c m1_vint) */
    bad |= render(dir, "vintage_MODULAR", 0, -1, -1, &db); printf("ville: MODULAR vintage peak %.1f dBFS\n", db);
    bad |= render(dir, "vintage_FLUTE", 6, -1, -1, &db); printf("ville: FLUTE vintage peak %.1f dBFS\n", db);
    bad |= render(dir, "vintage_JUNO", 1, -1, -1, &db); printf("ville: JUNO vintage peak %.1f dBFS\n", db);
    bad |= render(dir, "vintage_STRINGS", 8, -1, -1, &db); printf("ville: STRINGS vintage peak %.1f dBFS\n", db);
    m1_vintage = 0;
    base = cost(0, 0);
    printf("cost: ANALOG preset 1, 8 notes: %.1f ns / sample\n", base);
    for (pi = 0; pi < n; pi++) {
        double c = cost(ENGI_VILLE, pi);
        worst = c > worst ? c : worst;
        printf("cost: VILLE %-10s 8 notes: %6.1f ns / sample (%.2f x ANALOG)\n", ENG_VILLE.presets[pi].name, c, c / base);
    }
    printf("ville_demo: %s\n", bad ? "FAIL" : "ok");
    return bad;
}
