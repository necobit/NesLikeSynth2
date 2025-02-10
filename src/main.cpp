#include <M5Unified.h>
#include <M5Cardputer.h>
#include <MIDI.h>
#include <driver/i2s.h>
#include <cmath>
#include "NesWaveform.h"

// 音声パラメータ構造体
struct VoiceParams
{
    bool isActive;                      // 発音中かどうか
    float volume;                       // 音量
    float frequency;                    // 周波数
    float phase;                        // 現在の位相
    uint8_t note;                       // MIDIノート番号
    NesWaveform::WaveformType waveform; // 波形タイプ
};

// 音声パラメータ設定
const int SAMPLE_RATE = 44100;
const int MIDI_CHANNELS = 16;
const int VOICES_PER_CHANNEL = 6;

// MIDIチャンネル
const int MIDI_CHANNEL = MIDI_CHANNEL_OMNI;
// MIDIシリアルピン
const int MIDI_RX_PIN = 1;
const int MIDI_TX_PIN = 2;

// I2S設定
const i2s_port_t I2S_PORT = I2S_NUM_0;
const int I2S_BCLK = 41;
const int I2S_LRCK = 43;
const int I2S_DOUT = 42;

// 波形の種類と名前の配列
const char *WAVEFORM_NAMES[] = {
    "Square",     // 0
    "25%Pulse",   // 1
    "12.5%Pulse", // 2
    "Triangle",   // 3
    "Long Noise", // 4
    "Short Noise" // 5
};

// チャンネルごとの波形タイプ
NesWaveform::WaveformType channelWaveforms[] = {
    NesWaveform::SQUARE,     // CH1: 矩形波
    NesWaveform::SQUARE,     // CH2: 矩形波
    NesWaveform::PULSE_25,   // CH3: パルス波25%
    NesWaveform::PULSE_12_5, // CH4: パルス波12.5%
    NesWaveform::TRIANGLE,   // CH5: 三角波
    NesWaveform::SQUARE,     // CH6
    NesWaveform::SQUARE,     // CH7
    NesWaveform::SQUARE,     // CH8
    NesWaveform::SQUARE,     // CH9
    NesWaveform::NOISE_LONG, // CH10: 短周期ノイズ
    NesWaveform::NOISE_LONG, // CH11: 短周期ノイズ
    NesWaveform::SQUARE,     // CH12
    NesWaveform::SQUARE,     // CH13
    NesWaveform::SQUARE,     // CH14
    NesWaveform::SQUARE,     // CH15
    NesWaveform::SQUARE      // CH16
};

void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity); // プロトタイプ宣言

void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity); // プロトタイプ宣言

// MIDIインターフェースの設定
MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI);

// 音声パラメータ配列
VoiceParams voices[MIDI_CHANNELS][VOICES_PER_CHANNEL];

// MIDIノート番号から周波数を計算
float noteToFreq(uint8_t note)
{
    return 440.0f * pow(2.0f, (note - 69) / 12.0f);
}

// 波形生成関数
int16_t generateSample()
{
    int32_t mixedSample = 0;
    int totalActiveVoices = 0;

    // 全MIDIチャンネルの音声を合成
    for (int ch = 0; ch < MIDI_CHANNELS; ch++)
    {
        int32_t channelMix = 0;
        int channelActiveVoices = 0;

        // チャンネル内の全ボイスを合成
        for (int v = 0; v < VOICES_PER_CHANNEL; v++)
        {
            if (!voices[ch][v].isActive)
                continue;

            channelActiveVoices++;
            float phaseIncrement = voices[ch][v].frequency / SAMPLE_RATE;
            int16_t sample = 0;

            switch (voices[ch][v].waveform)
            {
            case NesWaveform::SQUARE:
                sample = (voices[ch][v].phase < 0.5f) ? 32767 : -32767;
                break;
            case NesWaveform::PULSE_25:
                sample = (voices[ch][v].phase < 0.25f) ? 32767 : -32767;
                break;
            case NesWaveform::PULSE_12_5:
                sample = (voices[ch][v].phase < 0.125f) ? 32767 : -32767;
                break;
            case NesWaveform::TRIANGLE:
                if (voices[ch][v].phase < 0.25f)
                    sample = voices[ch][v].phase * 4 * 32767;
                else if (voices[ch][v].phase < 0.75f)
                    sample = (0.5f - voices[ch][v].phase) * 4 * 32767;
                else
                    sample = (voices[ch][v].phase - 1.0f) * 4 * 32767;
                break;
            case NesWaveform::NOISE_LONG:
            case NesWaveform::NOISE_SHORT:
            {
                static uint16_t lfsr = 1;
                bool bit;
                if (voices[ch][v].waveform == NesWaveform::NOISE_LONG)
                    bit = ((lfsr >> 0) ^ (lfsr >> 1)) & 1;
                else
                    bit = ((lfsr >> 0) ^ (lfsr >> 6)) & 1;
                lfsr = (lfsr >> 1) | (bit << 14);
                sample = bit ? 32767 : -32767;
                break;
            }
            }

            // 位相を更新
            voices[ch][v].phase += phaseIncrement;
            if (voices[ch][v].phase >= 1.0f)
                voices[ch][v].phase -= 1.0f;

            // 音量を適用
            channelMix += sample * voices[ch][v].volume;
        }

        // チャンネル内のアクティブな音声がある場合は平均化
        if (channelActiveVoices > 0)
        {
            channelMix /= channelActiveVoices;
            mixedSample += channelMix;
            totalActiveVoices++;
        }
    }

    // アクティブなチャンネルがある場合は平均化
    if (totalActiveVoices > 0)
    {
        mixedSample /= totalActiveVoices;
    }

    return mixedSample;
}

// 音声出力タスク
void audioOutputTask(void *parameter)
{
    while (true)
    {
        int16_t stereoSample[2];
        size_t bytesWritten;

        // ステレオサンプルを生成
        int16_t sample = generateSample();
        stereoSample[0] = stereoSample[1] = sample;

        // I2Sに出力
        i2s_write(I2S_PORT, stereoSample, sizeof(stereoSample), &bytesWritten, portMAX_DELAY);
    }
}

// 指定されたMIDIチャンネル内で空いているボイスを探す
int findFreeVoice(uint8_t midiChannel)
{
    // まず非アクティブなボイスを探す
    for (int i = 0; i < VOICES_PER_CHANNEL; i++)
    {
        if (!voices[midiChannel][i].isActive)
            return i;
    }
    // 空きがない場合は最も古いボイスを再利用
    return 0;
}

// ノートオン処理
void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
    if (velocity > 0)
    {
        int voiceIndex = findFreeVoice(channel);
        voices[channel][voiceIndex].isActive = true;
        voices[channel][voiceIndex].frequency = noteToFreq(note);
        voices[channel][voiceIndex].volume = velocity / 127.0f;
        voices[channel][voiceIndex].note = note;
        voices[channel][voiceIndex].waveform = channelWaveforms[channel];

        // 波形名と周波数を表示
        M5.Display.fillScreen(BLACK);
        M5.Display.setCursor(0, 0);
        M5.Display.printf("MIDI CH%d V%d: %s\n%3.1fHz",
                          channel + 1, voiceIndex + 1,
                          WAVEFORM_NAMES[channelWaveforms[channel]],
                          voices[channel][voiceIndex].frequency);
    }
    else
    {
        handleNoteOff(channel, note, velocity);
    }
}

// ノートオフ処理
void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity)
{
    // 該当するMIDIチャンネル内の該当するノートを持つボイスを停止
    for (int i = 0; i < VOICES_PER_CHANNEL; i++)
    {
        if (voices[channel][i].isActive && voices[channel][i].note == note)
        {
            voices[channel][i].isActive = false;
            M5.Display.setCursor(0, 70);
            M5.Display.printf("MIDI CH%d V%d停止", channel + 1, i + 1);
        }
    }
}

// プログラムチェンジ処理
void handleProgramChange(uint8_t channel, uint8_t program)
{
    if (program >= 0 && program <= 5)
    {
        channelWaveforms[channel] = static_cast<NesWaveform::WaveformType>(program);

        // 波形名を表示
        M5.Display.fillScreen(BLACK);
        M5.Display.setCursor(0, 0);
        M5.Display.printf("CH%d波形変更:\n%s", channel + 1, WAVEFORM_NAMES[channelWaveforms[channel]]);
    }
}

void setup()
{
    // M5Stackの初期化
    auto cfg = M5.config();
    M5.begin(cfg);

    delay(2000);
    M5.Display.setTextSize(1);
    M5.Display.setFont(&fonts::efontJA_16);
    M5.Display.setCursor(0, 0);
    M5.Display.printf("Initializing...\n");

    // I2S設定
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0};

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_LRCK,
        .data_out_num = I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE};

    // I2Sドライバーのインストール
    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);

    M5.Display.printf("I2S Initialized\n");

    // 音声パラメータの初期化
    for (int ch = 0; ch < MIDI_CHANNELS; ch++)
    {
        for (int v = 0; v < VOICES_PER_CHANNEL; v++)
        {
            voices[ch][v].isActive = false;
            voices[ch][v].volume = 0.0f;
            voices[ch][v].frequency = 440.0f;
            voices[ch][v].phase = 0.0f;
            voices[ch][v].note = 69;
            voices[ch][v].waveform = NesWaveform::SQUARE;
        }
    }

    // Serial2のピン設定とMIDIの初期化
    Serial2.begin(31250, SERIAL_8N1, MIDI_RX_PIN, MIDI_TX_PIN);
    MIDI.begin(MIDI_CHANNEL);
    MIDI.setHandleNoteOn(handleNoteOn);
    MIDI.setHandleNoteOff(handleNoteOff);
    MIDI.setHandleProgramChange(handleProgramChange);

    // 音声出力タスクの開始
    xTaskCreate(
        audioOutputTask,
        "AudioOutput",
        4096,
        NULL,
        1,
        NULL);

    // 初期画面表示
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.printf("NES Synth\nMulti Channel");
}

void loop()
{
    M5.update();
    MIDI.read();
}
