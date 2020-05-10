
#define DR_FLAC_IMPLEMENTATION
#include "../extras/dr_flac.h"  /* Enables FLAC decoding. */
#define DR_MP3_IMPLEMENTATION
#include "../extras/dr_mp3.h"   /* Enables MP3 decoding. */
#define DR_WAV_IMPLEMENTATION
#include "../extras/dr_wav.h"   /* Enables WAV decoding. */

#define MA_DEBUG_OUTPUT
#define MA_IMPLEMENTATION
#include "../miniaudio.h"
#include "ma_mixing.h"

ma_mixer g_mixer;
ma_mixer g_mixerMusic;
ma_mixer g_mixerEffects;

ma_noise g_noise;
ma_waveform g_waveform;
ma_decoder g_decoder;
ma_bool32 g_hasDecoder = MA_FALSE;

void data_callback(ma_device* pDevice, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount)
{
    /* Make sure every output frame is written. */
    while (frameCount > 0) {
        ma_uint64 framesToMixOut;
        ma_uint64 framesToMixIn;
        ma_uint64 submixFrameCountOut;
        ma_uint64 submixFrameCountIn;

        framesToMixOut = frameCount;
        ma_mixer_begin(&g_mixer, NULL, &framesToMixOut, &framesToMixIn);
        {
            /* Music. */
            ma_mixer_begin(&g_mixerMusic, &g_mixer, &submixFrameCountOut, &submixFrameCountIn);
            {
                ma_mixer_mix_noise(&g_mixerMusic, &g_noise, submixFrameCountIn);
                ma_mixer_mix_waveform(&g_mixerMusic, &g_waveform, submixFrameCountIn);
            }
            ma_mixer_end(&g_mixerMusic, &g_mixer, NULL);

            /* Effects. */
            ma_mixer_begin(&g_mixerEffects, &g_mixer, &submixFrameCountOut, &submixFrameCountIn);
            {
                if (g_hasDecoder) {
                    ma_mixer_mix_decoder(&g_mixerEffects, &g_decoder, submixFrameCountIn, MA_TRUE);
                }
            }
            ma_mixer_end(&g_mixerEffects, &g_mixer, NULL);
        }
        ma_mixer_end(&g_mixer, NULL, pFramesOut);

        frameCount -= (ma_uint32)framesToMixOut;   /* Safe cast. */
        pFramesOut  = ma_offset_ptr(pFramesOut, framesToMixOut * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels));
    }
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_device_config deviceConfig;
    ma_device device;
    ma_mixer_config mixerConfig;
    ma_noise_config noiseConfig;
    ma_waveform_config waveformConfig;
    ma_decoder_config decoderConfig;
    const char* pInputFilePath = NULL;
    ma_effect_config effectConfig;
    ma_effect effect;
    
    if (argc > 1) {
        pInputFilePath = argv[1];
    }

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = ma_format_u8;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate        = 0;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = NULL;
    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        return (int)result;
    }


    /* Effect. */
    effectConfig = ma_effect_config_init(ma_effect_type_lpf, /*device.playback.format*/ ma_format_f32, device.playback.channels, device.sampleRate);
    effectConfig.lpf.cutoffFrequency = device.sampleRate / 16;
    effectConfig.lpf.order = 8;
    result = ma_effect_init(&effectConfig, &effect);
    if (result != MA_SUCCESS) {
        return result;
    }


    /* Mixers. */
    mixerConfig = ma_mixer_config_init(device.playback.format, device.playback.channels, 4096, NULL);
    result = ma_mixer_init(&mixerConfig, &g_mixer);
    if (result != MA_SUCCESS) {
        return result;
    }

    result = ma_mixer_init(&mixerConfig, &g_mixerMusic);
    if (result != MA_SUCCESS) {
        return result;
    }

    result = ma_mixer_init(&mixerConfig, &g_mixerEffects);
    if (result != MA_SUCCESS) {
        return result;
    }

    ma_mixer_set_volume(&g_mixerEffects, 1.0f);
    ma_mixer_set_effect(&g_mixerEffects, &effect);

    


    /* Data sources.*/
    noiseConfig = ma_noise_config_init(device.playback.format, device.playback.channels, ma_noise_type_brownian, 0, 0.2);
    result = ma_noise_init(&noiseConfig, &g_noise);
    if (result != MA_SUCCESS) {
        return result;
    }

    waveformConfig = ma_waveform_config_init(device.playback.format, device.playback.channels, device.sampleRate, ma_waveform_type_sine, 0.2, 220);
    result = ma_waveform_init(&waveformConfig, &g_waveform);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (pInputFilePath != NULL) {
        decoderConfig = ma_decoder_config_init(device.playback.format, device.playback.channels, device.sampleRate);
        result = ma_decoder_init_file(pInputFilePath, &decoderConfig, &g_decoder);
        if (result == MA_SUCCESS) {
            g_hasDecoder = MA_TRUE;
        }
    }


    /* Everything is setup. We can now start the device. */
    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        return result;
    }

    printf("Press Enter to quit...");
    getchar();

    return 0;
}