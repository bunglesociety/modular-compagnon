/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* SMK-37: MIDI OUT on the TRS jack (hal/fm1_uart.h, UART1 TX on PC7). Everything that goes out by USB
 * (usb.c midi_out_event: notes, the wheels, CC, clock) goes out here too, as plain MIDI bytes, with or without
 * a USB host. The writers are the audio ISR and the main loop with IRQs off (as usb.c's queue); the TIMER5 ISR
 * sends a byte whenever the UART is free (320 us a byte at 31250 baud), outside the audio. A full ring drops the
 * message whole. */
#define TRS_Q 256u
static uint8_t trs_q[TRS_Q];
static volatile uint32_t trs_w, trs_r;

static void trs_out_init(void)
{
    fm1_uart1_tx_init();
}

/* a USB-MIDI event packet (byte 0: CIN, bytes 1..3: the message) */
static void trs_out_event(uint32_t pkt)
{
    static const uint8_t LEN[16] = {0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1};
    uint32_t n = LEN[pkt & 15u], i, w = trs_w;
    if (!n || TRS_Q - (w - trs_r) < n)
        return;
    for (i = 0; i < n; i++)
        trs_q[(w + i) % TRS_Q] = (uint8_t)(pkt >> (8u + 8u * i));
    RING_PUBLISH();
    trs_w = w + n;
}

static void trs_out_poll(void)                      /* TIMER5 ISR, not inside the audio */
{
    if (trs_r != trs_w && fm1_uart1_tx_ready()) {
        fm1_uart1_tx_put(trs_q[trs_r % TRS_Q]);
        trs_r++;
    }
}
