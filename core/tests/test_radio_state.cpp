/**
 * @file test_radio_state.cpp
 * @brief Unit tests for RadioState model — setters, getters, CAT parsing, notifications.
 */

#include <gtest/gtest.h>
#include "models/radio_state.h"
#include <vector>
#include <string>

using namespace qk4;

class RadioStateTest : public ::testing::Test {
protected:
    RadioState state;
    std::vector<std::string> notifications;

    void SetUp() override {
        state.setChangeCallback([this](const std::string& key) {
            notifications.push_back(key);
        });
    }
};

// --- Basic getters/setters ---

TEST_F(RadioStateTest, DefaultState) {
    auto snap = state.snapshot();
    EXPECT_EQ(snap.vfo[0].frequency_hz, 14225000);
    EXPECT_EQ(snap.vfo[0].mode, 1); // USB
    EXPECT_TRUE(snap.vfo[0].active);
    EXPECT_FALSE(snap.vfo[1].active);
    EXPECT_FALSE(snap.tx.ptt);
}

TEST_F(RadioStateTest, SetFrequency) {
    state.setFrequency(0, 7040000);
    EXPECT_EQ(state.getVfo(0).frequency_hz, 7040000);
    ASSERT_EQ(notifications.size(), 1u);
    EXPECT_EQ(notifications[0], "vfo.a.freq");
}

TEST_F(RadioStateTest, SetFrequency_NoChangeNoNotify) {
    state.setFrequency(0, 14225000); // Same as default
    EXPECT_EQ(notifications.size(), 0u);
}

TEST_F(RadioStateTest, SetMode) {
    state.setMode(0, 2); // CW
    EXPECT_EQ(state.getVfo(0).mode, 2);
    ASSERT_EQ(notifications.size(), 1u);
    EXPECT_EQ(notifications[0], "vfo.a.mode");
}

TEST_F(RadioStateTest, SetSmeter) {
    state.setSmeter(0, 54); // S9
    EXPECT_EQ(state.getVfo(0).smeter, 54);
    EXPECT_EQ(notifications.back(), "vfo.a.smeter");
}

TEST_F(RadioStateTest, SetPtt) {
    state.setPtt(true);
    EXPECT_TRUE(state.getTx().ptt);
    EXPECT_EQ(notifications.back(), "tx.ptt");
}

TEST_F(RadioStateTest, SetPower) {
    state.setPower(50);
    EXPECT_EQ(state.getTx().power_watts, 50);
}

TEST_F(RadioStateTest, SetVolume_Clamped) {
    state.setMainVolume(150); // Should clamp to 100
    EXPECT_EQ(state.getAudio().main_volume, 100);

    state.setMainVolume(-10); // Should clamp to 0
    EXPECT_EQ(state.getAudio().main_volume, 0);
}

TEST_F(RadioStateTest, SetSidetonePitch_Clamped) {
    state.setSidetonePitch(200); // Below min, clamp to 400
    EXPECT_EQ(state.getAudio().sidetone_hz, 400);

    state.setSidetonePitch(1500); // Above max, clamp to 1000
    EXPECT_EQ(state.getAudio().sidetone_hz, 1000);

    state.setSidetonePitch(700); // Valid
    EXPECT_EQ(state.getAudio().sidetone_hz, 700);
}

TEST_F(RadioStateTest, SetNr) {
    state.setNrEnabled(0, 0, true); // MAIN, LMS, enabled
    EXPECT_TRUE(state.getNr(0).lms_enabled);

    state.setNrLevel(0, 0, 8);
    EXPECT_EQ(state.getNr(0).lms_level, 8);

    state.setNrLevel(0, 0, 15); // Clamp to 10
    EXPECT_EQ(state.getNr(0).lms_level, 10);
}

TEST_F(RadioStateTest, SetCwSpeed_Clamped) {
    state.setCwSpeed(3); // Below min, clamp to 5
    EXPECT_EQ(state.getCw().speed_wpm, 5);

    state.setCwSpeed(70); // Above max, clamp to 60
    EXPECT_EQ(state.getCw().speed_wpm, 60);

    state.setCwSpeed(25); // Valid
    EXPECT_EQ(state.getCw().speed_wpm, 25);
}

TEST_F(RadioStateTest, SetActiveVfo) {
    state.setActiveVfo(1);
    auto snap = state.snapshot();
    EXPECT_FALSE(snap.vfo[0].active);
    EXPECT_TRUE(snap.vfo[1].active);
}

// --- CAT command parsing ---

TEST_F(RadioStateTest, ParseCat_Frequency) {
    state.parseCatCommand("FA00014100000;");
    EXPECT_EQ(state.getVfo(0).frequency_hz, 14100000);
}

TEST_F(RadioStateTest, ParseCat_FrequencyB) {
    state.parseCatCommand("FB00007040000;");
    EXPECT_EQ(state.getVfo(1).frequency_hz, 7040000);
}

TEST_F(RadioStateTest, ParseCat_Mode_USB) {
    state.parseCatCommand("MD2;");
    EXPECT_EQ(state.getVfo(0).mode, 1); // USB = enum 1
}

TEST_F(RadioStateTest, ParseCat_Mode_CW) {
    state.parseCatCommand("MD3;");
    EXPECT_EQ(state.getVfo(0).mode, 2); // CW = enum 2
}

TEST_F(RadioStateTest, ParseCat_Smeter) {
    state.parseCatCommand("SM00054;");
    EXPECT_EQ(state.getVfo(0).smeter, 54);
}

TEST_F(RadioStateTest, ParseCat_TxState) {
    state.parseCatCommand("TQ1;");
    EXPECT_TRUE(state.getTx().ptt);

    state.parseCatCommand("TQ0;");
    EXPECT_FALSE(state.getTx().ptt);
}

TEST_F(RadioStateTest, ParseCat_Power) {
    state.parseCatCommand("PC050;");
    EXPECT_EQ(state.getTx().power_watts, 50);
}

TEST_F(RadioStateTest, ParseCat_InvalidCommand) {
    // Should not crash or change state
    auto before = state.snapshot();
    state.parseCatCommand("");
    state.parseCatCommand("X");
    state.parseCatCommand("ZZ999;");
    auto after = state.snapshot();

    EXPECT_EQ(before.vfo[0].frequency_hz, after.vfo[0].frequency_hz);
}

// --- Thread safety (basic) ---

TEST_F(RadioStateTest, Snapshot_IsConsistent) {
    state.setFrequency(0, 21300000);
    state.setMode(0, 4); // DATA
    state.setPtt(true);

    auto snap = state.snapshot();
    EXPECT_EQ(snap.vfo[0].frequency_hz, 21300000);
    EXPECT_EQ(snap.vfo[0].mode, 4);
    EXPECT_TRUE(snap.tx.ptt);
}
