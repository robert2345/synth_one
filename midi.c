#include <alsa/asoundlib.h>

#include "midi.h"

snd_rawmidi_t *midi_start()
{
    int fd;

    snd_rawmidi_t *inputp;
    if (0 < (fd = snd_rawmidi_open(&inputp, NULL, "virtual", SND_RAWMIDI_NONBLOCK)))

    {
        fprintf(stderr, "open error: %d - %s\n", (int)fd, snd_strerror(fd));
        return NULL;
    }
    else
    {
        return inputp;
    }
}

void midi_stop(snd_rawmidi_t *inputp)
{
    snd_rawmidi_close(inputp);
}

bool midi_get(snd_rawmidi_t *inputp, struct midi_message *msg)
{
    unsigned char byte;
    static enum midi_msg_type status = MIDI_MSG_NONE;
    static unsigned bytes_read = 0;

    if (inputp == NULL)
        return NULL;

    ssize_t ret;
    while (true)
    {
        ret = snd_rawmidi_read(inputp, &byte, 1);
        if (ret == 0)
        {
            return false;
        }
        else if (ret < 0)
        {
            if (ret != -EAGAIN)
            {
                fprintf(stderr, "read error: %d - %s\n", (int)ret, snd_strerror(ret));
            }
            return false;
        }
        else if (byte & 0x80) // STATUS BYTE
        {
            if ((byte >> 4) == 0x9) // nOTE on, any channel
            {
                msg->type = status = MIDI_MSG_NOTE_ON;
            }
            else if ((byte >> 4) == 0x8) // nOTE off, any channel
            {
                msg->type = status = MIDI_MSG_NOTE_OFF;
            }
            else
            {
                status = MIDI_MSG_NONE;
            }
            bytes_read = 0;
        }
        else // DATA BYTE
        {
            switch (status)
            {
            case MIDI_MSG_NOTE_ON:
            case MIDI_MSG_NOTE_OFF:
                if (bytes_read == 1)
                {
                    if (byte == 0)
                        msg->type = MIDI_MSG_NOTE_OFF;
                    else
                        msg->note.velocity = 1.0 * byte / 127;
                    bytes_read = 0;
                    return true;
                }
                else if (bytes_read == 0)
                {
                    msg->note.key = byte;
                    bytes_read++;
                }
                else
                {
                    fprintf(stderr, "wrong byte count!\n");
                    return false;
                }
                break;
            default:
                // Data regarding a status type not yet implemented
            }
        }
    }
}
