/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Compagnon: the pictures of the site (web/compagnon/img), drawn by the firmware's own screen code on the
 * host (tests/m1_screen.c's harness): each scene to OUTDIR/<scene>/fNNNNN.ppm, tools/make_compagnon_img.py makes the
 * GIFs and PNGs.
 *   cc -O1 -w -DMODULAR_ONE=1 -DFELUCCA_SLICE=0 -Ibuild/gen -Ifirmware/src -Itests -o build/host/m1_shots tests/m1_shots.c -lm
 *   build/host/m1_shots OUTDIR */
#define M1_SCREEN_NO_MAIN
#include "m1_screen.c"
#include <sys/stat.h>

static char scene_dir[512];
static void scene(const char *root, const char *name)
{
    snprintf(scene_dir, sizeof scene_dir, "%s/%s", root, name);
    mkdir(scene_dir, 0755);
    dir = scene_dir;
    nframe = 0;
}

int main(int argc, char **argv)
{
    const char *root = argc > 1 ? argv[1] : "build/shots";
    uint32_t k;
    mkdir(root, 0755);
    host_tracks_init();
    fm1_ms = 1000;
    no_shots = 1;
    m1_init();
    no_shots = 0;
    scene(root, "ouverture");                                              /* the opening, whole */
    run(5600);
    scene(root, "toilette");                                               /* the leg-up wash */
    cat_play(CAT_CELLO); cat.quiet_since = fm1_ms; run(9500);
    scene(root, "joue");                                                   /* playing: he listens */
    host_pad = 1; run(300);
    for (k = 0; k < 6u; k++) { host_notes |= 1u << (4u + k * 2u); fm1_in.key_vel[SMK_KEY_F3 + 4u + k * 2u] = 90; run(450); }
    run(600);
    scene(root, "reglage");                                                /* a knob: CUTOFF */
    host_enc[1] = -4; run(150); host_enc[1] = -6; run(150); host_enc[1] = -3; run(900);
    scene(root, "nuit");                                                   /* nightfall, dodo */
    for (k = 0; k < 30u; k++) { fm1_in.fader[0] = (uint8_t)(127 - k * 127 / 29); run(60); }
    run(9000);
    scene(root, "neige");                                                  /* winter by day */
    for (k = 0; k < 20u; k++) { fm1_in.fader[0] = (uint8_t)(k * 127 / 19); run(60); }
    run(2500);
    for (k = 0; k < 20u; k++) { fm1_in.fader[1] = (uint8_t)(k * 120 / 19); run(60); }
    run(5000);
    for (k = 0; k < 20u; k++) { fm1_in.fader[1] = (uint8_t)(120 - k * 120 / 19); run(60); }
    scene(root, "folie");                                                  /* la folie */
    host_pad = 0; run(2600);
    cat_play(CAT_FOLIE); cat.quiet_since = fm1_ms; run(6000);
    no_shots = 1; pet_pick(1); run(1800); no_shots = 0;
    scene(root, "renard");                                                 /* the fox: a mouse in the grass */
    cat_play(CAT_CHASSE); cat.quiet_since = fm1_ms; run(6500);
    no_shots = 1; pet_pick(2); run(1800); no_shots = 0;
    scene(root, "robot");                                                  /* the robot: his head spins, then love */
    cat_play(CAT_QUEUE); cat.quiet_since = fm1_ms; run(3200);
    cat_play(CAT_MLEM); cat.quiet_since = fm1_ms; run(3500);
    no_shots = 1; pet_pick(3); run(1800); no_shots = 0;
    scene(root, "poisson");                                                /* the fish: the screen is his aquarium */
    cat_play(CAT_IDLE); cat.quiet_since = fm1_ms; run(3500);
    cat_play(CAT_TERRITOIRE); cat.quiet_since = fm1_ms; run(4500);
    pet_pick(0); no_shots = 1; run(1600); no_shots = 0;
    scene(root, "arp");                                                    /* ARP held: its mode, its speed */
    host_btn = 1u << 6; host_pressed = host_btn; run(300);
    host_enc[1] = 1; run(500); host_enc[1] = 1; run(700);
    host_enc[2] = -1; run(900);
    host_released = host_btn; host_btn = 0; run(1500);
    scene(root, "globe");                                                  /* GLOBE */
    host_pressed = 1u << 10; run(600); host_enc[6] = 0; run(400);
    host_pressed = 1u << 10; run(300);
    printf("m1_shots: %s\n", root);
    return 0;
}
