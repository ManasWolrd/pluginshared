#pragma once
#include <type_traits>
#include <array>
#include <cstddef>

namespace simd {
#if defined (__clang__)
using Float128 = float __attribute__((ext_vector_type(4)));
using Int128 = int __attribute__((ext_vector_type(4)));

using Float256 = float __attribute__((ext_vector_type(8)));
using Int256 = int __attribute__((ext_vector_type(8)));
#else
using Float128 = float __attribute__((vector_size(16)));
using Int128 = int __attribute__((vector_size(16)));

using Float256 = float __attribute__((vector_size(32)));
using Int256 = int __attribute__((vector_size(32)));
#endif

template<class T, size_t N>
struct alignas(16) Array128 : public std::array<T, N> {};
template<class T, size_t N>
struct alignas(32) Array256 : public std::array<T, N> {};

template<class T>
concept IsSimdFloat = std::is_same_v<T, Float128> || std::is_same_v<T, Float256>;

template<class T>
static constexpr size_t LaneSize = sizeof(T) / sizeof(float);

// ----------------------------------------
// extension?
// ----------------------------------------

static inline Int128 ToInt128(Float128 x) noexcept {
    return __builtin_convertvector(x, Int128);
}
static inline Int256 ToInt256(Float256 x) noexcept {
    return __builtin_convertvector(x, Int256);
}

static inline Float128 ToFloat128(Int128 x) noexcept {
    return __builtin_convertvector(x, Float128);
}
static inline Float256 ToFloat256(Int256 x) noexcept {
    return __builtin_convertvector(x, Float256);
}

static inline Float128 Frac128(Float128 x) noexcept {
    return x - ToFloat128(ToInt128(x));
}
static inline Float256 Frac256(Float256 x) noexcept {
    return x - ToFloat256(ToInt256(x));
}

static inline Float128 Loadu128(const float* ptr) noexcept {
    return {ptr[0], ptr[1], ptr[2], ptr[3]};
}
static inline Float256 Loadu256(const float* ptr) noexcept {
    return {ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7]};
}

static inline Float128 Max128(Float128 a, Float128 b) noexcept {
    return a > b ? a : b;
}
static inline Float256 Max256(Float256 a, Float256 b) noexcept {
    return a > b ? a : b;
}

static inline Float128 BroadcastF128(float i) noexcept {
    return {i, i, i, i};
}
static inline Float256 BroadcastF256(float i) noexcept {
    return {i, i, i, i, i, i, i, i};
}

static inline Float256 combine(Float128 lo, Float128 hi) {
    return {lo[0], lo[1], lo[2], lo[3], hi[0], hi[1], hi[2], hi[3]};
}

static inline std::array<Float128, 4> Transpose(Float128 x0, Float128 x1, Float128 x2, Float128 x3) noexcept {
    Float128 tmp0 = __builtin_shufflevector(x0, x1, 0, 4, 1, 5); // row0[0], row1[0], row0[1], row1[1]
    Float128 tmp1 = __builtin_shufflevector(x0, x1, 2, 6, 3, 7); // row0[2], row1[2], row0[3], row1[3]
    Float128 tmp2 = __builtin_shufflevector(x2, x3, 0, 4, 1, 5); // row2[0], row3[0], row2[1], row3[1]
    Float128 tmp3 = __builtin_shufflevector(x2, x3, 2, 6, 3, 7); // row2[2], row3[2], row2[3], row3[3]

    Float128 row0 = __builtin_shufflevector(tmp0, tmp2, 0, 1, 4, 5); // [r0c0, r1c0, r2c0, r3c0]
    Float128 row1 = __builtin_shufflevector(tmp0, tmp2, 2, 3, 6, 7); // [r0c1, r1c1, r2c1, r3c1]
    Float128 row2 = __builtin_shufflevector(tmp1, tmp3, 0, 1, 4, 5); // [r0c2, r1c1, r2c2, r3c2]
    Float128 row3 = __builtin_shufflevector(tmp1, tmp3, 2, 3, 6, 7); // [r0c3, r1c3, r2c3, r3c3]
    
    return {row0, row1, row2, row3};
}
static inline std::array<Float256, 4> Transpose256(
    Float128 a, Float128 b, Float128 c, Float128 d,
    Float128 e, Float128 f, Float128 g, Float128 h
) noexcept {
    Float256 x0 = combine(a, e);
    Float256 x1 = combine(b, f);
    Float256 x2 = combine(c, g);
    Float256 x3 = combine(d, h);

    Float256 m0 = __builtin_shufflevector(x0, x1, 0, 8, 1, 9, 4, 12, 5, 13);
    Float256 m1 = __builtin_shufflevector(x0, x1, 2, 10, 3, 11, 6, 14, 7, 15);
    Float256 m2 = __builtin_shufflevector(x2, x3, 0, 8, 1, 9, 4, 12, 5, 13);
    Float256 m3 = __builtin_shufflevector(x2, x3, 2, 10, 3, 11, 6, 14, 7, 15);

    Float256 out0 = __builtin_shufflevector(m0, m2, 0, 1, 8, 9, 4, 5, 12, 13);
    Float256 out1 = __builtin_shufflevector(m0, m2, 2, 3, 10, 11, 6, 7, 14, 15);
    Float256 out2 = __builtin_shufflevector(m1, m3, 0, 1, 8, 9, 4, 5, 12, 13);
    Float256 out3 = __builtin_shufflevector(m1, m3, 2, 3, 10, 11, 6, 7, 14, 15);

    return {out0, out1, out2, out3};
}

static inline float ReduceAdd(Float128 x) noexcept {
    return x[0] + x[1] + x[2] + x[3];
}
static inline float ReduceAdd(Float256 x) noexcept {
    return x[0] + x[1] + x[2] + x[3] + x[4] + x[5] + x[6] + x[7];
}

}
