//================================================================================================//

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <memory>
#include <cstring>
#include <vector>
#include <algorithm>

//------------------------------------------------------------------------------------------------//

#include "piano.hh"

//================================================================================================//

namespace piano_midi
{

//================================================================================================//

using namespace piano;

//================================================================================================//

//
// Used Midi Events from MIDI format
//
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

//------------------------------------------------------------------------------------------------//

//
// Used Meta Events from MIDI format
//
enum meta_event_t
{
    META_EVENT_TEMPO = 0x51,
};

//------------------------------------------------------------------------------------------------//

//
// Prefix of Meta Event
//
static const uint8_t kMetaEventPrefix = 0xff;

//------------------------------------------------------------------------------------------------//

//
// Used System Exclusive events from MIDI format
//
enum sysex_event_t
{
};

//================================================================================================//

//
// Header of MIDI file
//
struct midi_header_t
{
    //
    // Identifier of MIDI file, always "MThd"
    //
    char identifier[4] = {};

    //
    // Number of bytes to the end of this chunk.
    //
    uint32_t chunk_length = 0;

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
    uint16_t format = 0;

    //
    // the number of MTrk chunks following this MThd chunk. For a format 0 MIDI file, ntracks can
    // only be '1'.
    //
    uint16_t ntracks = 0;

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
    uint16_t tickdiv = 0;
};

//------------------------------------------------------------------------------------------------//

//
// Header of track in MIDI file
//
struct track_header_t
{
    //
    // Identifier of track chunk, always "MTrk"
    //
    char     identifier[4] = {};

    //
    // Length of chunk
    //
    uint32_t chunk_length = 0;
};

//================================================================================================//

//
// Prefixes of System Exclusive events
//
static const uint8_t kMetaSysExPrefixes[] = {0xf0, 0xf7};

//
// Read N_BYTES for buffer and move position pointer
//
template <size_t N_BYTES>
static inline auto read_be(const uint8_t *&pos);

//
// Read variable length midi value
//
static inline uint64_t read_var_len(const uint8_t *&pos);

//
// Read MIDI file header. See midi_header_t
//
static status_t read_midi_header(const uint8_t *&pos, midi_header_t  *header);

//
// Read MIDI track header. See track_header_t
//
static status_t read_track_header(const uint8_t *&pos, track_header_t *header);

//
// Translate time in ticks to delta_time in milliseconds
//
static status_t translate_time(std::vector<event_t> &events, const midi_header_t  *midi_header);

//================================================================================================//

template <size_t N_BYTES>
static inline auto
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
    >;
    result_type_t result = 0;
    for (size_t i = 0; i != N_BYTES; ++i)
    {
        result = (result << 8) | static_cast<result_type_t>(pos[i]);
    }
    pos += N_BYTES;
    return result;
}

//------------------------------------------------------------------------------------------------//

static inline uint64_t
read_var_len(const uint8_t *&pos)
{
    uint64_t result = 0;
    uint8_t  byte   = 0;
    do
    {
        byte = *(pos++);
        result = (result << 7) | (byte & 0x7f);
    } while (byte & 0x80);
    return result;
}

//------------------------------------------------------------------------------------------------//

static status_t
read_midi_header(const uint8_t *&pos,
                 midi_header_t  *header)
{
    while (std::string(reinterpret_cast<const char *>(pos), sizeof(header->identifier)) != "MThd")
    {
        pos += sizeof(header->identifier);
        uint32_t length = read_be<4>(pos);
        pos += length;
    }
    std::memcpy(header->identifier, pos, sizeof(header->identifier));
    pos += sizeof(header->identifier);

    header->chunk_length = read_be<4>(pos);
    if (header->chunk_length != 6)
    {
        std::cerr << "Unexpected midi header length: " << (unsigned)header->chunk_length
                  << " (expected always 6)\n";
        return STATUS_MIDI_HEADER_LENGTH_ERROR;
    }

    header->format = read_be<2>(pos);
    if (header->format >= 3)
    {
        std::cerr << "Unexpected MIDI header format: " << header->format << "\n";
        return STATUS_MIDI_HEADER_FORMAT_ERROR;
    }

    header->ntracks = read_be<2>(pos);
    if (header->format == 0 && header->ntracks != 1)
    {
        std::cerr << "Unexpected MIDI header ntracks for format == 0: ntracks == "
                  << header->ntracks << " (expected ntracks == 1)\n";
        return STATUS_MIDI_HEADER_NTRACKS_ERROR;
    }

    header->tickdiv = read_be<2>(pos);
    return STATUS_SUCCESS;
}

//------------------------------------------------------------------------------------------------//

static status_t
read_track_header(const uint8_t *&pos,
                  track_header_t *header)
{
    while (std::string(reinterpret_cast<const char *>(pos), sizeof(header->identifier)) != "MTrk")
    {
        pos += sizeof(header->identifier);
        uint32_t length = read_be<4>(pos);
        pos += length;
    }
    std::memcpy(header->identifier, pos, sizeof(header->identifier));
    pos += sizeof(header->identifier);

    header->chunk_length = read_be<4>(pos);
    return STATUS_SUCCESS;
}

//------------------------------------------------------------------------------------------------//

static status_t
translate_time(std::vector<event_t> &events,
               const midi_header_t  *midi_header)
{
    uint64_t last_ticks = 0;
    for (size_t i = 0; i != events.size(); ++i)
    {
        uint64_t delta_ticks = 0;
        delta_ticks = events[i].time_.current_ticks - last_ticks;
        last_ticks  = events[i].time_.current_ticks;

        if ((midi_header->chunk_length & 0x0080) == 0)
        {
            // Time in metrical
            // Multiplied by 10^6
            // Need to be multiplied by current tempo and divided by 10^3
            events[i].time_.delta_time = static_cast<double>(delta_ticks) /
                                         static_cast<double>(midi_header->tickdiv);
        } else
        {
            uint8_t fps       = midi_header->tickdiv & 0x7f;
            uint8_t subframes = (midi_header->tickdiv >> 8) & 0xff;
            // Actual delta_time, which program has to sleep before event
            events[i].time_.delta_time = 1000.0 * static_cast<double>(delta_ticks) /
                                         static_cast<double>(fps * subframes);
        }
    }
    return STATUS_SUCCESS;
}
//================================================================================================//

status_t
parse_midi(const uint8_t        *midi_data,
           size_t                size,
           std::vector<event_t> &events)
{
    const uint8_t *position = midi_data;

    midi_header_t midi_header = {};
    status_t status = read_midi_header(position, &midi_header);
    if (status != piano::STATUS_SUCCESS)
    {
        return status;
    }

    uint64_t current_time     = 0;
    uint8_t  piano_chanel     = 0xff;
    bool     has_piano_chanel = false;
    for (uint16_t track = 0; track != midi_header.ntracks; ++track)
    {
        switch (midi_header.format)
        {
            case 0:
            {
                // Do nothing as variables will be initialized only once
                break;
            }
            case 1:
            {
                // Al tracks are playing at the same time
                current_time = 0;
                // Skipping if previous track already had piano chanel
                if (has_piano_chanel)
                {
                    // goto start of cycle and this switch again, skipping all tracks
                    continue;
                }
                break;
            }
            case 2:
            {
                // Tracks are playing separately
                piano_chanel     = 0xff;
                has_piano_chanel = false;
                break;
            }
            default:
            {
                std::cerr << "Unexpected format = " << midi_header.format << "\n";
                return STATUS_MIDI_HEADER_FORMAT_ERROR;
            }
        }

        track_header_t track_header = {};
        status = read_track_header(position, &track_header);
        if (status != STATUS_SUCCESS)
        {
            return status;
        }

        const uint8_t *end = position + track_header.chunk_length;
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
            if (track_event == kMetaEventPrefix)
            {
                uint8_t  meta_event        = read_be<1>(position);
                uint64_t meta_event_length = read_var_len(position);

                if (meta_event == META_EVENT_TEMPO)
                {
                    uint32_t tempo = read_be<3>(position);
                    events.emplace_back(event_t(EVENT_TEMPO_SET,
                                                tempo,
                                                current_time));
                } else
                {
                    position += meta_event_length;
                }

                continue;
            }

            // System Exclusive events
            if ((track_event == kMetaSysExPrefixes[0]) ||
                (track_event == kMetaSysExPrefixes[1]))
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
                    case MIDI_EVENT_NOTE_OFF:          { position += 2; break; }
                    case MIDI_EVENT_NOTE_ON:           { position += 2; break; }
                    case MIDI_EVENT_NOTE_AFTERTOUCH:   { position += 2; break; }
                    case MIDI_EVENT_CONTROLLER:        { position += 2; break; }
                    case MIDI_EVENT_PROGRAM_CHANGE:    { position += 1; break; }
                    case MIDI_EVENT_CHANEL_AFTERTOUCH: { position += 1; break; }
                    case MIDI_EVENT_PITCH_BEND:        { position += 2; break; }
                    default:
                    {
                        std::cerr << "Unexpected Midi event: 0x" << std::hex
                                  << (unsigned)midi_event << "\n";
                        return STATUS_MIDI_EVENT_ERROR;
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

            events.push_back(event_t((midi_event == MIDI_EVENT_NOTE_ON) ? EVENT_NOTE_ON
                                                                        : EVENT_NOTE_OFF,
                                     note,
                                     current_time));
        }
    }

    // Sorting events by time in ticks
    std::sort(events.begin(),
              events.end(),
              [](const auto &a, const auto &b)
                { return a.time_.current_ticks < b.time_.current_ticks; });

    // Translating time in millisecond to have faster computations later
    status = translate_time(events, &midi_header);
    if (status != STATUS_SUCCESS)
    {
        return status;
    }

    return STATUS_SUCCESS;
}

//================================================================================================//

} // !namespace piano_midi

//================================================================================================//
