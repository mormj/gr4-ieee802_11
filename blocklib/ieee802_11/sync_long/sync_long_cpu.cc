/* -*- c++ -*- */
/*
 * Copyright 2022 Block Author
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "sync_long_cpu.h"
#include "sync_long_cpu_gen.h"

#include "utils.h"

namespace gr {
namespace ieee802_11 {

sync_long_cpu::sync_long_cpu(block_args args)
    : INHERITED_CONSTRUCTORS,
      d_fir(gr::kernel::filter::fir_filter_ccc(LONG)),
      d_log(args.log),
      d_debug(args.debug),
      d_offset(0),
      d_state(SYNC),
      SYNC_LENGTH(args.sync_length)
{
    set_tag_propagation_policy(tag_propagation_policy_t::TPP_DONT);
    d_correlation = (gr_complex*)volk_malloc(sizeof(gr_complex) * 8192, volk_get_alignment());
}

work_return_t sync_long_cpu::work(work_io& wio)
{
    auto in = wio.inputs()[0].items<gr_complex>();
    auto in_delayed = wio.inputs()[1].items<gr_complex>();
    auto out = wio.outputs()[0].items<gr_complex>();

    /* Forecasting */
    size_t ninput = std::min(std::min(wio.inputs()[0].n_items, wio.inputs()[1].n_items), size_t(8192));
    auto noutput = std::min(ninput, wio.outputs()[0].n_items);
    noutput = (noutput / 64) * 64; // output_multiple

    if (noutput < 1)
        return work_return_t::INSUFFICIENT_INPUT_ITEMS;
    /***************/


    // dout << "LONG ninput[0] " << ninput_items[0] << "   ninput[1] " << ninput_items[1]
    //      << "  noutput " << noutput << "   state " << d_state << std::endl;


    const uint64_t nread = wio.inputs()[0].nitems_read();
    d_tags = wio.inputs()[0].tags_in_window(0, ninput);
    // wio.get_tags_in_range(d_tags, 0, nread, nread + ninput);
    if (d_tags.size()) {
        // std::sort(d_tags.begin(), d_tags.end(), gr::tag_t::offset_compare);

        const uint64_t offset = d_tags.front().offset();

        if (offset > nread) {
            ninput = offset - nread;
        } else {
            if (d_offset && (d_state == SYNC)) {
                throw std::runtime_error("wtf");
            }
            if (d_state == COPY) {
                d_state = RESET;
            }
            // d_freq_offset_short = pmt::to_double(d_tags.front().value);
            // d_freq_offset_short = pmtf::get_as<double>(d_tags.front().value);
            d_freq_offset_short = pmtf::get_as<double>(d_tags[0]["wifi_start"]);
        }
    }


    size_t i = 0;
    size_t o = 0;

    switch (d_state) {

    case SYNC:
        d_fir.filterN(d_correlation, in, std::min(SYNC_LENGTH, (size_t)std::max((int)ninput - 63, 0)));

        while (i + 63 < ninput) {

            d_cor.push_back(pair<gr_complex, int>(d_correlation[i], d_offset));

            i++;
            d_offset++;

            if (d_offset == SYNC_LENGTH) {
                search_frame_start();
                // mylog(boost::format("LONG: frame start at %1%") % d_frame_start);
                mylog(fmt::format("LONG: frame start {}", d_frame_start));
                d_offset = 0;
                d_count = 0;
                d_state = COPY;

                break;
            }
        }

        break;

    case COPY:
        while (i < ninput && o < noutput) {

            int rel = d_offset - d_frame_start;

            if (!rel) {
                // add_item_tag(0,
                //              nitems_written(0),
                //              pmt::string_to_symbol("wifi_start"),
                //              pmt::from_double(d_freq_offset_short - d_freq_offset),
                //              pmt::string_to_symbol(name()));
                wio.outputs()[0].add_tag(
                    wio.outputs()[0].nitems_written(),
                    pmtf::map({ { "wifi_start", d_freq_offset_short - d_freq_offset },
                                { "srcid", name() } }));
            }

            if (rel >= 0 && (rel < 128 || ((rel - 128) % 80) > 15)) {
                out[o] = in_delayed[i] * exp(gr_complex(0, d_offset * d_freq_offset));
                o++;
            }

            i++;
            d_offset++;
        }

        break;

    case RESET: {
        while (o < noutput) {
            if (((d_count + o) % 64) == 0) {
                d_offset = 0;
                d_state = SYNC;
                break;
            } else {
                out[o] = 0;
                o++;
            }
        }

        break;
    }
    }

    dout << "produced : " << o << " consumed: " << i << std::endl;

    d_count += o;
    // consume(0, i);
    // consume(1, i);
    wio.consume_each(i);
    wio.produce_each(o);
    // return o;
    return work_return_t::OK;
}


const std::vector<gr_complex> sync_long_cpu::LONG = {
    gr_complex(-0.0455, -1.0679), gr_complex(0.3528, -0.9865),
    gr_complex(0.8594, 0.7348),   gr_complex(0.1874, 0.2475),
    gr_complex(0.5309, -0.7784),  gr_complex(-1.0218, -0.4897),
    gr_complex(-0.3401, -0.9423), gr_complex(0.8657, -0.2298),
    gr_complex(0.4734, 0.0362),   gr_complex(0.0088, -1.0207),
    gr_complex(-1.2142, -0.4205), gr_complex(0.2172, -0.5195),
    gr_complex(0.5207, -0.1326),  gr_complex(-0.1995, 1.4259),
    gr_complex(1.0583, -0.0363),  gr_complex(0.5547, -0.5547),
    gr_complex(0.3277, 0.8728),   gr_complex(-0.5077, 0.3488),
    gr_complex(-1.1650, 0.5789),  gr_complex(0.7297, 0.8197),
    gr_complex(0.6173, 0.1253),   gr_complex(-0.5353, 0.7214),
    gr_complex(-0.5011, -0.1935), gr_complex(-0.3110, -1.3392),
    gr_complex(-1.0818, -0.1470), gr_complex(-1.1300, -0.1820),
    gr_complex(0.6663, -0.6571),  gr_complex(-0.0249, 0.4773),
    gr_complex(-0.8155, 1.0218),  gr_complex(0.8140, 0.9396),
    gr_complex(0.1090, 0.8662),   gr_complex(-1.3868, -0.0000),
    gr_complex(0.1090, -0.8662),  gr_complex(0.8140, -0.9396),
    gr_complex(-0.8155, -1.0218), gr_complex(-0.0249, -0.4773),
    gr_complex(0.6663, 0.6571),   gr_complex(-1.1300, 0.1820),
    gr_complex(-1.0818, 0.1470),  gr_complex(-0.3110, 1.3392),
    gr_complex(-0.5011, 0.1935),  gr_complex(-0.5353, -0.7214),
    gr_complex(0.6173, -0.1253),  gr_complex(0.7297, -0.8197),
    gr_complex(-1.1650, -0.5789), gr_complex(-0.5077, -0.3488),
    gr_complex(0.3277, -0.8728),  gr_complex(0.5547, 0.5547),
    gr_complex(1.0583, 0.0363),   gr_complex(-0.1995, -1.4259),
    gr_complex(0.5207, 0.1326),   gr_complex(0.2172, 0.5195),
    gr_complex(-1.2142, 0.4205),  gr_complex(0.0088, 1.0207),
    gr_complex(0.4734, -0.0362),  gr_complex(0.8657, 0.2298),
    gr_complex(-0.3401, 0.9423),  gr_complex(-1.0218, 0.4897),
    gr_complex(0.5309, 0.7784),   gr_complex(0.1874, -0.2475),
    gr_complex(0.8594, -0.7348),  gr_complex(0.3528, 0.9865),
    gr_complex(-0.0455, 1.0679),  gr_complex(1.3868, -0.0000),
};

} // namespace ieee802_11
} // namespace gr