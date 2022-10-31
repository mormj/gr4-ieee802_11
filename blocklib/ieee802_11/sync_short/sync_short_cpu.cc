/* -*- c++ -*- */
/*
 * Copyright 2022 Block Author
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "sync_short_cpu.h"
#include "sync_short_cpu_gen.h"
#include "utils.h"

namespace gr {
namespace ieee802_11 {

static const int MIN_GAP = 480;
static const int MAX_SAMPLES = 540 * 80;

sync_short_cpu::sync_short_cpu(block_args args)
    : INHERITED_CONSTRUCTORS,
      d_log(args.log),
      d_debug(args.debug),
      d_min_plateau(args.min_plateau),
      d_threshold(args.threshold),
      d_state(SEARCH),
      d_copied(0),
      d_plateau(0),
      d_freq_offset(0)
{

    set_tag_propagation_policy(tag_propagation_policy_t::TPP_DONT);
}

work_return_t sync_short_cpu::work(work_io& wio)
{
    auto in = wio.inputs()[0].items<gr_complex>();
    auto in_abs = wio.inputs()[1].items<gr_complex>();
    auto in_cor = wio.inputs()[2].items<float>();
    auto out = wio.outputs()[0].items<gr_complex>();

    int noutput = wio.outputs()[0].n_items;
    int ninput = std::min(std::min(wio.inputs()[0].n_items, wio.inputs()[1].n_items),
                          wio.inputs()[2].n_items);

    noutput = std::min(ninput, noutput);

    size_t nc = 0;
    size_t np = 0;

    switch (d_state) {

    case SEARCH: {
        int i;

        for (i = 0; i < ninput; i++) {
            if (in_cor[i] > d_threshold) {
                if (d_plateau < d_min_plateau) {
                    d_plateau++;

                } else {
                    d_state = COPY;
                    d_copied = 0;
                    d_freq_offset = arg(in_abs[i]) / 16;
                    d_plateau = 0;
                    insert_tag(wio.outputs()[0].nitems_written(),
                               d_freq_offset,
                               wio.inputs()[0].nitems_read() + i,
                               wio.outputs()[0]);
                    dout << "SHORT Frame!" << std::endl;
                    break;
                }
            } else {
                d_plateau = 0;
            }
        }

        nc = i;
        np = 0;
        break;
    }

    case COPY: {

        int o = 0;
        while (o < ninput && o < noutput && d_copied < MAX_SAMPLES) {
            if (in_cor[o] > d_threshold) {
                if (d_plateau < d_min_plateau) {
                    d_plateau++;

                    // there's another frame
                } else if (d_copied > MIN_GAP) {
                    d_copied = 0;
                    d_plateau = 0;
                    d_freq_offset = arg(in_abs[o]) / 16;
                    insert_tag(wio.outputs()[0].nitems_written() + o,
                               d_freq_offset,
                               wio.inputs()[0].nitems_read() + o,
                               wio.outputs()[0]);
                    dout << "SHORT Frame!" << std::endl;
                    break;
                }

            } else {
                d_plateau = 0;
            }

            out[o] = in[o] * exp(gr_complex(0, -d_freq_offset * d_copied));
            o++;
            d_copied++;
        }

        if (d_copied == MAX_SAMPLES) {
            d_state = SEARCH;
        }

        dout << "SHORT copied " << o << std::endl;

        nc = np = o;
        break;
    }
    }

    wio.consume_each(nc);
    wio.produce_each(np);

    return work_return_t::OK;
}


} // namespace ieee802_11
} // namespace gr