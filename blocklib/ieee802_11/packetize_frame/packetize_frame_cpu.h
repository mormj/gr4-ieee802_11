/* -*- c++ -*- */
/*
 * Copyright 2022 Josh Morman
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#pragma once

#include "equalizer/base.h"
#include "viterbi_decoder/viterbi_decoder.h"
#include <gnuradio/ieee802_11/constellations.h>
#include <gnuradio/ieee802_11/enums.h>
#include <gnuradio/ieee802_11/packetize_frame.h>
#include <mutex>
namespace gr {
namespace ieee802_11 {

class packetize_frame_cpu : public virtual packetize_frame
{
public:
    packetize_frame_cpu(block_args args);
    virtual work_return_t
    work(work_io& wio) override;

private:
    enum { WAITING_FOR_TAG, FINISH_LAST_FRAME } d_state = WAITING_FOR_TAG;
    void set_algorithm(algorithm_t algo);
    bool parse_signal(uint8_t* signal);
    bool decode_signal_field(uint8_t* rx_bits);
    void deinterleave(uint8_t* rx_bits);

    equalizer::base* d_equalizer = nullptr;
    std::mutex d_mutex;
    std::vector<gr::tag_t> tags;
    bool d_log;
    bool d_debug;
    int d_current_symbol = 0;
    viterbi_decoder d_decoder;

    // freq offset
    double d_freq;                            // Hz
    double d_freq_offset_from_synclong = 0.0; // Hz, estimation from "sync_long" block
    double d_bw;                              // Hz
    double d_er;
    double d_epsilon0;
    gr_complex d_prev_pilots[4];

    int d_frame_bytes = 0;
    int d_frame_symbols = 0;
    int d_frame_encoding;

    uint8_t d_deinterleaved[48];
    gr_complex symbols[48];

    std::shared_ptr<gr::kernel::digital::constellation> d_frame_mod;
    constellation_bpsk::sptr d_bpsk;
    constellation_qpsk::sptr d_qpsk;
    constellation_16qam::sptr d_16qam;
    constellation_64qam::sptr d_64qam;

    static const int interleaver_pattern[48];

    size_t ntags_rx = 0;
    size_t ntags_tx = 0;

    int packet_cnt = 0;
    pmtf::pmt d_pdu;

    std::vector<gr_complex> samples_buf;
    bool new_packet = false;

    void process_symbol(gr_complex* in, gr_complex* symbols, uint8_t* out);
};

} // namespace wifi
} // namespace gr