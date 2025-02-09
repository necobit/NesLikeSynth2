#include <M5Unified.h>
#include <M5Cardputer.h>
#include <MIDI.h>
#include <math.h>

// サンプリング周波数
const int SAMPLE_RATE = 44100;
// 再生時間(500ミリ秒)
const int DURATION_MS = 500;
// 目的の周波数(440Hz)
const float FREQUENCY = 440.0;

// サイン波データを生成する関数
void generateSineWave(int16_t *buffer, size_t length)
{
    const float amplitude = 32767.0; // 16ビットの最大振幅
    const float angular_frequency = 2.0 * M_PI * FREQUENCY;

    for (size_t i = 0; i < length; i++)
    {
        float time = (float)i / SAMPLE_RATE;
        buffer[i] = (int16_t)(amplitude * sin(angular_frequency * time));
    }
}

void setup()
{
    // M5Stackの初期化
    auto cfg = M5.config();
    M5.begin(cfg);

    // スピーカーの初期化
    M5.Speaker.begin();

    // 500ミリ秒分のサンプル数を計算
    size_t num_samples = (SAMPLE_RATE * DURATION_MS) / 1000;

    // サイン波データを格納するバッファを作成
    int16_t *wave_buffer = (int16_t *)malloc(num_samples * sizeof(int16_t));
    if (wave_buffer == nullptr)
    {
        return; // メモリ確保失敗
    }

    // サイン波データを生成
    generateSineWave(wave_buffer, num_samples);

    // 生成したデータを再生
    M5.Speaker.playRaw(wave_buffer, num_samples, SAMPLE_RATE, false, 1, 0);

    // 再生が終わるまで待機
    delay(DURATION_MS);

    // 音を停止
    M5.Speaker.stop();

    // メモリを解放
    free(wave_buffer);
}

void loop()
{
    // メインループでは何もしない
    M5.update();
}
