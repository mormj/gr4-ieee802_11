/* -*- c++ -*- */
/*
 * Copyright 2022 Block Author
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#pragma once

#include <gnuradio/ieee802_11/sync_long.h>
#include <gnuradio/kernel/fft/fftw_fft.h>
#include <gnuradio/kernel/filter/fir_filter.h>
#include <volk/volk.h>
#include <list>
#include <tuple>
#include <vector>

using namespace std;

bool compare_abs(const std::pair<gr_complex, int>& first,
                 const std::pair<gr_complex, int>& second)
{
    return abs(get<0>(first)) > abs(get<0>(second));
}

namespace gr {
namespace ieee802_11 {

class sync_long_opt : public virtual sync_long
{
public:
    sync_long_opt(block_args args);
    work_return_t work(work_io& wio) override;

private:
    void search_frame_start()
    {

        // sort list (highest correlation first)
        assert(d_cor.size() == SYNC_LENGTH);
        d_cor.sort(compare_abs);

        // copy list in vector for nicer access
        vector<pair<gr_complex, int>> vec(d_cor.begin(), d_cor.end());
        d_cor.clear();

        // in case we don't find anything use SYNC_LENGTH
        d_frame_start = SYNC_LENGTH;

        for (int i = 0; i < 3; i++) {
            for (int k = i + 1; k < 4; k++) {
                gr_complex first;
                gr_complex second;
                if (get<1>(vec[i]) > get<1>(vec[k])) {
                    first = get<0>(vec[k]);
                    second = get<0>(vec[i]);
                } else {
                    first = get<0>(vec[i]);
                    second = get<0>(vec[k]);
                }
                int diff = abs(get<1>(vec[i]) - get<1>(vec[k]));
                if (diff == 64) {
                    d_frame_start = min(get<1>(vec[i]), get<1>(vec[k]));
                    d_freq_offset = arg(first * conj(second)) / 64;
                    // nice match found, return immediately
                    return;

                } else if (diff == 63) {
                    d_frame_start = min(get<1>(vec[i]), get<1>(vec[k]));
                    d_freq_offset = arg(first * conj(second)) / 63;
                } else if (diff == 65) {
                    d_frame_start = min(get<1>(vec[i]), get<1>(vec[k]));
                    d_freq_offset = arg(first * conj(second)) / 65;
                }
            }
        }
    }


    int d_block_size;
    int d_min_grid_size;
    cudaStream_t d_stream;

    enum { WAITING_FOR_TAG, FINISH_LAST_FRAME } d_state = WAITING_FOR_TAG;
    unsigned int d_sync_length;
    static const std::vector<gr_complex> LONG;
    int d_fftsize = 512;

    cufftHandle d_plan;
    cufftComplex* d_dev_training_freq;
    cufftComplex* d_dev_in;

    std::vector<gr::tag_t> tags;

    int d_ncopied = 0;
    float d_freq_offset = 0;
    float d_freq_offset_short = 0;

    int d_num_syms = 0;
    int ntags = 0;

    size_t packet_cnt = 0;

    int d_offset = 0;
};

} // namespace ieee802_11
} // namespace gr