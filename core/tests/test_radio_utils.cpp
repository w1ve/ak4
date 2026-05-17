/**
 * @file test_radio_utils.cpp
 * @brief Unit tests for RadioUtils — frequency, band, span, S-meter math.
 * Validates Properties 5, 9, 10, 11, 12, 13, 20, 22.
 */

#include <gtest/gtest.h>
#include "utils/radio_utils.h"

using namespace qk4::RadioUtils;

// --- Property 5: Frequency-Band Consistency ---

TEST(BandTest, FreqToBand_20m) {
    EXPECT_EQ(freqToBand(14225000), 5); // 20m
}

TEST(BandTest, FreqToBand_40m) {
    EXPECT_EQ(freqToBand(7040000), 3); // 40m
}

TEST(BandTest, FreqToBand_OutOfBand) {
    EXPECT_EQ(freqToBand(12000000), -1); // Not in any amateur band
}

TEST(BandTest, FreqToBand_BandEdges) {
    // Lower edge of 20m
    EXPECT_EQ(freqToBand(14000000), 5);
    // Upper edge of 20m
    EXPECT_EQ(freqToBand(14350000), 5);
    // Just below 20m
    EXPECT_EQ(freqToBand(13999999), -1);
    // Just above 20m
    EXPECT_EQ(freqToBand(14350001), -1);
}

TEST(BandTest, IsFreqInBand) {
    EXPECT_TRUE(isFreqInBand(14225000, 5));
    EXPECT_FALSE(isFreqInBand(14225000, 3));
    EXPECT_FALSE(isFreqInBand(14350001, 5));
}

TEST(BandTest, Consistency_AllBands) {
    // For every band, check that frequencies within it map back correctly
    for (const auto& band : getAllBands()) {
        int64_t mid = (band.lower_hz + band.upper_hz) / 2;
        EXPECT_EQ(freqToBand(mid), band.id) << "Band: " << band.name;
        EXPECT_EQ(freqToBand(band.lower_hz), band.id) << "Lower edge: " << band.name;
        EXPECT_EQ(freqToBand(band.upper_hz), band.id) << "Upper edge: " << band.name;
    }
}

// --- Property 9: Frequency Formatting Round-Trip ---

TEST(FreqFormatTest, Format_14225000) {
    EXPECT_EQ(formatFrequency(14225000), "14.225.000");
}

TEST(FreqFormatTest, Format_7040000) {
    EXPECT_EQ(formatFrequency(7040000), "7.040.000");
}

TEST(FreqFormatTest, Format_50125000) {
    EXPECT_EQ(formatFrequency(50125000), "50.125.000");
}

TEST(FreqFormatTest, Parse_14225000) {
    EXPECT_EQ(parseFrequency("14.225.000"), 14225000);
}

TEST(FreqFormatTest, Parse_7040000) {
    EXPECT_EQ(parseFrequency("7.040.000"), 7040000);
}

TEST(FreqFormatTest, RoundTrip) {
    std::vector<int64_t> freqs = {1800000, 3500000, 7040000, 14225000, 21300000, 28500000, 50125000};
    for (int64_t f : freqs) {
        std::string formatted = formatFrequency(f);
        int64_t parsed = parseFrequency(formatted);
        EXPECT_EQ(parsed, f) << "Failed for freq: " << f << " formatted: " << formatted;
    }
}

TEST(FreqFormatTest, ParseKhz) {
    EXPECT_EQ(parseFrequencyKhz("14225"), 14225000);
    EXPECT_EQ(parseFrequencyKhz("7040"), 7040000);
    EXPECT_EQ(parseFrequencyKhz("14225.5"), 14225500);
}

TEST(FreqFormatTest, ParseInvalid) {
    EXPECT_EQ(parseFrequency(""), -1);
    EXPECT_EQ(parseFrequency("abc"), -1);
    EXPECT_EQ(parseFrequencyKhz(""), -1);
    EXPECT_EQ(parseFrequencyKhz("xyz"), -1);
}

// --- Property 10: S-Meter Value Mapping ---

TEST(SmeterTest, S0) {
    EXPECT_EQ(smeterToString(0), "S0");
}

TEST(SmeterTest, S9) {
    EXPECT_EQ(smeterToString(54), "S9");
}

TEST(SmeterTest, S9Plus20) {
    EXPECT_EQ(smeterToString(74), "S9+20");
}

TEST(SmeterTest, S9Plus60) {
    EXPECT_EQ(smeterToString(114), "S9+60");
}

TEST(SmeterTest, Monotonic) {
    // Property 10: mapping is monotonically non-decreasing
    int32_t prev_s = 0, prev_db = 0;
    for (int32_t raw = 0; raw <= 120; ++raw) {
        int32_t s_unit, db_over;
        smeterToUnits(raw, s_unit, db_over);

        int32_t composite = s_unit * 100 + db_over;
        int32_t prev_composite = prev_s * 100 + prev_db;
        EXPECT_GE(composite, prev_composite) << "Non-monotonic at raw=" << raw;

        prev_s = s_unit;
        prev_db = db_over;
    }
}

// --- Property 20: Tuning Rate Cycling ---

TEST(TuningRateTest, CycleForward) {
    EXPECT_EQ(nextTuningRate(1), 10);
    EXPECT_EQ(nextTuningRate(10), 20);
    EXPECT_EQ(nextTuningRate(100), 200);
    EXPECT_EQ(nextTuningRate(10000), 1); // Wrap
}

TEST(TuningRateTest, CycleBackward) {
    EXPECT_EQ(prevTuningRate(1), 10000); // Wrap
    EXPECT_EQ(prevTuningRate(10), 1);
    EXPECT_EQ(prevTuningRate(10000), 5000);
}

TEST(TuningRateTest, InvalidRate_ReturnsFirst) {
    EXPECT_EQ(nextTuningRate(999), 1); // Not in list
}

// --- Property 11: Touch-to-Frequency Mapping ---

TEST(SpectrumTest, PixelToFreq_Center) {
    // Center pixel should map to center frequency
    int64_t freq = pixelToFreq(500, 1000, 14225000, 100000);
    EXPECT_EQ(freq, 14225000);
}

TEST(SpectrumTest, PixelToFreq_LeftEdge) {
    int64_t freq = pixelToFreq(0, 1000, 14225000, 100000);
    EXPECT_EQ(freq, 14175000); // center - span/2
}

TEST(SpectrumTest, PixelToFreq_RightEdge) {
    int64_t freq = pixelToFreq(1000, 1000, 14225000, 100000);
    EXPECT_EQ(freq, 14275000); // center + span/2
}

TEST(SpectrumTest, FreqToPixel_RoundTrip) {
    int64_t center = 14225000;
    int32_t span = 100000;
    int width = 1000;

    for (int x = 0; x <= width; x += 50) {
        int64_t freq = pixelToFreq(x, width, center, span);
        int px = freqToPixel(freq, width, center, span);
        EXPECT_NEAR(px, x, 1) << "Failed at x=" << x;
    }
}

// --- Property 13: Spectrum Scroll Band Clamping ---

TEST(SpectrumTest, ClampCenter_WithinBand) {
    int64_t clamped = clampCenter(14225000, 100000, 14000000, 14350000);
    EXPECT_EQ(clamped, 14225000); // No change needed
}

TEST(SpectrumTest, ClampCenter_TooLow) {
    // center - span/2 would go below band lower
    int64_t clamped = clampCenter(14020000, 100000, 14000000, 14350000);
    EXPECT_EQ(clamped, 14050000); // band_lower + span/2
}

TEST(SpectrumTest, ClampCenter_TooHigh) {
    int64_t clamped = clampCenter(14330000, 100000, 14000000, 14350000);
    EXPECT_EQ(clamped, 14300000); // band_upper - span/2
}

// --- Property 12: Span Tier Snapping ---

TEST(SpanTest, SnapToTier) {
    EXPECT_EQ(snapToSpanTier(6000), 5000);   // Closer to 5000 than 7000
    EXPECT_EQ(snapToSpanTier(6500), 7000);   // Closer to 7000
    EXPECT_EQ(snapToSpanTier(100000), 100000); // Exact match
}

TEST(SpanTest, NextTier) {
    EXPECT_EQ(nextSpanTier(5000), 7000);     // WHY: K4 skips 6kHz
    EXPECT_EQ(nextSpanTier(100000), 150000);
    EXPECT_EQ(nextSpanTier(500000), 500000); // Already at max
}

TEST(SpanTest, PrevTier) {
    EXPECT_EQ(prevSpanTier(7000), 5000);     // WHY: K4 skips 6kHz
    EXPECT_EQ(prevSpanTier(2000), 2000);     // Already at min
}

// --- Property 22: Frequency Range Validation ---

TEST(ValidationTest, FreqInBand_Valid) {
    EXPECT_TRUE(isFreqInBand(14225000, 5));  // 20m
    EXPECT_TRUE(isFreqInBand(14000000, 5));  // Lower edge
    EXPECT_TRUE(isFreqInBand(14350000, 5));  // Upper edge
}

TEST(ValidationTest, FreqInBand_Invalid) {
    EXPECT_FALSE(isFreqInBand(13999999, 5)); // Below 20m
    EXPECT_FALSE(isFreqInBand(14350001, 5)); // Above 20m
    EXPECT_FALSE(isFreqInBand(7040000, 5));  // Wrong band
}
