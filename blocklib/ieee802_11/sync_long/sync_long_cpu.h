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

class sync_long_cpu : public virtual sync_long
{
public:
    sync_long_cpu(block_args args);
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


    int d_count;

    int d_frame_start;
    float d_freq_offset;
    double d_freq_offset_short;

    gr_complex* d_correlation;
    list<pair<gr_complex, int>> d_cor;
    std::vector<gr::tag_t> d_tags;
    gr::kernel::filter::fir_filter_ccc d_fir;

    const bool d_log;
    const bool d_debug;
    size_t d_offset;
    enum { SYNC, COPY, RESET } d_state;
    const size_t SYNC_LENGTH;

    static const std::vector<gr_complex> LONG;
};

} // namespace ieee802_11
} // namespace gr