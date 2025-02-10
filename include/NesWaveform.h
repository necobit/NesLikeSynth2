#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <cmath>

class NesWaveform
{
public:
    // 波形の種類
    enum WaveformType
    {
        SQUARE,     // 矩形波 (デューティ比50%)
        PULSE_25,   // パルス波 (デューティ比25%)
        PULSE_12_5, // パルス波 (デューティ比12.5%)
        TRIANGLE,   // 擬似三角波
        NOISE_LONG, // 長周期ノイズ
        NOISE_SHORT // 短周期ノイズ
    };

    // 波形データを生成
    static void generateWaveform(int16_t *buffer, size_t length, float frequency, float sampleRate, WaveformType type)
    {
        const float amplitude = 16384.0f; // 最大振幅を半分に抑える
        const float period = sampleRate / frequency;

        switch (type)
        {
        case SQUARE:
            generateSquareWave(buffer, length, period, amplitude, 0.5f);
            break;
        case PULSE_25:
            generateSquareWave(buffer, length, period, amplitude, 0.25f);
            break;
        case PULSE_12_5:
            generateSquareWave(buffer, length, period, amplitude, 0.125f);
            break;
        case TRIANGLE:
            generateTriangleWave(buffer, length, period, amplitude);
            break;
        case NOISE_LONG:
            generateNoise(buffer, length, amplitude, false);
            break;
        case NOISE_SHORT:
            generateNoise(buffer, length, amplitude, true);
            break;
        }
    }

private:
    // 矩形波/パルス波生成
    static void generateSquareWave(int16_t *buffer, size_t length, float period, float amplitude, float duty)
    {
        float phase = 0.0f;
        const float phaseIncrement = 1.0f / period;

        for (size_t i = 0; i < length; i++)
        {
            buffer[i] = (phase < duty) ? amplitude : -amplitude;
            phase += phaseIncrement;
            if (phase >= 1.0f)
                phase -= 1.0f;
        }
    }

    // 擬似三角波生成(8段階)
    static void generateTriangleWave(int16_t *buffer, size_t length, float period, float amplitude)
    {
        const int STEPS = 15; // ファミコンの三角波は15段階
        const float stepHeight = amplitude / STEPS;
        float phase = 0.0f;
        const float phaseIncrement = 1.0f / period;

        for (size_t i = 0; i < length; i++)
        {
            if (phase < 0.5f)
            {
                // 上昇部分
                int step = (int)(phase * 2 * STEPS);
                buffer[i] = step * stepHeight;
            }
            else
            {
                // 下降部分
                int step = (int)((1.0f - phase) * 2 * STEPS);
                buffer[i] = step * stepHeight;
            }

            phase += phaseIncrement;
            if (phase >= 1.0f)
                phase -= 1.0f;
        }
    }

    // ノイズ生成
    static void generateNoise(int16_t *buffer, size_t length, float amplitude, bool shortPeriod)
    {
        uint16_t lfsr = 1;                          // Linear Feedback Shift Register
        const int updateRate = shortPeriod ? 2 : 4; // 更新頻度を上げる

        for (size_t i = 0; i < length; i++)
        {
            // ノイズの周期を調整
            if (i % updateRate == 0)
            {
                // シフトレジスタの更新
                uint16_t bit = ((lfsr >> 0) ^ (lfsr >> (shortPeriod ? 6 : 1))) & 1;
                lfsr = (lfsr >> 1) | (bit << 14);
            }

            buffer[i] = (lfsr & 1) ? amplitude : -amplitude;
        }
    }
};
