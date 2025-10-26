#ifndef __MIDI_PARSER_HH__
#define __MIDI_PARSER_HH__

#include <cstdint>
#include <vector>

#include "piano.hh"

namespace piano_midi
{

piano::status_t parse_midi(const uint8_t *midi_data, size_t size, std::vector<piano::event_t> &events);

} // ! namespace piano_midi

#endif // ! __MIDI_PARSER_HH__
