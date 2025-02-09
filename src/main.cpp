#include <M5Unified.h>
#include <M5Cardputer.h>
#include <MIDI.h>
#include <cmath>
#include "NesWaveform.h"

// サンプリング周波数
const int SAMPLE_RATE = 44100;
// 最大バッファサイズ(ノイズ用)
const int MAX_BUFFER_SIZE = SAMPLE_RATE / 4; // 0.25秒分
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
    float currentPhase; // 波形の現在の位相を追跡
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
    buffer.data = (int16_t *)malloc(MAX_BUFFER_SIZE * sizeof(int16_t));
    buffer.size = MAX_BUFFER_SIZE;
    buffer.isPlaying = false;
    buffer.volume = 0.0f;
    buffer.frequency = 440.0f;
    buffer.note = 69;
    buffer.currentPhase = 0.0f;
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

// 波形を生成して再生
void playNote(uint8_t note, uint8_t velocity)
{
    float frequency = noteToFreq(note);
    float volume = velocity / 127.0f;

    // 波形データを生成
    NesWaveform::generateWaveform(currentBuffer.data, currentBuffer.size, frequency, SAMPLE_RATE, currentWaveform);
    currentBuffer.note = note;
    currentBuffer.volume = volume;     // volumeを保存
    currentBuffer.currentPhase = 0.0f; // 位相をリセット

    // 波形名を表示
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.printf("%s\n%3.1fHz", WAVEFORMS[currentWaveform].name, frequency);

    // 生成したデータをループ再生
    M5.Speaker.setVolume(128);
    // チャンネル0を使用、ループ再生、音量1.0、バッファ数8
    M5.Speaker.playRaw(currentBuffer.data, currentBuffer.size, SAMPLE_RATE, true, 1.0f, 0, 8);
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
    // プログラム番号は0-5を使用
    if (program >= 0 && program <= 5)
    {
        currentWaveform = static_cast<NesWaveform::WaveformType>(program);

        // 波形名を表示
        M5.Display.fillScreen(BLACK);
        M5.Display.setCursor(0, 0);
        M5.Display.printf("波形変更:\n%s", WAVEFORMS[currentWaveform].name);

        // 現在音が鳴っている場合は、新しい波形で再生し直す
        if (currentBuffer.isPlaying)
        {
            stopNote();
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

    // バッファの初期化
    initBuffer(currentBuffer);

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
