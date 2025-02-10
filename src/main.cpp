#include <M5Unified.h>
#include <M5Cardputer.h>
#include <MIDI.h>
#include <driver/i2s.h>
#include <cmath>
#include "NesWaveform.h"

// サンプリング周波数
const int SAMPLE_RATE = 44100;
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

// 現在の波形タイプ
NesWaveform::WaveformType currentWaveform = NesWaveform::SQUARE;

void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity); // プロトタイプ宣言

void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity); // プロトタイプ宣言

// 波形の種類と名前の配列
const struct
{
    NesWaveform::WaveformType type;
    const char *name;
} WAVEFORMS[] = {
    {NesWaveform::SQUARE, "Square"},
    {NesWaveform::PULSE_25, "25%Pulse"},
    {NesWaveform::PULSE_12_5, "12.5%Pulse"},
    {NesWaveform::TRIANGLE, "Triangle"},
    {NesWaveform::NOISE_LONG, "Long Noise"},
    {NesWaveform::NOISE_SHORT, "Short Noise"}};

// MIDIインターフェースの設定
MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI);

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

// 音声パラメータ(16チャンネル)
const int MAX_VOICES = 16;
VoiceParams voices[MAX_VOICES];

// MIDIノート番号から周波数を計算
float noteToFreq(uint8_t note)
{
    return 440.0f * pow(2.0f, (note - 69) / 12.0f);
}

// 波形生成関数
int16_t generateSample()
{
    int32_t mixedSample = 0;
    int activeVoices = 0;

    // 全てのアクティブな音声を合成
    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (!voices[i].isActive)
            continue;

        activeVoices++;
        float phaseIncrement = voices[i].frequency / SAMPLE_RATE;
        int16_t sample = 0;

        switch (voices[i].waveform)
        {
        case NesWaveform::SQUARE:
            sample = (voices[i].phase < 0.5f) ? 32767 : -32767;
            break;
        case NesWaveform::PULSE_25:
            sample = (voices[i].phase < 0.25f) ? 32767 : -32767;
            break;
        case NesWaveform::PULSE_12_5:
            sample = (voices[i].phase < 0.125f) ? 32767 : -32767;
            break;
        case NesWaveform::TRIANGLE:
            if (voices[i].phase < 0.25f)
                sample = voices[i].phase * 4 * 32767;
            else if (voices[i].phase < 0.75f)
                sample = (0.5f - voices[i].phase) * 4 * 32767;
            else
                sample = (voices[i].phase - 1.0f) * 4 * 32767;
            break;
        case NesWaveform::NOISE_LONG:
        case NesWaveform::NOISE_SHORT:
        {
            static uint16_t lfsr = 1;
            bool bit;
            if (voices[i].waveform == NesWaveform::NOISE_LONG)
                bit = ((lfsr >> 0) ^ (lfsr >> 1)) & 1;
            else
                bit = ((lfsr >> 0) ^ (lfsr >> 6)) & 1;
            lfsr = (lfsr >> 1) | (bit << 14);
            sample = bit ? 32767 : -32767;
            break;
        }
        }

        // 位相を更新
        voices[i].phase += phaseIncrement;
        if (voices[i].phase >= 1.0f)
            voices[i].phase -= 1.0f;

        // 音量を適用
        mixedSample += sample * voices[i].volume;
    }

    // アクティブな音声がある場合は平均化
    if (activeVoices > 0)
    {
        mixedSample /= activeVoices;
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

// 空いているボイスを探す
int findFreeVoice()
{
    // まず非アクティブなボイスを探す
    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (!voices[i].isActive)
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
        int voiceIndex = findFreeVoice();
        voices[voiceIndex].isActive = true;
        voices[voiceIndex].frequency = noteToFreq(note);
        voices[voiceIndex].volume = velocity / 127.0f;
        voices[voiceIndex].note = note;
        voices[voiceIndex].waveform = currentWaveform;

        // 波形名と周波数を表示
        M5.Display.fillScreen(BLACK);
        M5.Display.setCursor(0, 0);
        M5.Display.printf("CH%d: %s\n%3.1fHz", voiceIndex + 1, WAVEFORMS[currentWaveform].name, voices[voiceIndex].frequency);
    }
    else
    {
        handleNoteOff(channel, note, velocity);
    }
}

// ノートオフ処理
void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity)
{
    // 該当するノートを持つ全てのボイスを停止
    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (voices[i].isActive && voices[i].note == note)
        {
            voices[i].isActive = false;
            M5.Display.setCursor(0, 70);
            M5.Display.printf("CH%d停止", i + 1);
        }
    }
}

// プログラムチェンジ処理
void handleProgramChange(uint8_t channel, uint8_t program)
{
    if (program >= 0 && program <= 5)
    {
        currentWaveform = static_cast<NesWaveform::WaveformType>(program);

        // 波形名を表示
        M5.Display.fillScreen(BLACK);
        M5.Display.setCursor(0, 0);
        M5.Display.printf("波形変更:\n%s", WAVEFORMS[currentWaveform].name);
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
    for (int i = 0; i < MAX_VOICES; i++)
    {
        voices[i].isActive = false;
        voices[i].volume = 0.0f;
        voices[i].frequency = 440.0f;
        voices[i].phase = 0.0f;
        voices[i].note = 69;
        voices[i].waveform = NesWaveform::SQUARE;
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
    M5.Display.printf("NES Synth\n%s", WAVEFORMS[currentWaveform].name);
}

void loop()
{
    M5.update();
    MIDI.read();
}
