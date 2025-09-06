#include <alsa/asoundlib.h>
#include <stdbool.h>

enum midi_msg_type
{
    MIDI_MSG_NONE = 0,
    MIDI_MSG_NOTE_ON,
    MIDI_MSG_NOTE_OFF,
    MIDI_MSG_NBR_OFF,
};
struct midi_note
{
    int key;
    float velocity;
};
struct midi_message
{
    enum midi_msg_type type;
    struct midi_note note;
};

snd_rawmidi_t *midi_start();
void midi_stop(snd_rawmidi_t *inputp);
bool midi_get(snd_rawmidi_t *inputp, struct midi_message *msg);
