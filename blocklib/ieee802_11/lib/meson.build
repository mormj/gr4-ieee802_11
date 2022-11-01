ieee802_11_sources += ['utils.cc',
                'constellations.cc',
                'equalizer/base.cc',
                'equalizer/comb.cc',
                'equalizer/lms.cc',
                'equalizer/ls.cc',
                'equalizer/sta.cc',
                'viterbi_decoder/base.cc',
                # 'viterbi_decoder/viterbi_decoder_generic.cc',
                'viterbi_decoder/viterbi_decoder_x86.cc']
ieee802_11_deps += [gnuradio_gr_dep, gr_kernel_fft_lib_dep, gr_kernel_filter_lib_dep, gr_kernel_digital_lib_dep, volk_dep, fmt_dep, pmtf_dep, python3_embed_dep, json_dep]

block_cpp_args = ['-DHAVE_CPU']

# if IMPLEMENT_CUDA
#     block_cpp_args += '-DHAVE_CUDA'

#     gnuradio_blocklib_ieee802_11_cu = library('gnuradio-blocklib-ieee802_11-cu', 
#         ieee802_11_cu_sources, 
#         include_directories : incdir, 
#         install : true, 
#         dependencies : [cuda_dep])

#     gnuradio_blocklib_ieee802_11_cu_dep = declare_dependency(include_directories : incdir,
#                         link_with : gnuradio_blocklib_ieee802_11_cu,
#                         dependencies : cuda_dep)

#     ieee802_11_deps += [gnuradio_blocklib_ieee802_11_cu_dep, cuda_dep]

# endif

incdir = include_directories(['../include/gnuradio/ieee802_11','../include'])
gnuradio_blocklib_ieee802_11_lib = library('gnuradio-blocklib-ieee802_11', 
    ieee802_11_sources, 
    include_directories : incdir, 
    install : true,
    link_language: 'cpp',
    dependencies : ieee802_11_deps,
    link_args : ['-lgnuradio-runtime', '-lgnuradio-kernel-filter', '-lgnuradio-kernel-fft', '-lgnuradio-kernel-digital'],
    cpp_args : block_cpp_args)

gnuradio_blocklib_ieee802_11_dep = declare_dependency(include_directories : incdir,
					   link_with : gnuradio_blocklib_ieee802_11_lib,
                       dependencies : ieee802_11_deps)
