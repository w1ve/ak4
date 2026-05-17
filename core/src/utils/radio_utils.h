/**
 * @file radio_utils.h
 * @brief Shared utility functions for frequency, band, and span math.
 *
 * Pure functions with no side effects — portable across all platforms.
 */

#ifndef QK4_RADIO_UTILS_H
#define QK4_RADIO_UTILS_H

#include <cstdint>
#include <string>
#include <vector>

namespace qk4 {
namespace RadioUtils {

/* --------------------------------------------------------------------------
 * Band definitions
 * -------------------------------------------------------------------------- */

struct BandInfo {
    int32_t id;
    const char* name;       // e.g., "20m"
    int64_t lower_hz;       // Lower band edge
    int64_t upper_hz;       // Upper band edge
};

/**
 * Get all supported amateur bands (160m through 6m).
 */
const std::vector<BandInfo>& getAllBands();

/**
 * Get band info by ID. Returns nullptr if not found.
 */
const BandInfo* getBandById(int32_t band_id);

/**
 * Determine which band a frequency falls in.
 * @return Band ID, or -1 if frequency is not in any amateur band.
 */
int32_t freqToBand(int64_t freq_hz);

/**
 * Check if a frequency is within a specific band's limits.
 */
bool isFreqInBand(int64_t freq_hz, int32_t band_id);

/* --------------------------------------------------------------------------
 * Frequency formatting
 * -------------------------------------------------------------------------- */

/**
 * Format a frequency in Hz to period-separated display string.
 * e.g., 14225000 → "14.225.000"
 */
std::string formatFrequency(int64_t freq_hz);

/**
 * Parse a period-separated frequency string back to Hz.
 * e.g., "14.225.000" → 14225000
 * @return Frequency in Hz, or -1 on parse error.
 */
int64_t parseFrequency(const std::string& str);

/**
 * Parse a kHz entry (as used in direct frequency entry keypad).
 * e.g., "14225" → 14225000 Hz
 * @return Frequency in Hz, or -1 on parse error.
 */
int64_t parseFrequencyKhz(const std::string& str);

/* --------------------------------------------------------------------------
 * Tuning rates
 * -------------------------------------------------------------------------- */

/**
 * Get the ordered list of supported tuning rates in Hz.
 * {1, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000}
 */
const std::vector<int32_t>& getTuningRates();

/**
 * Get the next tuning rate in the cycle (wraps from 10000 back to 1).
 */
int32_t nextTuningRate(int32_t current_rate);

/**
 * Get the previous tuning rate in the cycle (wraps from 1 back to 10000).
 */
int32_t prevTuningRate(int32_t current_rate);

/* --------------------------------------------------------------------------
 * Span tiers
 * -------------------------------------------------------------------------- */

/**
 * Get the ordered list of K4 supported span values in Hz.
 */
const std::vector<int32_t>& getSpanTiers();

/**
 * Snap a span value to the nearest supported tier.
 */
int32_t snapToSpanTier(int32_t span_hz);

/**
 * Get the next wider span tier. Returns current if already at max.
 */
int32_t nextSpanTier(int32_t current_span_hz);

/**
 * Get the next narrower span tier. Returns current if already at min.
 */
int32_t prevSpanTier(int32_t current_span_hz);

/* --------------------------------------------------------------------------
 * S-Meter
 * -------------------------------------------------------------------------- */

/**
 * Convert raw S-meter value (0-120) to display string.
 * 0-54 → S0-S9, 54+ → S9+N dB (in 10dB steps)
 */
std::string smeterToString(int32_t raw_value);

/**
 * Convert raw S-meter value to S-unit (0-9) and dB over S9.
 * @param raw_value  Input (0-120)
 * @param s_unit     Output S-unit (0-9)
 * @param db_over    Output dB over S9 (0, 10, 20, 30, 40, 50, 60)
 */
void smeterToUnits(int32_t raw_value, int32_t& s_unit, int32_t& db_over);

/* --------------------------------------------------------------------------
 * Spectrum helpers
 * -------------------------------------------------------------------------- */

/**
 * Map a pixel X coordinate to a frequency given center and span.
 * @param x_pixel     Touch X position
 * @param width       Display width in pixels
 * @param center_hz   Center frequency
 * @param span_hz     Displayed span
 * @return Frequency at that pixel position
 */
int64_t pixelToFreq(int x_pixel, int width, int64_t center_hz, int32_t span_hz);

/**
 * Map a frequency to a pixel X coordinate.
 */
int freqToPixel(int64_t freq_hz, int width, int64_t center_hz, int32_t span_hz);

/**
 * Clamp a center frequency so the displayed range stays within band limits.
 */
int64_t clampCenter(int64_t center_hz, int32_t span_hz, int64_t band_lower, int64_t band_upper);

} // namespace RadioUtils
} // namespace qk4

#endif // QK4_RADIO_UTILS_H
