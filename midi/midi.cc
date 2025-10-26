#include <iostream>
#include <fstream>
#include <cstdlib>
#include <memory>
#include <cstring>
#include <vector>
#include <algorithm>

namespace piano_midi
{

enum status_t
{
    STATUS_SUCCESS = 0x0,
    STATUS_UNEXPECTED_MIDI_HEADER = 0x1,
    STATUS_UNEXPECTED_MIDI_HEADER_LENGTH = 0x2,
    STATUS_UNEXPECTED_MIDI_FORMAT = 0x3,
    STATUS_UNEXPECTED_MIDI_EVENT = 0x4,
    STATUS_UNEXPECTED_NTRACKS = 0x5,
};

status_t handle_midi(const uint8_t *midi_data, size_t size);

} // !namespace piano_midi

size_t get_file_size(std::ifstream &file);

int
main(int argc, const char *argv[])
{
    if (argc != 2)
    {
        std::cerr << argv[0] << ": usage: " << argv[0] << " <file.mid>\n" ;
        return EXIT_FAILURE;
    }

    std::ifstream midi_file(argv[1], std::ios::binary);
    if (!midi_file.is_open())
    {
        std::cerr << "Error while opening " << argv[1] << "\n";
        return EXIT_FAILURE;
    }

    size_t midi_size = get_file_size(midi_file);

    std::unique_ptr<uint8_t[]> midi_data = std::make_unique<uint8_t[]>(midi_size);
    midi_file.read(reinterpret_cast<char *>(midi_data.get()), midi_size);
    midi_file.close();

    piano_midi::status_t result = piano_midi::handle_midi(midi_data.get(), midi_size);
    std::cout << "Exit status = " << static_cast<int>(result) << std::endl;
    return static_cast<int>(result);
}

size_t
get_file_size(std::ifstream &file)
{
    std::streampos pos = file.tellg();
    file.seekg(0, std::ios::end);
    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(pos, std::ios::beg);
    return size;
}

namespace piano_midi
{

template <size_t N_BYTES>
inline auto
read_be(const uint8_t *&pos)
{
    static_assert(N_BYTES >= 1 && N_BYTES <= 8, "N must be between 1 and 8");
    using result_type_t = std::conditional_t<
        N_BYTES <= 1,
        uint8_t,
        std::conditional_t<
            N_BYTES <= 2,
            uint16_t,
            std::conditional_t<
                N_BYTES <= 4,
                uint32_t,
                uint64_t
            >
        >
    >
    result_type_t result = 0;
    for (size_t i = 0; i != N_BYTES; ++i)
    {
        result = (result << 8) | static_cast<result_type_t>(pos[i]);
    }
    pos += N_BYTES;
    return result;
}

inline uint64_t
read_var_len(const uint8_t *&pos)
{
    uint64_t result = 0;
    uint8_t byte    = 0;
    do
    {
        byte = *(pos++);
        result = (result << 7) | byte;
    } while (byte & 0x80);
    return result;
}

enum midi_event_t
{
    MIDI_EVENT_NOTE_OFF          = 0x80,
    MIDI_EVENT_NOTE_ON           = 0x90,
    MIDI_EVENT_NOTE_AFTERTOUCH   = 0xa0,
    MIDI_EVENT_CONTROLLER        = 0xb0,
    MIDI_EVENT_PROGRAM_CHANGE    = 0xc0,
    MIDI_EVENT_CHANEL_AFTERTOUCH = 0xd0,
    MIDI_EVENT_PITCH_BEND        = 0xe0,
};

enum meta_event_t
{
    META_EVENT_TEMPO = 0x51,
};

status_t
handle_midi(const uint8_t *midi_data,
            size_t         size)
{
    const uint8_t *position = midi_data;

    //
    // Midi header
    //
    // "MThd" - identifier (4 bytes)
    // length - number of bytes in next 3 elements, always 6 bytes(variable length value)
    while (std::string(reinterpret_cast<const char *>(position), 4) != "MThd")
    {
        position += 4;
        uint32_t chunk_length = read_be<4>(position);
        position += chunk_length;
    }
    position += 4;

    uint32_t length = read_be<4>(position);
    if (length != 6)
    {
        std::cerr << "Unexpected length = " << length << "(expected always 6)\n";
        return STATUS_UNEXPECTED_MIDI_HEADER_LENGTH;
    }

    //
    //  - format == 0:
    //    The MIDI file contains just a single MTrk chunk, that can potentially contain
    //    multi-channel MIDI data.
    //
    //  - format == 1:
    //    The file contains two or more MTrk chunks (as specified by the following parameter,
    //    ntracks) that are to be played simultaneously, i.e. analogous to a mulitrack tape
    //    recorder. The first track is a tempo track that should only contain tempo related Meta
    //    events (i.e. no actual MIDI data) – this is clarified later. This is the most commonly
    //    used format, as the various instrumental parts within a composition can be stored in
    //    separate tracks, allowing for easier editing. It is possible to store multi-channel data
    //    in a track, though it is more usual to keep data relevant to a single MIDI channel in each
    //    track.
    //
    //  - format == 2:
    //    The file contains one or more MTrk chunks (as specified by the following parameter,
    //    ntracks) that are intended to be played independently, i.e. analogous to a drum machine's
    //    pattern memory. A format 2 file can be likened to multiple format 0 files all wrapped up
    //    in a single file.
    //
    uint16_t format = read_be<2>(position);
    if (format >= 3)
    {
        std::cerr << "Unexpected MIDI format: " << format << "\n";
        return STATUS_UNEXPECTED_MIDI_FORMAT;
    }

    //
    // the number of MTrk chunks following this MThd chunk. For a format 0 MIDI file, ntracks can
    // only be '1'.
    //
    uint16_t ntracks = read_be<2>(position);
    if (format == 0 && ntracks != 1)
    {
        std::cerr << "It is expected to be ntracks == 1 for format == 0, actually: ntracks == "
                  << ntracks << "\n";
        return STATUS_UNEXPECTED_NTRACKS;
    }

    //
    // tickdiv specifies the timing interval to be used, and whether timecode (Hrs.Mins.Secs.Frames)
    // or metrical (Bar.Beat) timing is to be used. With metrical timing, the timing interval is
    // tempo related, whereas with timecode the timing interval is in absolute time, and hence not
    // related to tempo.
    //
    // Bit 15 (the top bit of the first byte) is a flag indicating the timing scheme in use :
    //
    // Bit 15 = 0 : metrical timing
    // Bits 0 - 14 are a 15-bit number indicating the number of sub-divisions of a quarter note
    // (aka pulses per quarter note, ppqn). A common value is 96, which would be represented in hex
    // as 00 60. You will notice that 96 is a nice number for dividing by 2 or 3 (with further
    // repeated halving), so using this value for tickdiv allows triplets and dotted notes right
    // down to hemi-demi-semiquavers to be represented.
    //
    // Bit 15 = 1 : timecode
    // Bits 8 - 15 (i.e. the first byte) specifies the number of frames per second (fps), and will
    // be one of the four SMPTE standards - 24, 25, 29 or 30, though expressed as a negative value
    // (using 2's complement notation), as follows:
    //
    // fps	Representation (hex)
    // 24 E8
    // 25 E7
    // 29 E3
    // 30 E2
    //
    // Bits 0 - 7 (the second byte) specifies the sub-frame resolution, i.e. the number of
    // sub-divisions of a frame. Typical values are 4 (corresponding to MIDI Time Code), 8, 10, 80
    // (corresponding to SMPTE bit resolution), or 100.
    //
    // A timing resolution of 1 ms can be achieved by specifying 25 fps and 40 sub-frames, which
    // would be encoded in hex as E7 28.
    //
    uint16_t tickdiv = read_be<2>(position);

    // Creating array of events, which are interesting for piano
    enum piano_event_num_t
    {
        PIANO_EVENT_NOTE_ON   = 0x1,
        PIANO_EVENT_NOTE_OFF  = 0x2,
        PIANO_EVENT_TEMPO_SET = 0x3,
    };
    struct piano_event_t
    {
        piano_event_num_t event;
        // new tempo value for PIANO_EVENT_TEMPO_SET and note number for PIANO_EVENT_NOTE_ON and
        // PIANO_EVENT_NOTE_OFF events.
        uint32_t          data;
        union
        {
            double        delta_time;
            uint64_t      current_time;
        };
    };

    std::vector<piano_event_t> piano_events = {};

    uint64_t current_time     = 0;
    uint8_t  piano_chanel     = 0xff;
    bool     has_piano_chanel = false;
    for (uint16_t track = 0; track != ntracks; ++track)
    {
        switch (format)
        {
            case 0:
            {
                // Do nothing as variables will be initialized only once
                break;
            }
            case 1:
            {
                current_time = 0;
                // Skipping if previous track already had piano chanel
                if (has_piano_chanel)
                {
                    continue;
                }
            }
            case 2:
            {
                piano_chanel     = 0xff;
                has_piano_chanel = false;
            }
            default:
            {
                return STATUS_UNEXPECTED_MIDI_FORMAT;
            }
        }

        // MTrk track identifier
        while (std::string(reinterpret_cast<const char *>(position), 4) != "MThd")
        {
            position += 4;
            uint32_t chunk_length = read_be<4>(position);
            position += chunk_length;
        }
        position += 4;

        uint32_t track_length = read_be<4>(position);

        const uint8_t *end = position + track_length;
        uint8_t last_track_event = 0;
        while (position != end)
        {
            uint64_t delta_time = read_var_len(position);
            current_time += delta_time;

            uint8_t track_event = read_be<1>(position);
            if ((track_event & 0x80) == 0x0)
            {
                // Saving last event
                track_event = last_track_event;
                position--;
            } else
            {
                last_track_event = track_event;
            }

            // Meta events
            if (track_event == 0xff)
            {
                uint8_t  meta_event        = read_be<1>(position);
                uint64_t meta_event_length = read_var_len(position);

                if (meta_event == META_EVENT_TEMPO)
                {
                    uint32_t tempo = read_be<3>(position);
                    piano_event_t event =
                    {
                        .event        = PIANO_EVENT_TEMPO_SET,
                        .data         = tempo,
                        .current_time = current_time
                    };
                    piano_events.push_back(event);
                } else
                {
                    position += length;
                }

                continue;
            }

            // System Exclusive events
            if (track_event == 0xf0 || track_event == 0xf7)
            {
                uint64_t sysex_event_length = read_var_len(position);

                position += sysex_event_length;

                continue;
            }

            // Midi events
            uint8_t midi_event  = track_event & 0xf0;
            uint8_t midi_chanel = track_event & 0x0f;

            // Program change event is parsed separately to get piano chanel
            if (midi_event == MIDI_EVENT_PROGRAM_CHANGE)
            {
                uint8_t program = read_be<1>(position);

                if (program >= 0 && program <= 7)
                {
                    piano_chanel     = std::min(midi_chanel, piano_chanel);
                    has_piano_chanel = true;
                }

                continue;
            }

            // Skipping if not piano event or not note on and note off events
            if ((!has_piano_chanel) ||
                (midi_chanel != piano_chanel) ||
                (midi_event != MIDI_EVENT_NOTE_OFF && midi_event != MIDI_EVENT_NOTE_ON))
            {
                switch (midi_event)
                {
                    case MIDI_EVENT_NOTE_OFF         : {position += 2; break; }
                    case MIDI_EVENT_NOTE_ON          : {position += 2; break; }
                    case MIDI_EVENT_NOTE_AFTERTOUCH  : {position += 2; break; }
                    case MIDI_EVENT_CONTROLLER       : {position += 2; break; }
                    case MIDI_EVENT_PROGRAM_CHANGE   : {position += 1; break; }
                    case MIDI_EVENT_CHANEL_AFTERTOUCH: {position += 1; break; }
                    case MIDI_EVENT_PITCH_BEND       : {position += 2; break; }
                    default:
                    {
                        std::cerr << "Unexpected Midi event: 0x" << std::hex
                                  << (unsigned)midi_event << "\n";
                        return STATUS_UNEXPECTED_MIDI_EVENT;
                    }
                }
                continue;
            }

            // Note On and Note Off events
            uint8_t note     = read_be<1>(position);
            uint8_t velocity = read_be<1>(position);

            if (midi_event == MIDI_EVENT_NOTE_ON && velocity == 0)
            {
                midi_event = MIDI_EVENT_NOTE_OFF;
            }

            piano_event_t event =
            {
                .event        = (midi_event == MIDI_EVENT_NOTE_ON) ? PIANO_EVENT_NOTE_ON
                                                                   : PIANO_EVENT_NOTE_OFF,
                .data         = static_cast<uint32_t>(note),
                .current_time = current_time
            };

            piano_events.emplace_back(event);
        }
    }

    // Sorting events by time in ticks
    std::sort(piano_events.begin(),
              piano_events.end(),
              [](const auto &a, const auto &b) { return a.current_time < b.current_time; });

    // Translating time in ticks to time in milliseconds
    for (size_t i = 0; i != piano_events.size(); ++i)
    {
        uint64_t delta_ticks = 0;
        if (i != 0)
        {
            delta_ticks = piano_events[i].current_time - piano_events[i - 1].current_time;
        } else
        {
            delta_ticks = piano_events[i].current_time;
        }

        if ((tickdiv & 0x0080) == 0x0)
        {
            // Time in metrical
            // Multiplied by 10^6
            // Need to be multiplied by current delta and divided by 10^6
            piano_events[i].delta_time = 1000.0 * static_cast<double>(delta_ticks) /
                                         static_cast<double>(tickdiv);
        } else
        {
            int fps = -(int8_t)(tickdiv & 0xff);
            int subframes = (tickdiv >> 8) & 0xff;
            // Actual delta_time, which program has to sleep before event
            piano_events[i].delta_time = 1000.0 * static_cast<double>(delta_ticks) / static_cast<double>(fps * subframes);
        }
    }

    return STATUS_SUCCESS;
}

} // !namespace piano_midi
