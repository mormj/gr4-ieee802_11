/* -*- c++ -*- */
/*
 * Copyright 2022 Josh Morman
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "packetize_frame_cpu.h"
#include "packetize_frame_cpu_gen.h"

#include "equalizer/base.h"
#include "equalizer/comb.h"
#include "equalizer/lms.h"
#include "equalizer/ls.h"
#include "equalizer/sta.h"

#include "utils.h"

#include <pmtf/map.hpp>


namespace gr {
namespace ieee802_11 {

packetize_frame_cpu::packetize_frame_cpu(block_args args)
    : INHERITED_CONSTRUCTORS,
      d_log(args.log),
      d_debug(args.debug),
      d_freq(args.freq),
      d_bw(args.bw)
{

    d_bpsk = constellation_bpsk::make();
    d_qpsk = constellation_qpsk::make();
    d_16qam = constellation_16qam::make();
    d_64qam = constellation_64qam::make();

    d_frame_mod = d_bpsk;

    set_tag_propagation_policy(tag_propagation_policy_t::TPP_DONT);
    set_algorithm(args.algo);
}


void packetize_frame_cpu::set_algorithm(algorithm_t algo)
{

    delete d_equalizer;

    switch (algo) {

    case algorithm_t::COMB:
        dout << "Comb" << std::endl;
        d_equalizer = new equalizer::comb();
        break;
    case algorithm_t::LS:
        dout << "LS" << std::endl;
        d_equalizer = new equalizer::ls();
        break;
    case algorithm_t::LMS:
        dout << "LMS" << std::endl;
        d_equalizer = new equalizer::lms();
        break;
    case algorithm_t::STA:
        dout << "STA" << std::endl;
        d_equalizer = new equalizer::sta();
        break;
    default:
        throw std::runtime_error("Algorithm not implemented");
    }
}
work_return_t
packetize_frame_cpu::work(work_io& wio)
{
    auto in = wio.inputs()[0].items<gr_complex>();

    auto ninput = wio.inputs()[0].n_items;
    auto nread = wio.inputs()[0].nitems_read();

    int symbols_to_consume = ninput;

    tags = wio.inputs()[0].tags_in_window(0, ninput);

    // std::cout << "PACKETIZE_FRAME : nread " << nread      << " tags " << tags.size() <<
    // std::endl; std::cout << "   packetize raw tags: " <<
    // work_input[0].buffer->tags().size() << std::endl;

    // std::cout << " -- ninput " << ninput << std::endl;;
    int nconsumed = 0;
    size_t tag_idx = 0;
    while (nconsumed < ninput) {
        if (d_state == FINISH_LAST_FRAME) {
            auto max_consume = ninput - nconsumed;
            if (tag_idx < tags.size()) {
                // only consume up to the next tag
                max_consume = tags[tag_idx].offset() - (nread + nconsumed);
                if (max_consume <= 0) {

                    nconsumed = tags[tag_idx].offset() - nread;
                    // std::cout << " -- nconsumed (1) " << nconsumed << std::endl;;
                    d_state = WAITING_FOR_TAG;
                    continue;
                }
            }


            if (d_frame_symbols + 2 > d_current_symbol &&
                d_frame_symbols + 2 - d_current_symbol < symbols_to_consume) {
                symbols_to_consume = d_frame_symbols + 2 - d_current_symbol;
            }

            if (symbols_to_consume) {
                auto symbols_to_process = symbols_to_consume;
                if (symbols_to_process > (d_frame_symbols + 2)) {
                    symbols_to_process = d_frame_symbols + 2;
                }

                if (nconsumed + symbols_to_process > ninput) {
                    symbols_to_process = ninput - nconsumed;
                }

                for (int o = 0; o < symbols_to_process; o++) {
                    if (d_current_symbol > d_frame_symbols + 2) {
                        d_state = WAITING_FOR_TAG;
                        continue;
                    }

                    memcpy(samples_buf.data() + 64 * (d_current_symbol - 3),
                           in + nconsumed * 64,
                           64 * sizeof(gr_complex));

                    d_current_symbol++;
                    nconsumed++;
                }
                // std::cout << " -- nconsumed (2) " << nconsumed << std::endl;;
            }
        } else {

            if (tag_idx < tags.size()) {
                // new frame -- send out the old frame
                if (new_packet) {
                    auto samples = pmtf::vector<gr_complex>(
                        samples_buf.data(), samples_buf.data() + 64 * d_frame_symbols);

                    auto d = pmtf::map({
                        { "frame_bytes", d_frame_bytes },
                        { "frame_symbols", d_frame_symbols },
                        { "encoding", d_frame_encoding },
                        { "snr", d_equalizer->get_snr() },
                        { "freq", d_freq },
                        { "bw", d_bw },
                        { "freq_offset", d_freq_offset_from_synclong },
                        { "H",
                          pmtf::vector<gr_complex>(d_equalizer->get_H(),
                                                   d_equalizer->get_H() + 64) },
                        { "prev_pilots",
                          pmtf::vector<gr_complex>(d_prev_pilots, d_prev_pilots + 4) },
                        { "packet_cnt", packet_cnt },
                    });

                    auto pdu =
                        pmtf::map({ { "data", samples }, { "meta", d } });

                    get_message_port("pdus")->post(pdu);
                    packet_cnt++;

                    if (packet_cnt % 1000 == 0) {
                        std::cout << "packetize_frame: " << packet_cnt << std::endl;
                    }

                    new_packet = false;
                }

                d_current_symbol = 0;
                d_frame_symbols = 0;
                d_frame_mod = d_bpsk;

                d_freq_offset_from_synclong =
                    pmtf::get_as<double>(tags[tag_idx]["wifi_start"]) * d_bw / (2 * M_PI);
                d_epsilon0 = pmtf::get_as<double>(tags[tag_idx]["wifi_start"]) * d_bw /
                             (2 * M_PI * d_freq);
                d_er = 0;

                auto frame_start = tags[tag_idx].offset() - nread;

                if ((int)frame_start + 3 >= ninput) {
                    nconsumed = frame_start;
                    // std::cout << " -- nconsumed (3) " << nconsumed << std::endl;;
                    break;
                }

                static uint8_t signal_field[48];
                static gr_complex symbols[64];

                for (int i = 0; i < 3; i++) {
                    process_symbol((gr_complex*)(in + (frame_start + i) * 64),
                                   symbols,
                                   signal_field);
                    d_current_symbol++;
                }

                if (decode_signal_field(signal_field)) {

                    // check for maximum frame size
                    if (d_frame_symbols > MAX_SYM || d_frame_bytes > MAX_PSDU_SIZE) {
                        nconsumed = frame_start + 3;
                        d_state = WAITING_FOR_TAG;
                        tag_idx++;
                        // std::cout << " -- nconsumed (3) " << nconsumed << std::endl;;
                        continue;
                    }

                    if ((int)samples_buf.size() < d_frame_symbols * 64) {
                        samples_buf.resize(d_frame_symbols * 64);
                    }
                    new_packet = true;


                    d_state = FINISH_LAST_FRAME;

                    nconsumed = frame_start + 3;
                    // std::cout << " -- nconsumed (4) " << nconsumed << std::endl;;
                } else {
                    nconsumed = frame_start + 3;
                    // produce_each(0, work_output);
                    d_state = WAITING_FOR_TAG;
                    // std::cout << " -- nconsumed (5) " << nconsumed << std::endl;;
                }


            } else {
                nconsumed = ninput;
                break;
            }

            tag_idx++;
        }
    }

    // std::cout << "   packetize ninput/nconsumed: " << ninput << " / " << nconsumed <<
    // std::endl;

    wio.consume_each(nconsumed);
   return work_return_t::OK;
}

bool packetize_frame_cpu::decode_signal_field(uint8_t* rx_bits)
{

    static ofdm_param ofdm(BPSK_1_2);
    static frame_param frame(ofdm, 0);

    deinterleave(rx_bits);
    uint8_t* decoded_bits = d_decoder.decode(&ofdm, &frame, d_deinterleaved);

    return parse_signal(decoded_bits);
}

void packetize_frame_cpu::deinterleave(uint8_t* rx_bits)
{
    for (int i = 0; i < 48; i++) {
        d_deinterleaved[i] = rx_bits[interleaver_pattern[i]];
    }
}

bool packetize_frame_cpu::parse_signal(uint8_t* decoded_bits)
{

    int r = 0;
    d_frame_bytes = 0;
    bool parity = false;
    for (int i = 0; i < 17; i++) {
        parity ^= decoded_bits[i];

        if ((i < 4) && decoded_bits[i]) {
            r = r | (1 << i);
        }

        if (decoded_bits[i] && (i > 4) && (i < 17)) {
            d_frame_bytes = d_frame_bytes | (1 << (i - 5));
        }
    }

    if (parity != decoded_bits[17]) {
        dout << "SIGNAL: wrong parity" << std::endl;
        return false;
    }

    switch (r) {
    case 11:
        d_frame_encoding = 0;
        d_frame_symbols = (int)ceil((16 + 8 * d_frame_bytes + 6) / (double)24);
        d_frame_mod = d_bpsk;
        dout << "Encoding: 3 Mbit/s   ";
        break;
    case 15:
        d_frame_encoding = 1;
        d_frame_symbols = (int)ceil((16 + 8 * d_frame_bytes + 6) / (double)36);
        d_frame_mod = d_bpsk;
        dout << "Encoding: 4.5 Mbit/s   ";
        break;
    case 10:
        d_frame_encoding = 2;
        d_frame_symbols = (int)ceil((16 + 8 * d_frame_bytes + 6) / (double)48);
        d_frame_mod = d_qpsk;
        dout << "Encoding: 6 Mbit/s   ";
        break;
    case 14:
        d_frame_encoding = 3;
        d_frame_symbols = (int)ceil((16 + 8 * d_frame_bytes + 6) / (double)72);
        d_frame_mod = d_qpsk;
        dout << "Encoding: 9 Mbit/s   ";
        break;
    case 9:
        d_frame_encoding = 4;
        d_frame_symbols = (int)ceil((16 + 8 * d_frame_bytes + 6) / (double)96);
        d_frame_mod = d_16qam;
        dout << "Encoding: 12 Mbit/s   ";
        break;
    case 13:
        d_frame_encoding = 5;
        d_frame_symbols = (int)ceil((16 + 8 * d_frame_bytes + 6) / (double)144);
        d_frame_mod = d_16qam;
        dout << "Encoding: 18 Mbit/s   ";
        break;
    case 8:
        d_frame_encoding = 6;
        d_frame_symbols = (int)ceil((16 + 8 * d_frame_bytes + 6) / (double)192);
        d_frame_mod = d_64qam;
        dout << "Encoding: 24 Mbit/s   ";
        break;
    case 12:
        d_frame_encoding = 7;
        d_frame_symbols = (int)ceil((16 + 8 * d_frame_bytes + 6) / (double)216);
        d_frame_mod = d_64qam;
        dout << "Encoding: 27 Mbit/s   ";
        break;
    default:
        dout << "unknown encoding" << std::endl;
        return false;
    }

    // mylog(boost::format("encoding: %1% - length: %2% - symbols: %3%")
    // % d_frame_encoding % d_frame_bytes % d_frame_symbols);
    return true;
}

const int packetize_frame_cpu::interleaver_pattern[48] = {
    0, 3, 6, 9,  12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45,
    1, 4, 7, 10, 13, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46,
    2, 5, 8, 11, 14, 17, 20, 23, 26, 29, 32, 35, 38, 41, 44, 47
};

void packetize_frame_cpu::process_symbol(gr_complex* in,
                                         gr_complex* symbols,
                                         uint8_t* out)
{
    static gr_complex current_symbol[64];

    std::memcpy(current_symbol, in, 64 * sizeof(gr_complex));

    // compensate sampling offset
    for (int i = 0; i < 64; i++) {
        current_symbol[i] *= exp(gr_complex(
            0, 2 * M_PI * d_current_symbol * 80 * (d_epsilon0 + d_er) * (i - 32) / 64));
    }

    gr_complex p = equalizer::base::POLARITY[(d_current_symbol - 2) % 127];

    double beta;
    if (d_current_symbol < 2) {
        beta = arg(current_symbol[11] - current_symbol[25] + current_symbol[39] +
                   current_symbol[53]);

    } else {
        beta = arg((current_symbol[11] * p) + (current_symbol[39] * p) +
                   (current_symbol[25] * p) + (current_symbol[53] * -p));
    }

    double er = arg((conj(d_prev_pilots[0]) * current_symbol[11] * p) +
                    (conj(d_prev_pilots[1]) * current_symbol[25] * p) +
                    (conj(d_prev_pilots[2]) * current_symbol[39] * p) +
                    (conj(d_prev_pilots[3]) * current_symbol[53] * -p));

    er *= d_bw / (2 * M_PI * d_freq * 80);

    if (d_current_symbol < 2) {
        d_prev_pilots[0] = current_symbol[11];
        d_prev_pilots[1] = -current_symbol[25];
        d_prev_pilots[2] = current_symbol[39];
        d_prev_pilots[3] = current_symbol[53];
    } else {
        d_prev_pilots[0] = current_symbol[11] * p;
        d_prev_pilots[1] = current_symbol[25] * p;
        d_prev_pilots[2] = current_symbol[39] * p;
        d_prev_pilots[3] = current_symbol[53] * -p;
    }

    // compensate residual frequency offset
    for (int i = 0; i < 64; i++) {
        current_symbol[i] *= exp(gr_complex(0, -beta));
    }

    // update estimate of residual frequency offset
    if (d_current_symbol >= 2) {

        double alpha = 0.1;
        d_er = (1 - alpha) * d_er + alpha * er;
    }

    // do equalization
    d_equalizer->equalize(current_symbol, d_current_symbol, symbols, out, d_frame_mod);
}

} // namespace wifi
} // namespace gr