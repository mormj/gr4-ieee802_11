#!/usr/bin/env python3
# -*- coding: utf-8 -*-

#
# SPDX-License-Identifier: GPL-3.0
#
# GNU Radio Python Flow Graph
# Title: Not titled yet
# Author: josh
# GNU Radio version: 4.0.0.0-preview0

from gnuradio import fileio
from gnuradio import filter
from gnuradio import gr
#from gnuradio.filter import firdes
#from gnuradio.fft import window
import sys
import signal
from argparse import ArgumentParser
#from gnuradio.eng_arg import eng_float, intx
#from gnuradio import eng_notation
from gnuradio import ieee802_11
from gnuradio import math
from gnuradio import streamops
import time



def snipfcn_snippet_0(fg, rt=None):
    fg.startt = time.time()

def snipfcn_snippet_0_0(fg, rt=None):
    endt = time.time()
    print(f'[PROFILE_TIME]{endt-fg.startt}[PROFILE_TIME]')


def snippets_main_after_init(fg, rt=None):
    snipfcn_snippet_0(fg, rt)

def snippets_main_after_stop(fg, rt=None):
    snipfcn_snippet_0_0(fg, rt)


class bench_wifi_rx(gr.flowgraph):

    def __init__(self):
        gr.flowgraph.__init__(self, "Not titled yet")

        ##################################################
        # Variables
        ##################################################
        self.window_size = window_size = 48
        self.sync_length = sync_length = 320
        self.samp_rate_0 = samp_rate_0 = 20000000
        self.samp_rate = samp_rate = 32000
        self.freq = freq = 2462000000
        self.chan_est = chan_est = 0

        ##################################################
        # Blocks
        ##################################################
        self.streamops_stream_to_vector_0 = streamops.stream_to_vector( 64,0, impl=streamops.stream_to_vector.cpu)
        self.streamops_delay_0_0 = streamops.delay( sync_length,0, impl=streamops.delay.cpu)
        self.streamops_delay_0 = streamops.delay( 16,0, impl=streamops.delay.cpu)
        self.math_multiply_0 = math.multiply_cc( 2,1, impl=math.multiply_cc.cpu)
        self.math_divide_0 = math.divide_ff( 2,1, impl=math.divide_ff.cpu)
        self.math_conjugate_0 = math.conjugate( impl=math.conjugate.cpu)
        self.math_complex_to_mag_squared_0 = math.complex_to_mag_squared( 1, impl=math.complex_to_mag_squared.cpu)
        self.math_complex_to_mag_1 = math.complex_to_mag( 1, impl=math.complex_to_mag.cpu)
        self.ieee802_11_sync_short_0 = ieee802_11.sync_short( 0.56,2,False,False, impl=ieee802_11.sync_short.cpu)
        self.ieee802_11_sync_long_0 = ieee802_11.sync_long( 320,False,False, impl=ieee802_11.sync_long.cpu)
        self.ieee802_11_packetize_frame_0 = ieee802_11.packetize_frame( ieee802_11.algorithm_t.LS,freq,samp_rate,False,False, impl=ieee802_11.packetize_frame.cpu)
        self.filter_moving_average_1 = filter.moving_average_cc( window_size,1,4096,1, impl=filter.moving_average_cc.cpu)
        self.filter_moving_average_0 = filter.moving_average_ff( window_size  + 16,1,4096,1, impl=filter.moving_average_ff.cpu)
        self.fileio_file_source_0 = fileio.file_source( '/data/data/cropcircles/wifi_synth_1500_1kpad_20MHz_10s_MCS0.fc32',False,0,0,0, impl=fileio.file_source.cpu)


        ##################################################
        # Connections
        ##################################################
        self.connect((self.fileio_file_source_0, 0), (self.math_complex_to_mag_squared_0, 0))
        self.connect((self.fileio_file_source_0, 0), (self.math_multiply_0, 1))
        self.connect((self.fileio_file_source_0, 0), (self.streamops_delay_0, 0))
        self.connect((self.filter_moving_average_0, 0), (self.math_divide_0, 1))
        self.connect((self.filter_moving_average_1, 0), (self.ieee802_11_sync_short_0, 1))
        self.connect((self.filter_moving_average_1, 0), (self.math_complex_to_mag_1, 0))
        self.connect((self.ieee802_11_sync_long_0, 0), (self.streamops_stream_to_vector_0, 0))
        self.connect((self.ieee802_11_sync_short_0, 0), (self.ieee802_11_sync_long_0, 0))
        self.connect((self.ieee802_11_sync_short_0, 0), (self.streamops_delay_0_0, 0))
        self.connect((self.math_complex_to_mag_1, 0), (self.math_divide_0, 0))
        self.connect((self.math_complex_to_mag_squared_0, 0), (self.filter_moving_average_0, 0))
        self.connect((self.math_conjugate_0, 0), (self.math_multiply_0, 0))
        self.connect((self.math_divide_0, 0), (self.ieee802_11_sync_short_0, 2))
        self.connect((self.math_multiply_0, 0), (self.filter_moving_average_1, 0))
        self.connect((self.streamops_delay_0, 0), (self.ieee802_11_sync_short_0, 0))
        self.connect((self.streamops_delay_0, 0), (self.math_conjugate_0, 0))
        self.connect((self.streamops_delay_0_0, 0), (self.ieee802_11_sync_long_0, 1))
        self.connect((self.streamops_stream_to_vector_0, 0), (self.ieee802_11_packetize_frame_0, 0))


    def get_window_size(self):
        return self.window_size

    def set_window_size(self, window_size):
        self.window_size = window_size

    def get_sync_length(self):
        return self.sync_length

    def set_sync_length(self, sync_length):
        self.sync_length = sync_length

    def get_samp_rate_0(self):
        return self.samp_rate_0

    def set_samp_rate_0(self, samp_rate_0):
        self.samp_rate_0 = samp_rate_0

    def get_samp_rate(self):
        return self.samp_rate

    def set_samp_rate(self, samp_rate):
        self.samp_rate = samp_rate

    def get_freq(self):
        return self.freq

    def set_freq(self, freq):
        self.freq = freq

    def get_chan_est(self):
        return self.chan_est

    def set_chan_est(self, chan_est):
        self.chan_est = chan_est




def main(flowgraph_cls=bench_wifi_rx, options=None):
    fg = flowgraph_cls()
    rt = gr.runtime()
    snippets_main_after_init(fg, rt)

    rt.initialize(fg)

    rt.start()

    try:
        rt.wait()
    except KeyboardInterrupt:
        rt.stop()
        rt.wait()
    snippets_main_after_stop(fg, rt)

if __name__ == '__main__':
    main()
