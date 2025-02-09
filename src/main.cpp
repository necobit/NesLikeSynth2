#include <M5Unified.h>
#include <M5Cardputer.h>
#include <MIDI.h>
#include <cmath>
#include "NesWaveform.h"

// サンプリング周波数
const int SAMPLE_RATE = 44100;
// バッファサイズ(4秒分)
const int BUFFER_SIZE = SAMPLE_RATE * 4;
// MIDIチャンネル
const int MIDI_CHANNEL = 1;
// MIDIシリアルピン
const int MIDI_RX_PIN = 1;
const int MIDI_TX_PIN = 2;

// 現在の波形タイプ
NesWaveform::WaveformType currentWaveform = NesWaveform::SQUARE;

// 波形の種類と名前の配列
const struct
{
    NesWaveform::WaveformType type;
    const char *name;
} WAVEFORMS[] = {
    {NesWaveform::SQUARE, "矩形波"},
    {NesWaveform::PULSE_25, "25%パルス波"},
    {NesWaveform::PULSE_12_5, "12.5%パルス波"},
    {NesWaveform::TRIANGLE, "擬似三角波"},
    {NesWaveform::NOISE_LONG, "長周期ノイズ"},
    {NesWaveform::NOISE_SHORT, "短周期ノイズ"}};

// MIDIインターフェースの設定
MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI);

// オーディオバッファ
struct AudioBuffer
{
    int16_t *data;
    size_t size;
    bool isPlaying;
    float volume;
    float frequency;
    uint8_t note;
};

// 現在のバッファ
AudioBuffer currentBuffer;

// MIDIノート番号から周波数を計算
float noteToFreq(uint8_t note)
{
    return 440.0f * pow(2.0f, (note - 69) / 12.0f);
}

// バッファの初期化
void initBuffer(AudioBuffer &buffer)
{
    buffer.data = (int16_t *)malloc(BUFFER_SIZE * sizeof(int16_t));
    buffer.size = BUFFER_SIZE;
    buffer.isPlaying = false;
    buffer.volume = 0.0f;
    buffer.frequency = 440.0f;
    buffer.note = 69;
}

// バッファのクリーンアップ
void cleanupBuffer(AudioBuffer &buffer)
{
    if (buffer.data != nullptr)
    {
        free(buffer.data);
        buffer.data = nullptr;
    }
}

// 波形を生成
void generateWaveform(AudioBuffer &buffer, float frequency, float volume)
{
    if (buffer.data == nullptr)
        return;

    // 波形データを生成
    NesWaveform::generateWaveform(buffer.data, buffer.size, frequency, SAMPLE_RATE, currentWaveform);

    // 音量を適用
    for (size_t i = 0; i < buffer.size; i++)
    {
        buffer.data[i] = buffer.data[i] * volume;
    }

    buffer.frequency = frequency;
    buffer.volume = volume;
}

// 波形を生成して再生
void playNote(uint8_t note, uint8_t velocity)
{
    float frequency = noteToFreq(note);
    float volume = velocity / 127.0f;

    // 波形データを生成
    generateWaveform(currentBuffer, frequency, volume);
    currentBuffer.note = note;

    // 波形名を表示
    M5.Display.clear();
    M5.Display.setCursor(0, 0);
    M5.Display.printf("%s\n%3.1fHz", WAVEFORMS[currentWaveform].name, frequency);

    // 生成したデータをループ再生
    M5.Speaker.playRaw(currentBuffer.data, currentBuffer.size, SAMPLE_RATE, true, 1.0f);
    currentBuffer.isPlaying = true;
}

// 音を停止
void stopNote()
{
    if (currentBuffer.isPlaying)
    {
        M5.Speaker.stop();
        currentBuffer.isPlaying = false;
    }
}

void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
    if (velocity > 0)
    {
        playNote(note, velocity);
    }
    else
    {
        stopNote();
    }
}

void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity)
{
    if (currentBuffer.note == note)
    {
        stopNote();
    }
}

void handleProgramChange(uint8_t channel, uint8_t program)
{
    // プログラム番号は1-6を使用
    if (program >= 1 && program <= 6)
    {
        currentWaveform = static_cast<NesWaveform::WaveformType>(program - 1);

        // 波形名を表示
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.printf("波形変更:\n%s", WAVEFORMS[currentWaveform].name);

        // 現在音が鳴っている場合は、新しい波形で再生し直す
        if (currentBuffer.isPlaying)
        {
            playNote(currentBuffer.note, currentBuffer.volume * 127);
        }
    }
}

void setup()
{
    // M5Stackの初期化
    auto cfg = M5.config();
    M5.begin(cfg);

    // スピーカーの初期化
    M5.Speaker.begin();
    M5.Display.setTextSize(2);
    M5.Display.setFont(&fonts::efontJA_16);
    // バッファの初期化
    initBuffer(currentBuffer);

    // Serial2のピン設定とMIDIの初期化
    Serial2.begin(31250, SERIAL_8N1, MIDI_RX_PIN, MIDI_TX_PIN);
    MIDI.begin(MIDI_CHANNEL);
    MIDI.setHandleNoteOn(handleNoteOn);
    MIDI.setHandleNoteOff(handleNoteOff);
    MIDI.setHandleProgramChange(handleProgramChange);

    M5.Display.setTextSize(2);

    // バッファの初期化
    initBuffer(currentBuffer);

    // Serial2のピン設定とMIDIの初期化
    Serial2.begin(31250, SERIAL_8N1, MIDI_RX_PIN, MIDI_TX_PIN);
    MIDI.begin(MIDI_CHANNEL);
    MIDI.setHandleNoteOn(handleNoteOn);
    MIDI.setHandleNoteOff(handleNoteOff);
    MIDI.setHandleProgramChange(handleProgramChange);

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
