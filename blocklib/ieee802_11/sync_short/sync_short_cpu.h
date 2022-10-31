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

#include <gnuradio/ieee802_11/sync_short.h>

namespace gr {
namespace ieee802_11 {

class sync_short_cpu : public virtual sync_short
{
public:
    sync_short_cpu(block_args args);
    work_return_t work(work_io& wio) override;

private:
    const bool d_log;
    const bool d_debug;
    const unsigned int d_min_plateau;
    const double d_threshold;

    enum { SEARCH, COPY } d_state;
    int d_copied;

    unsigned int d_plateau;
    float d_freq_offset;



    void insert_tag(uint64_t item,
                    double freq_offset,
                    uint64_t input_item,
                    block_work_output& work_output)
    {
        work_output.add_tag(
            item, pmtf::map({ { "wifi_start", freq_offset }, { "srcid", name() } }));
    }
};

} // namespace ieee802_11
} // namespace gr