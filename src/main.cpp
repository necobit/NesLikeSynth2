#include <M5Unified.h>
#include <M5Cardputer.h>
#include <MIDI.h>
#include <driver/i2s.h>
#include <cmath>
#include "NesWaveform.h"

// サンプリング周波数
const int SAMPLE_RATE = 44100;
// 最大バッファサイズ(ノイズ用)
const int MAX_BUFFER_SIZE = SAMPLE_RATE; // 1秒分(最大値)
// MIDIチャンネル
const int MIDI_CHANNEL = MIDI_CHANNEL_OMNI;
// MIDIシリアルピン
const int MIDI_RX_PIN = 1;
const int MIDI_TX_PIN = 2;

// I2S設定
const i2s_port_t I2S_PORT = I2S_NUM_0;
const int I2S_BCLK = 41; // I2S BCLKピン
const int I2S_LRCK = 43; // I2S WS(LRCK)ピン
const int I2S_DOUT = 42; // I2S DATAピン

// 現在の波形タイプ
NesWaveform::WaveformType currentWaveform = NesWaveform::SQUARE;

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

// オーディオバッファ
struct AudioBuffer
{
    int16_t *data;
    size_t size;
    bool isPlaying;
    float volume;
    float frequency;
    uint8_t note;
    float currentPhase;        // 波形の現在の位相を追跡
    TaskHandle_t playbackTask; // 再生用タスクハンドル
};

// 現在のバッファ
AudioBuffer currentBuffer;

// 再生用タスク
void playbackTask(void *parameter)
{
    AudioBuffer *buffer = (AudioBuffer *)parameter;
    size_t bytesWritten = 0;

    while (buffer->isPlaying)
    {
        // ステレオデータを作成(左右同じデータ)
        int16_t stereoData[2];
        static size_t currentIndex = 0;

        // 音量を適用
        stereoData[0] = stereoData[1] = buffer->data[currentIndex] * buffer->volume;

        // I2Sにデータを書き込み
        i2s_write(I2S_PORT, stereoData, sizeof(stereoData), &bytesWritten, portMAX_DELAY);

        // インデックスを更新(ループ再生)
        currentIndex = (currentIndex + 1) % buffer->size;
    }

    // タスクを削除
    buffer->playbackTask = nullptr;
    vTaskDelete(NULL);
}

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
    buffer.playbackTask = nullptr;
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

    // バッファサイズを周波数に応じて1周期分に設定
    currentBuffer.size = static_cast<size_t>(SAMPLE_RATE / frequency);

    // 波形データを生成
    NesWaveform::generateWaveform(currentBuffer.data, currentBuffer.size, frequency, SAMPLE_RATE, currentWaveform);
    currentBuffer.note = note;
    currentBuffer.volume = volume;
    currentBuffer.currentPhase = 0.0f;

    // 波形名を表示
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.printf("%s\n%3.1fHz", WAVEFORMS[currentWaveform].name, frequency);

    // 再生タスクを開始
    currentBuffer.isPlaying = true;
    xTaskCreate(
        playbackTask,
        "PlaybackTask",
        4096,
        &currentBuffer,
        1,
        &currentBuffer.playbackTask);

    M5.Display.setCursor(0, 70);
    M5.Display.printf("音再生");
}

// 音を停止
void stopNote()
{
    if (currentBuffer.isPlaying)
    {
        M5.Display.setCursor(0, 70);
        M5.Display.printf("音停止");

        // 再生フラグをオフにしてタスクを終了させる
        currentBuffer.isPlaying = false;

        // タスクが完全に終了するまで待機
        while (currentBuffer.playbackTask != nullptr)
        {
            delay(1);
        }
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

    delay(2000);
    M5.Display.setTextSize(2);
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

    // I2Sピン設定
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_LRCK,
        .data_out_num = I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE};

    // I2Sドライバーのインストール
    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);

    M5.Display.printf("I2S Initialized\n");

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

    // 初期音テスト
}

void loop()
{
    M5.update();
    MIDI.read();
}
