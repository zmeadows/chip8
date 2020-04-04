#include "chip8_audio.hpp"

#define MA_NO_DECODING
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"

namespace chip8::audio {

namespace {

constexpr auto DEVICE_FORMAT = ma_format_f32;
constexpr auto DEVICE_CHANNELS = 2;
constexpr auto DEVICE_SAMPLE_RATE = 48000;
constexpr auto SINE_WAVE_FREQUENCY = 250;

ma_device device;
ma_device_config device_config;

ma_waveform sine_wave;
ma_waveform_config sine_wave_config;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_waveform* pSineWave;

    MA_ASSERT(pDevice->playback.channels == DEVICE_CHANNELS);

    pSineWave = (ma_waveform*)pDevice->pUserData;
    MA_ASSERT(pSineWave != NULL);

    ma_waveform_read_pcm_frames(pSineWave, pOutput, frameCount);

    (void)pInput; /* Unused. */
}

} // namespace

void init(void)
{
    sine_wave_config =
        ma_waveform_config_init(DEVICE_FORMAT, DEVICE_CHANNELS, DEVICE_SAMPLE_RATE,
                                ma_waveform_type_sine, 0.2, SINE_WAVE_FREQUENCY);

    ma_waveform_init(&sine_wave_config, &sine_wave);

    device_config = ma_device_config_init(ma_device_type_playback);
    device_config.playback.format = DEVICE_FORMAT;
    device_config.playback.channels = DEVICE_CHANNELS;
    device_config.sampleRate = DEVICE_SAMPLE_RATE;
    device_config.dataCallback = data_callback;
    device_config.pUserData = &sine_wave;

    if (ma_device_init(NULL, &device_config, &device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to open audio playback device.\n");
        return;
    }

    printf("Device Name: %s\n", device.playback.name);
}

void terminate(void) { ma_device_uninit(&device); }

void start_beep(void)
{
    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        return;
    }
}

void stop_beep(void)
{
    if (ma_device_stop(&device) != MA_SUCCESS) {
        printf("Failed to stop playback device.\n");
        ma_device_uninit(&device);
        return;
    }
}

} // namespace chip8::audio
