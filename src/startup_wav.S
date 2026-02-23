    .section .rodata
    .align 4
    .global startup_wav_data
startup_wav_data:
    .incbin "../sounds/w95s.wav"
startup_wav_end:
    .global startup_wav_size
startup_wav_size:
    .word startup_wav_end - startup_wav_data
