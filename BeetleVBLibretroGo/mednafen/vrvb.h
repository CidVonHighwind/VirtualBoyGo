
#include <functional>

namespace VRVB {

    extern uint16_t input_buf[];

    std::function<void(int16_t *SoundBuf, int32_t SoundBufSize)> audio_cb;

    std::function<void(const void *data, unsigned width, unsigned height)> video_cb;

    void Init();

    void LoadRom(const uint8_t *data, size_t size);

    void unload_game();

    void Run();

    void UpdateScreen();

    void SetPalette();

    size_t retro_serialize_size();

    void Reset();

    size_t save_ram_size();

    void *save_ram();

    void load_ram(const void *data, size_t size);

    bool retro_serialize(void *data, size_t size);

    bool retro_unserialize(const void *data, size_t size);

}