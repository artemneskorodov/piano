#ifndef __PIANO_HH__
#define __PIANO_HH__

namespace piano
{

enum status_t
{
    STATUS_SUCCESS                   = 0x0,
    STATUS_MIDI_HEADER_LENGTH_ERROR  = 0x1,
    STATUS_MIDI_HEADER_FORMAT_ERROR  = 0x2,
    STATUS_MIDI_HEADER_NTRACKS_ERROR = 0x3,
    STATUS_MIDI_EVENT_ERROR          = 0x4,
};

enum event_num_t
{
    EVENT_NOTE_ON   = 0x1,
    EVENT_NOTE_OFF  = 0x2,
    EVENT_TEMPO_SET = 0x3,
};

struct event_t
{
    event_num_t  event;
    union
    {
        uint8_t  note;
        uint32_t tempo;
    } data;
    union
    {
        uint64_t current_ticks;
        double   delta_time;
    } time;
};

} // ! namespace piano

#endif // ! __PIANO_HH__
