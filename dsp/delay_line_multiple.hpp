#pragma once
#include <vector>
#include "../simd.hpp"

namespace pluginshared::dsp {
template <simd::IsSimdFloat SimdT>
class DelayLineMultiple {
public:
    void Init(float max_ms, float fs) {
        float d = max_ms * fs / 1000.0f;
        size_t i = static_cast<size_t>(d);
        Init(i);
    }

    void Init(size_t max_samples) {
        uint32_t a = 1;
        while (a < max_samples) {
            a *= 2;
        }
        size_ = static_cast<int>(a);
        mask_ = static_cast<int>(a - 1);
        uint32_t each_size = a * 2;
        buffer_.resize(each_size * simd::LaneSize<SimdT>);
        for (size_t i = 0; i < simd::LaneSize<SimdT>; ++i) {
            ptrs_[i] = buffer_.data() + static_cast<size_t>(size_) * i * 2;
        }
    }

    void Reset() noexcept {
        wpos_ = 0;
        std::fill(buffer_.begin(), buffer_.end(), float{});
    }

    void Push(SimdT x) noexcept {
        wpos_ = (wpos_ + 1) & mask_;
        for (size_t i = 0; i < simd::LaneSize<SimdT>; ++i) {
            ptrs_[i][wpos_] = x[i];
            ptrs_[i][wpos_ + size_] = x[i];
        }
    }

    SimdT GetAfterPush(SimdT delay_samples) noexcept {
        auto rpos = static_cast<float>(wpos_ + size_) - delay_samples;
        return GetRpos(rpos);
    }

    SimdT GetBeforePush(SimdT delay_samples) noexcept {
        auto rpos = static_cast<float>(wpos_ + mask_) - delay_samples;
        return GetRpos(rpos);
    }
private:
    simd::Float128 GetRpos(simd::Float128 rpos) noexcept {
        auto t = simd::Frac128(rpos);
        // we are at the -1 position[-1, 0, 1, 2]
        auto irpos = simd::ToInt128(rpos) - 1;
        irpos &= mask_;

        auto [yn1, y0, y1, y2] =
            simd::Transpose(simd::Loadu128(ptrs_[0] + irpos[0]), simd::Loadu128(ptrs_[1] + irpos[1]),
                            simd::Loadu128(ptrs_[2] + irpos[2]), simd::Loadu128(ptrs_[3] + irpos[3]));

        auto d0 = (y1 - yn1) * 0.5f;
        auto d1 = (y2 - y0) * 0.5f;
        auto d = y1 - y0;
        auto m0 = 3.0f * d - 2.0f * d0 - d1;
        auto m1 = d0 - 2.0f * d + d1;
        return y0 + t * (d0 + t * (m0 + t * m1));
    }

    simd::Float256 GetRpos(simd::Float256 rpos) noexcept {
        auto t = simd::Frac256(rpos);
        // we are at the -1 position[-1, 0, 1, 2]
        auto irpos = simd::ToInt256(rpos) - 1;
        irpos &= mask_;

        auto [yn1, y0, y1, y2] =
            simd::Transpose256(simd::Loadu128(ptrs_[0] + irpos[0]), simd::Loadu128(ptrs_[1] + irpos[1]),
                               simd::Loadu128(ptrs_[2] + irpos[2]), simd::Loadu128(ptrs_[3] + irpos[3]),
                               simd::Loadu128(ptrs_[4] + irpos[4]), simd::Loadu128(ptrs_[5] + irpos[5]),
                               simd::Loadu128(ptrs_[6] + irpos[6]), simd::Loadu128(ptrs_[7] + irpos[7]));

        auto d0 = (y1 - yn1) * 0.5f;
        auto d1 = (y2 - y0) * 0.5f;
        auto d = y1 - y0;
        auto m0 = 3.0f * d - 2.0f * d0 - d1;
        auto m1 = d0 - 2.0f * d + d1;
        return y0 + t * (d0 + t * (m0 + t * m1));
    }

    std::vector<float> buffer_;
    std::array<float*, simd::LaneSize<SimdT>> ptrs_;
    int size_{};
    int wpos_{};
    int mask_{};
};
} // namespace pluginshared::dsp
