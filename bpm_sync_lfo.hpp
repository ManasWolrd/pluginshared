#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <bit>

namespace pluginshared {
class BpmSyncLFO {
public:
    static constexpr std::array kBaseRateTable{
        1.0f / 64.0f, 1.0f / 32.0f, 1.0f / 16.0f, 1.0f / 8.0f, 1.0f / 4.0f, 1.0f / 2.0f, 1.0f, 2.0f, 4.0f, 8.0f,
    };
    static constexpr std::array kBaseRateTextTable{
        "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1", "2", "4", "8",
    };
    static constexpr size_t kRateTableSize = kBaseRateTable.size() * 3 * 2 + 1;
    static constexpr std::array kRateMulTable = [] {
        std::array<float, kRateTableSize> output_table{};
        // negative 0 positive
        // 1.5f 1.0f 0.66f
        size_t wpos = 0;
        for (auto it = kBaseRateTable.rbegin(); it != kBaseRateTable.rend(); ++it) {
            auto val = *it;
            output_table[wpos++] = -1.5f * val;
            output_table[wpos++] = -1.0f * val;
            output_table[wpos++] = -2.0f / 3.0f * val;
        }
        output_table[wpos++] = 0.0f;
        for (auto val : kBaseRateTable) {
            output_table[wpos++] = 2.0f / 3.0f * val;
            output_table[wpos++] = 1.0f * val;
            output_table[wpos++] = 1.5f * val;
        }
        return output_table;
    }();
    inline static std::array<juce::String, kRateTableSize> s_rate_name_table;
    inline static bool s_static_inited = false;

    static void TryInitTempoTable() {
        if (s_static_inited) return;
        s_static_inited = true;

        size_t wpos = 0;
        for (auto val : kBaseRateTextTable) {
            s_rate_name_table[wpos++] = juce::String{"-"} + val + "T";
            s_rate_name_table[wpos++] = juce::String{"-"} + val;
            s_rate_name_table[wpos++] = juce::String{"-"} + val + "D";
        }
        s_rate_name_table[wpos++] = "freeze";
        for (auto it = kBaseRateTextTable.rbegin(); it != kBaseRateTextTable.rend(); ++it) {
            auto val = *it;
            s_rate_name_table[wpos++] = juce::String{val} + "D";
            s_rate_name_table[wpos++] = juce::String{val};
            s_rate_name_table[wpos++] = juce::String{val} + "T";
        }
    }

    static int GetTempoValueIndex(juce::StringRef rate_name) {
        auto it = std::find(s_rate_name_table.begin(), s_rate_name_table.end(), rate_name);
        jassert(it != s_rate_name_table.end());
        int where = static_cast<int>(it - s_rate_name_table.begin());
        return where;
    }

    BpmSyncLFO() {
        TryInitTempoTable();
    }

    BpmSyncLFO(juce::String name, float min_freq, float max_freq, float interval, float warp, bool warpnp,
               juce::StringRef begin_tempo, juce::StringRef end_tempo) {
        TryInitTempoTable();

        name_ = std::move(name);
        std::construct_at(&free_freq_range_, min_freq, max_freq, interval, warp, warpnp);
        tempo_begin_idx_ = GetTempoValueIndex(begin_tempo);
        tempo_end_idx_ = GetTempoValueIndex(end_tempo);
    }

    struct LfoInfo {
        float lfo_freq;
        float lfo_phase;
    };
    LfoInfo SyncBpm(juce::AudioPlayHead* head, float phase01) {
        float fbpm = 120.0f;
        float fppq = 0.0f;
        bool sync_lfo = false;
        if (head != nullptr) {
            auto pos = head->getPosition();
            if (auto bpm = pos->getBpm(); bpm) {
                fbpm = static_cast<float>(*bpm);
            }
            if (auto ppq = pos->getPpqPosition(); ppq) {
                fppq = static_cast<float>(*ppq);
                sync_lfo = true;
            }
            if (!pos->getIsPlaying()) {
                sync_lfo = false;
            }
        }

        float lfo_freq = 0.0f;
        FreqAttrubute freq_attr = GetFreqAttribute();
        if (!freq_attr.tempo_sync) {
            lfo_freq = free_freq_range_.convertFrom0to1(param_freq->get());
            sync_lfo = false;
        }
        else {
            if (!freq_attr.ppq_sync) {
                sync_lfo = false;
            }

            float findex =
                std::lerp(static_cast<float>(tempo_begin_idx_), static_cast<float>(tempo_end_idx_), param_freq->get());
            int index = static_cast<int>(findex);
            index = std::clamp(index, tempo_begin_idx_, tempo_end_idx_);

            float sync_rate = kRateMulTable[static_cast<size_t>(index)];
            if (!freq_attr.tempo_snap) {
                int nindex = std::min(index + 1, tempo_end_idx_);
                float next_rate = kRateMulTable[static_cast<size_t>(nindex)];
                sync_rate = std::lerp(sync_rate, next_rate, findex - static_cast<float>(index));
            }

            float sync_phase = sync_rate * fppq;
            sync_phase -= std::floor(sync_phase);
            if (sync_lfo) {
                phase01 = sync_phase;
            }

            lfo_freq = sync_rate * fbpm / 60.0f;
        }

        return LfoInfo{lfo_freq, phase01};
    }

    [[nodiscard]]
    std::pair<std::unique_ptr<juce::AudioParameterInt>, std::unique_ptr<juce::AudioParameterFloat>> Build() {
        auto ptype = std::make_unique<juce::AudioParameterInt>(juce::ParameterID{name_ + "_type", 1}, name_ + "_type",
                                                               0, std::numeric_limits<int32_t>::max(), 0);
        param_type = ptype.get();

        auto attr =
            juce::AudioParameterFloatAttributes{}
                .withStringFromValueFunction([this, float_numeric = GetFloatNumericText(free_freq_range_.interval)](
                                                 auto x, auto) -> juce::String {
                    FreqAttrubute freq_attr = GetFreqAttribute();
                    if (freq_attr.tempo_sync) {
                        int index = static_cast<int>(
                            std::lerp(static_cast<float>(tempo_begin_idx_), static_cast<float>(tempo_end_idx_), x));
                        index = std::clamp(index, tempo_begin_idx_, tempo_end_idx_);
                        return s_rate_name_table[static_cast<size_t>(index)];
                    }
                    else {
                        return juce::String{free_freq_range_.convertFrom0to1(x), float_numeric};
                    }
                })
                .withValueFromStringFunction([this](juce::String x) -> float {
                    FreqAttrubute freq_attr = GetFreqAttribute();
                    if (freq_attr.tempo_sync) {
                        auto it = std::find(s_rate_name_table.begin(), s_rate_name_table.end(), x);
                        if (it == s_rate_name_table.end()) return 0.0f;

                        int where = static_cast<int>(it - s_rate_name_table.begin());
                        where = std::clamp(where, tempo_begin_idx_, tempo_end_idx_);
                        float val01 = static_cast<float>(where - tempo_begin_idx_)
                                    / static_cast<float>(tempo_end_idx_ - tempo_begin_idx_);
                        return val01;
                    }
                    else {
                        return free_freq_range_.convertTo0to1(x.getFloatValue());
                    }
                });
        auto pfreq = std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{name_ + "_freq", 1}, name_ + "_freq",
                                                                 juce::NormalisableRange<float>{0, 1}, 0, attr);
        param_freq = pfreq.get();

        return {std::move(ptype), std::move(pfreq)};
    }

    [[nodiscard]]
    std::pair<std::unique_ptr<juce::AudioParameterInt>, std::unique_ptr<juce::AudioParameterFloat>> Build(
        juce::String name, float min_freq, float max_freq, float interval, float warp, bool warpnp,
        juce::StringRef begin_tempo, juce::StringRef end_tempo) {
        name_ = std::move(name);
        std::construct_at(&free_freq_range_, min_freq, max_freq, interval, warp, warpnp);
        tempo_begin_idx_ = GetTempoValueIndex(begin_tempo);
        tempo_end_idx_ = GetTempoValueIndex(end_tempo);
        return Build();
    }

    struct FreqAttrubute {
        int tempo_sync : 1;
        int ppq_sync : 1;
        int tempo_snap : 1;
    };
    FreqAttrubute GetFreqAttribute() const {
        return std::bit_cast<FreqAttrubute>(param_type->get());
    }

    void SetFreqAttribute(FreqAttrubute attr) {
        param_freq->setValueNotifyingHost(param_freq->convertTo0to1(static_cast<float>(std::bit_cast<int>(attr))));
    }

    juce::AudioParameterFloat* param_freq{};
    juce::AudioParameterInt* param_type{};
private:
    static int GetFloatNumericText(float interval) {
        float v = 1.0f;
        int num = 0;
        while (v > interval) {
            v /= 10.0f;
            ++num;
        }
        return num;
    }

    juce::String name_;
    juce::NormalisableRange<float> free_freq_range_;
    int tempo_begin_idx_;
    int tempo_end_idx_;
};
} // namespace pluginshared
