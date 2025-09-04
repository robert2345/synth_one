Plays sounds with computer keyboard as note input.

Currently consists of a PWM pulse oscillator source which feeds a low pass filter. Low frequency sine wave oscillators modulate the pulse width of the oscillator and the cutoff frequency of the filter. There is a delay and chorus.

<img width="642" height="519" alt="image" src="https://github.com/user-attachments/assets/69df59ea-e27c-4679-a4b3-b2c4e53e890e" />

- Clone repo
- cmake .
- make
- ./synth_one
