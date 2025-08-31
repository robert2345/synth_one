Plays sounds with computer keyboard as note input.

Currently consists of a PWM pulse oscillator source which feeds a low pass filter. Low frequency sine wave oscillators modulate the pulse width of the oscillator and the cutoff frequency of the filter.

- Clone repo
- cmake .
- make
- ./synth_one
