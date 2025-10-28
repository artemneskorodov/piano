#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <thread>

#include "piano.hh"
#include "midi_parser.hh"

size_t get_file_size(std::ifstream &file);

void draw_keys(char *keys)
{
    static int counter = 0;
    std::cout << counter++ << ": ";
    for (int i = 0; i != 128; ++i)
    {
        putchar(keys[i]);
    }
    putchar('\n');
}

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

    std::vector<piano::event_t> events = {};

    piano::status_t result = piano_midi::parse_midi(midi_data.get(), midi_size, events);

    char keys[128] = {};
    for (int i = 0; i < 128; ++i)
    {
        keys[i] = '-';
    }

    uint32_t tempo = 500000;

    for (auto event : events)
    {
        double sleep_step = 100000;
        double sleep_time = static_cast<double>(tempo) * event.time.delta_time;

        for (double slept = 0; slept < sleep_time; slept += sleep_step)
        {
            std::this_thread::sleep_for(std::chrono::duration<double, std::micro>(sleep_step));
            draw_keys(keys);
        }
        if (event.event == piano::EVENT_NOTE_ON)
        {
            keys[event.data.note] = 'x';
        } else if (event.event == piano::EVENT_NOTE_OFF)
        {
            keys[event.data.note] = '-';
        } else if (event.event == piano::EVENT_TEMPO_SET)
        {
            tempo = event.data.tempo;
        }
    }

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
