/**
 * @file radio_utils.cpp
 * @brief Implementation of frequency, band, span, and S-meter utilities.
 */

#include "radio_utils.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace qk4 {
namespace RadioUtils {

/* --------------------------------------------------------------------------
 * Band definitions
 * -------------------------------------------------------------------------- */

static const std::vector<BandInfo> s_bands = {
    { 0, "160m",  1800000,   2000000},
    { 1, "80m",   3500000,   4000000},
    { 2, "60m",   5330500,   5403500},
    { 3, "40m",   7000000,   7300000},
    { 4, "30m",  10100000,  10150000},
    { 5, "20m",  14000000,  14350000},
    { 6, "17m",  18068000,  18168000},
    { 7, "15m",  21000000,  21450000},
    { 8, "12m",  24890000,  24990000},
    { 9, "10m",  28000000,  29700000},
    {10, "6m",   50000000,  54000000},
};

const std::vector<BandInfo>& getAllBands() {
    return s_bands;
}

const BandInfo* getBandById(int32_t band_id) {
    for (const auto& band : s_bands) {
        if (band.id == band_id) return &band;
    }
    return nullptr;
}

int32_t freqToBand(int64_t freq_hz) {
    for (const auto& band : s_bands) {
        if (freq_hz >= band.lower_hz && freq_hz <= band.upper_hz) {
            return band.id;
        }
    }
    return -1;
}

bool isFreqInBand(int64_t freq_hz, int32_t band_id) {
    const BandInfo* band = getBandById(band_id);
    if (!band) return false;
    return freq_hz >= band->lower_hz && freq_hz <= band->upper_hz;
}

/* --------------------------------------------------------------------------
 * Frequency formatting
 * -------------------------------------------------------------------------- */

std::string formatFrequency(int64_t freq_hz) {
    if (freq_hz < 0) freq_hz = 0;

    // Format as groups of 3 from the right, separated by periods
    // e.g., 14225000 → "14.225.000"
    std::string digits = std::to_string(freq_hz);

    // Pad to at least 7 digits for consistent display
    while (digits.size() < 7) {
        digits = "0" + digits;
    }

    std::string result;
    int len = static_cast<int>(digits.size());
    int groups = (len + 2) / 3; // Number of 3-digit groups

    for (int i = 0; i < len; ++i) {
        result += digits[i];
        int remaining = len - i - 1;
        if (remaining > 0 && remaining % 3 == 0) {
            result += '.';
        }
    }

    // Remove leading zeros but keep at least one digit before first period
    size_t firstDot = result.find('.');
    if (firstDot != std::string::npos) {
        size_t firstNonZero = result.find_first_not_of('0');
        if (firstNonZero != std::string::npos && firstNonZero < firstDot) {
            result = result.substr(firstNonZero);
        } else if (firstNonZero >= firstDot) {
            // All digits before dot are zero — keep one zero
            result = result.substr(firstDot - 1);
        }
    }

    return result;
}

int64_t parseFrequency(const std::string& str) {
    // Remove periods and parse as integer Hz
    std::string clean;
    for (char c : str) {
        if (c >= '0' && c <= '9') {
            clean += c;
        } else if (c != '.') {
            return -1; // Invalid character
        }
    }
    if (clean.empty()) return -1;

    try {
        return std::stoll(clean);
    } catch (...) {
        return -1;
    }
}

int64_t parseFrequencyKhz(const std::string& str) {
    // Parse as kHz, convert to Hz
    std::string clean;
    bool hasDot = false;
    for (char c : str) {
        if (c >= '0' && c <= '9') {
            clean += c;
        } else if (c == '.' && !hasDot) {
            clean += c;
            hasDot = true;
        } else {
            return -1;
        }
    }
    if (clean.empty()) return -1;

    try {
        double khz = std::stod(clean);
        return static_cast<int64_t>(khz * 1000.0 + 0.5);
    } catch (...) {
        return -1;
    }
}

/* --------------------------------------------------------------------------
 * Tuning rates
 * -------------------------------------------------------------------------- */

static const std::vector<int32_t> s_tuningRates = {
    1, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000
};

const std::vector<int32_t>& getTuningRates() {
    return s_tuningRates;
}

int32_t nextTuningRate(int32_t current_rate) {
    auto it = std::find(s_tuningRates.begin(), s_tuningRates.end(), current_rate);
    if (it == s_tuningRates.end()) return s_tuningRates[0];
    ++it;
    if (it == s_tuningRates.end()) return s_tuningRates[0]; // Wrap
    return *it;
}

int32_t prevTuningRate(int32_t current_rate) {
    auto it = std::find(s_tuningRates.begin(), s_tuningRates.end(), current_rate);
    if (it == s_tuningRates.end()) return s_tuningRates.back();
    if (it == s_tuningRates.begin()) return s_tuningRates.back(); // Wrap
    --it;
    return *it;
}

/* --------------------------------------------------------------------------
 * Span tiers
 * -------------------------------------------------------------------------- */

// WHY: K4 has a quirk where span steps from 5kHz to 7kHz (skipping 6kHz).
static const std::vector<int32_t> s_spanTiers = {
    2000, 3000, 4000, 5000, 7000, 10000, 15000, 20000, 30000,
    50000, 75000, 100000, 150000, 200000, 300000, 500000
};

const std::vector<int32_t>& getSpanTiers() {
    return s_spanTiers;
}

int32_t snapToSpanTier(int32_t span_hz) {
    // Find the closest tier
    int32_t closest = s_spanTiers[0];
    int32_t minDiff = std::abs(span_hz - closest);

    for (int32_t tier : s_spanTiers) {
        int32_t diff = std::abs(span_hz - tier);
        if (diff < minDiff) {
            minDiff = diff;
            closest = tier;
        }
    }
    return closest;
}

int32_t nextSpanTier(int32_t current_span_hz) {
    for (size_t i = 0; i < s_spanTiers.size() - 1; ++i) {
        if (s_spanTiers[i] >= current_span_hz) {
            return s_spanTiers[i + 1];
        }
    }
    return s_spanTiers.back();
}

int32_t prevSpanTier(int32_t current_span_hz) {
    for (size_t i = s_spanTiers.size() - 1; i > 0; --i) {
        if (s_spanTiers[i] <= current_span_hz) {
            return s_spanTiers[i - 1];
        }
    }
    return s_spanTiers[0];
}

/* --------------------------------------------------------------------------
 * S-Meter
 * -------------------------------------------------------------------------- */

std::string smeterToString(int32_t raw_value) {
    int32_t s_unit, db_over;
    smeterToUnits(raw_value, s_unit, db_over);

    if (db_over > 0) {
        return "S9+" + std::to_string(db_over);
    }
    return "S" + std::to_string(s_unit);
}

void smeterToUnits(int32_t raw_value, int32_t& s_unit, int32_t& db_over) {
    if (raw_value < 0) raw_value = 0;
    if (raw_value > 120) raw_value = 120;

    if (raw_value <= 54) {
        // S0-S9: each S-unit is 6 raw units
        s_unit = raw_value / 6;
        if (s_unit > 9) s_unit = 9;
        db_over = 0;
    } else {
        s_unit = 9;
        // Above S9: each 10dB is 10 raw units
        db_over = ((raw_value - 54) / 10) * 10;
        if (db_over > 60) db_over = 60;
    }
}

/* --------------------------------------------------------------------------
 * Spectrum helpers
 * -------------------------------------------------------------------------- */

int64_t pixelToFreq(int x_pixel, int width, int64_t center_hz, int32_t span_hz) {
    if (width <= 0) return center_hz;
    double ratio = static_cast<double>(x_pixel) / static_cast<double>(width);
    return center_hz - (span_hz / 2) + static_cast<int64_t>(ratio * span_hz);
}

int freqToPixel(int64_t freq_hz, int width, int64_t center_hz, int32_t span_hz) {
    if (span_hz <= 0) return width / 2;
    double offset = static_cast<double>(freq_hz - (center_hz - span_hz / 2));
    return static_cast<int>((offset / span_hz) * width);
}

int64_t clampCenter(int64_t center_hz, int32_t span_hz, int64_t band_lower, int64_t band_upper) {
    int64_t half_span = span_hz / 2;
    int64_t min_center = band_lower + half_span;
    int64_t max_center = band_upper - half_span;

    if (min_center > max_center) {
        // Band is narrower than span — center on band
        return (band_lower + band_upper) / 2;
    }

    if (center_hz < min_center) return min_center;
    if (center_hz > max_center) return max_center;
    return center_hz;
}

} // namespace RadioUtils
} // namespace qk4
