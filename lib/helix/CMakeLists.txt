add_library(helix STATIC
    mp3dec.c
    mp3tabs.c
    real/bitstream.c
    real/buffers.c
    real/dct32.c
    real/dequant.c
    real/dqchan.c
    real/huffman.c
    real/hufftabs.c
    real/imdct.c
    real/polyphase.c
    real/scalfact.c
    real/stproc.c
    real/subband.c
    real/trigtabs.c
)
target_include_directories(helix PUBLIC pub real)
