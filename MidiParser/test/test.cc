#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <thread>

#include "piano.hh"
#include "midi_parser.hh"

size_t get_file_size(std::ifstream &file);

char gKeys[128] = {};
bool gNeedDrawing = false;

void
worker()
{
    while (gNeedDrawing)
    {
        for (int i = 0; i != 128; ++i)
        {
            if (gKeys[i] == 1)
            {
                printf("█");
            } else
            {
                printf("░");
            }
        }
        putchar('\n');
        std::this_thread::sleep_for(std::chrono::duration<double, std::micro>(100000));
    }
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

    std::cout << result << std::endl;

    std::cout << "3\n";
    std::this_thread::sleep_for(std::chrono::duration<double, std::micro>(1000000));

    std::cout << "2\n";
    std::this_thread::sleep_for(std::chrono::duration<double, std::micro>(1000000));

    std::cout << "1\n";
    std::this_thread::sleep_for(std::chrono::duration<double, std::micro>(1000000));

    gNeedDrawing = true;
    std::thread t(worker);

    std::cout << "0\n";

    double tempo = 500000.;

    for (auto event : events)
    {
        double sleep_time = tempo * event.time.delta_time;

        std::this_thread::sleep_for(std::chrono::duration<double, std::micro>(sleep_time));

        if (event.event == piano::EVENT_NOTE_ON)
        {
            gKeys[event.data.note] = 1;
        } else if (event.event == piano::EVENT_NOTE_OFF)
        {
            gKeys[event.data.note] = 0;
        } else if (event.event == piano::EVENT_TEMPO_SET)
        {
            tempo = static_cast<double>(event.data.tempo);
        }
    }
    gNeedDrawing = false;
    t.join();
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
