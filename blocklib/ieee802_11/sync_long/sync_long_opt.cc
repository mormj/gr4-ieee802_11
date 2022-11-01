/* -*- c++ -*- */
/*
 * Copyright 2022 Block Author
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "sync_long_opt.h"
#include "sync_long_opt_gen.h"

#include "utils.h"

namespace gr {
namespace ieee802_11 {

sync_long_opt::sync_long_opt(block_args args)
    : INHERITED_CONSTRUCTORS,
      d_fir(gr::kernel::filter::fir_filter_ccc(LONG)),
      d_log(args.log),
      d_debug(args.debug),
      d_offset(0),
      d_state(SYNC),
      SYNC_LENGTH(args.sync_length)
{
    set_tag_propagation_policy(tag_propagation_policy_t::TPP_DONT);
    d_correlation =
        (gr_complex*)volk_malloc(sizeof(gr_complex) * 8192, volk_get_alignment());
    set_output_multiple(64);
    set_relative_rate(80.0 / 64.0);
}

work_return_t sync_long_opt::work(work_io& wio)
{
    // Since this is a decimating block, forecast for noutput
    int ninput = std::min(wio.inputs()[0].n_items, wio.inputs()[1].n_items);
    // auto ninput = wio.inputs()[0].n_items;
    auto noutput = wio.outputs()[0].n_items;

    auto in = wio.inputs()[0].items<gr_complex>();
    auto out = wio.outputs()[0].items<gr_complex>();


    // d_debug_logger->debug("LONG ninput[0] {}  ninput[1] {} noutput {} state {}",
    //                       wio.inputs()[0].n_items,
    //                       work_input[1]->n_items,
    //                       noutput,
    //                       d_state);


    auto nread = wio.inputs()[0].nitems_read();
    auto nwritten = wio.outputs()[0].nitems_written();
    tags = wio.inputs()[0].tags_in_window(0, ninput);

    // std::cout << tags.size() << std::endl;

    int nconsumed = 0;
    int nproduced = 0;

    size_t tag_idx = 0;
    while (true) {
        if (d_state == FINISH_LAST_FRAME) {
            auto max_consume = ninput - nconsumed;
            auto max_produce = noutput - nproduced;
            if (tag_idx < tags.size()) {
                // only consume up to the next tag
                max_consume = tags[tag_idx].offset() - (nread + nconsumed);
                if (max_consume < 80 ||
                    max_produce < 64) { // need an entire OFDM symbol to do anything here
                    nconsumed = tags[tag_idx].offset() - nread;
                    d_state = WAITING_FOR_TAG;
                    continue;
                }
            } else { // no more tags
                if (max_consume < 80 ||
                    max_produce < 64) { // need an entire OFDM symbol to do anything here
                    // nconsumed += max_consume;
                    break;
                }
            }

            auto nsyms = std::min(max_consume / 80, max_produce / 64);
            size_t cplen = 16;
            size_t symlen = 80;
            for (auto sym_idx = 0; sym_idx < nsyms; sym_idx++) {
                for (auto samp_idx = 0; samp_idx < 64; samp_idx++) {
                    out[nproduced + sym_idx * (symlen - cplen) + samp_idx - cplen] =
                        in[nconsumed + sym_idx * symlen + samp_idx] *
                        exp(gr_complex(0, d_offset * d_freq_offset));
                    d_offset++;
                }
            }

            int i = 80 * nsyms;
            int o = 64 * nsyms;
            nconsumed += i;
            nproduced += o;

            // d_offset += i;

        } else { // WAITING_FOR_TAG

            if (tag_idx < tags.size()) {
                auto offset = tags[tag_idx].offset();

                d_freq_offset_short = pmtf::get_as<double>(tags[0]["wifi_start"]);

                if ((int)(offset - nread + d_fftsize) <= ninput &&
                    (noutput - nproduced) >= 128) {

                    checkCudaErrors(cufftExecC2C(d_plan,
                                                 (cufftComplex*)&in[offset - nread],
                                                 d_dev_in,
                                                 CUFFT_FORWARD));


                    auto gridSize = (d_fftsize + d_block_size - 1) / d_block_size;
                    sync_long_cu::exec_multiply_kernel_ccc(d_dev_in,
                                                           d_dev_training_freq,
                                                           d_dev_in,
                                                           d_fftsize,
                                                           gridSize,
                                                           d_block_size,
                                                           d_stream);

                    checkCudaErrors(
                        cufftExecC2C(d_plan, d_dev_in, d_dev_in, CUFFT_INVERSE));

                    // Find the peak
                    std::vector<gr_complex> host_data(d_fftsize);
                    checkCudaErrors(cudaMemcpyAsync(host_data.data(),
                                                    d_dev_in,
                                                    d_fftsize * sizeof(cufftComplex),
                                                    cudaMemcpyDeviceToHost,
                                                    d_stream));

                    cudaStreamSynchronize(d_stream);
                    std::vector<float> abs_corr(d_fftsize);
                    // std::cout << "freq_corr = [";

                    size_t max_index = 0;
                    size_t max_index_2 = 0;
                    float max_value = 0.0;
                    float max_value_2 = 0.0;
                    gr_complex corr_1;
                    gr_complex corr_2;

                    for (size_t i = 0; i < host_data.size(); i++) {
                        abs_corr[i] = std::abs(host_data[i]);

                        if (abs_corr[i] > max_value) {
                            max_value = abs_corr[i];
                            max_index = i;
                        }
                    }
                    for (size_t i = 0; i < host_data.size(); i++) {
                        if (abs_corr[i] > max_value_2 && i != max_index) {
                            max_value_2 = abs_corr[i];
                            max_index_2 = i;
                        }
                    }

                    bool valid = false;
                    if (max_index_2 > max_index) {
                        auto diff = max_index_2 - max_index;
                        if (diff <= 65 && diff >= 63) {

                            d_freq_offset =
                                arg(host_data[max_index] * conj(host_data[max_index_2])) /
                                (max_index_2 - max_index);
                            max_index = max_index_2;
                            valid = true;
                        }
                    } else {
                        auto diff = max_index - max_index_2;
                        if (diff <= 65 && diff >= 63) {
                            d_freq_offset =
                                arg(host_data[max_index_2] * conj(host_data[max_index])) /
                                (max_index - max_index_2);

                            valid = true;
                        }
                    }

                    if (valid) {

                        // Copy the LTF symbols
                        // offset and nread should always be equal
                        size_t copy_index = 0;
                        if (max_index > (160 - 32 - 1)) {
                            copy_index = max_index - 160 + 32 + 1;
                        }

                        exec_freq_correction(
                            (cuFloatComplex*)in + (offset - nread + copy_index),
                            (cuFloatComplex*)out + nproduced,
                            -d_freq_offset, // it gets negated in the kernel
                            0,
                            128,
                            1,
                            128,
                            d_stream);
                        cudaStreamSynchronize(d_stream);
                        gr_complex host_in[128];
                        gr_complex host_out[128];
                        cudaMemcpy(host_in,
                                   in + (offset - nread + copy_index),
                                   128 * sizeof(gr_complex),
                                   cudaMemcpyDeviceToHost);
                        cudaMemcpy(host_out,
                                   out + nproduced,
                                   128 * sizeof(gr_complex),
                                   cudaMemcpyDeviceToHost);

                        d_offset = 160;

                        wio.outputs()[0].add_tag(
                            nwritten + nproduced / 64,
                            { { "wifi_start", d_freq_offset_short - d_freq_offset },
                              { "srcid", name() } });
                        // std::cout << "SYNC LONG tag at " << nwritten + nproduced /
                        // 64
                        // << std::endl;
                        //   add_item_tag(0, nwritten + nproduced, key, value, srcid);
                        packet_cnt++;
                        if (packet_cnt % 100 == 0) {
                            std::cout << "sync_long: " << packet_cnt << std::endl;
                        }

                        d_num_syms = 0;
                        d_state = FINISH_LAST_FRAME;
                        nconsumed = (offset - nread + copy_index + 128);
                        nproduced += 128;
                    }
                    tag_idx++;

                } else {
                    // not enough left with this tag
                    // clear up to the current tag
                    nconsumed = offset - nread;
                    break;
                }

            } else // out of tags
            {
                nconsumed = ninput;
                break;
            }
        }
    }
    cudaStreamSynchronize(d_stream);
    consume_each(nconsumed, work_input);
    assert(nproduced % 64 == 0);
    // std::cout << " nproduced " <<  nproduced / 64;
    produce_each(nproduced / 64, work_output);

    return work_return_t::OK;
}


const std::vector<gr_complex> sync_long_opt::LONG = {
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