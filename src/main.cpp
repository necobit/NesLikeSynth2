#include <M5Unified.h>
#include <M5Cardputer.h>
#include <MIDI.h>
#include "NesWaveform.h"

// サンプリング周波数
const int SAMPLE_RATE = 44100;
// 再生時間(500ミリ秒)
const int DURATION_MS = 500;
// 目的の周波数(440Hz)
const float FREQUENCY = 440.0;

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

// 波形を再生する関数
void playWaveform(NesWaveform::WaveformType type, const char *name)
{
    // サンプル数を計算
    size_t num_samples = (SAMPLE_RATE * DURATION_MS) / 1000;

    // 波形データ用のバッファを確保
    int16_t *wave_buffer = (int16_t *)malloc(num_samples * sizeof(int16_t));
    if (wave_buffer == nullptr)
    {
        return; // メモリ確保失敗
    }

    // 波形データを生成
    NesWaveform::generateWaveform(wave_buffer, num_samples, FREQUENCY, SAMPLE_RATE, type);

    // 波形名を表示
    M5.Display.clear();
    M5.Display.setCursor(0, 0);
    M5.Display.printf("%s\n440Hz", name);

    // 生成したデータを再生
    M5.Speaker.playRaw(wave_buffer, num_samples, SAMPLE_RATE, false, 1, 0);

    // 再生が終わるまで待機
    delay(DURATION_MS);

    // 音を停止
    M5.Speaker.stop();

    // メモリを解放
    free(wave_buffer);

    // 次の波形までの間隔
    delay(100);
}

void setup()
{
    // M5Stackの初期化
    auto cfg = M5.config();
    M5.begin(cfg);

    // スピーカーの初期化
    M5.Speaker.begin();
    M5.Display.setTextSize(2);

    // 各波形を順番に再生
    for (const auto &waveform : WAVEFORMS)
    {
        playWaveform(waveform.type, waveform.name);
    }
}

void loop()
{
    // メインループでは何もしない
    M5.update();
}
