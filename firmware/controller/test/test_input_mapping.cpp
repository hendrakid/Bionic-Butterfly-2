#include "input/ArmButton.h"
#include "input/InputManager.h"
#include "control/ChannelManager.h"
#include "config.h"
#include <RadioProtocol.h>

#include <cassert>
#include <cstdint>

namespace {
uint16_t readings[4]{};

uint16_t readAnalog(uint8_t pin) {
    switch (pin) {
    case 32: return readings[0];
    case 33: return readings[1];
    case 34: return readings[2];
    case 35: return readings[3];
    default: return 0;
    }
}

void testMovingAverageAndSchedule() {
    InputManager input(readAnalog);
    readings[0] = 100; readings[1] = 200; readings[2] = 300; readings[3] = 400;
    assert(input.update(0));
    assert(!input.ready());
    assert(!input.update(4));
    readings[0] = 200; readings[1] = 300; readings[2] = 400; readings[3] = 500;
    assert(input.update(5));
    auto snapshot = input.rawSnapshot();
    assert(snapshot.leftX == 150 && snapshot.leftY == 250);
    assert(snapshot.rightX == 350 && snapshot.rightY == 450);

    readings[0] = 300; input.update(10);
    readings[0] = 400; input.update(15);
    assert(input.ready());
    readings[0] = 500; input.update(20);
    assert(input.rawSnapshot().leftX == 350);
    readings[0] = 600;
    assert(input.update(1000));
    assert(!input.update(1000));
    assert(input.rawSnapshot().leftX == 450);

    InputManager wrapped(readAnalog);
    assert(wrapped.update(UINT32_MAX - 2U));
    assert(!wrapped.update(1));
    assert(wrapped.update(2));
    InputManager missingReader(nullptr);
    assert(!missingReader.update(0));
    assert(!missingReader.ready());
    assert(missingReader.rawSnapshot().leftY == 0);
}

void testButtonDebounceAndPressLength() {
    ArmButton button;
    assert(!button.update(0, false));
    assert(!button.update(10, true));
    assert(!button.update(15, false));
    assert(!button.update(30, true));
    assert(!button.update(49, true));
    assert(!button.update(50, true));
    assert(!button.update(100, false));
    assert(button.update(120, false));

    assert(!button.update(200, true));
    assert(!button.update(220, true));
    assert(!button.update(719, false));
    assert(!button.update(739, false));
}

void testPressBoundaryAndTimerWrap() {
    ArmButton button;
    assert(!button.update(UINT32_MAX - 30U, false));
    assert(!button.update(UINT32_MAX - 10U, true));
    assert(!button.update(9, true));
    assert(!button.update(10, true));
    assert(!button.update(488, false));
    assert(button.update(508, false));
    assert(!button.update(508, false));

    assert(!button.update(600, true));
    assert(!button.update(620, true));
    assert(!button.update(1100, false));
    assert(!button.update(1120, false));
}

void testHeldAtBootRequiresRelease() {
    ArmButton button;
    assert(!button.update(0, true));
    assert(!button.update(100, false));
    assert(!button.update(120, false));
    assert(!button.update(200, true));
    assert(!button.update(220, true));
    assert(!button.update(250, false));
    assert(button.update(270, false));
}

void assertReservedCentered(const uint16_t channels[12]) {
    assert(channels[2] == RadioProtocol::CHANNEL_CENTER);
    assert(channels[3] == RadioProtocol::CHANNEL_CENTER);
    for (uint8_t index = 5; index < 12; ++index) {
        assert(channels[index] == RadioProtocol::CHANNEL_CENTER);
    }
}

void testMappingIncreasingAndClamps() {
    const ChannelManager::Calibration calibration{100, 1100, 200, 700, 1200, true};
    uint16_t channels[12]{};
    assert(ChannelManager::map(100, 700, calibration, channels, true));
    assert(channels[0] == 1000 && channels[1] == 1500 && channels[4] == 2000);
    assertReservedCentered(channels);

    assert(ChannelManager::map(50, 100, calibration, channels, false));
    assert(channels[0] == 1000 && channels[1] == 1000 && channels[4] == 1000);
    assert(ChannelManager::map(1200, 1300, calibration, channels, false));
    assert(channels[0] == 2000 && channels[1] == 2000);
    assert(ChannelManager::map(150, 724, calibration, channels, false));
    assert(channels[0] == 1000 && channels[1] == 1500);
    assert(ChannelManager::map(151, 726, calibration, channels, false));
    assert(channels[0] > 1000 && channels[1] > 1500);
    assert(ChannelManager::map(150, 676, calibration, channels, false));
    assert(channels[0] == 1000 && channels[1] == 1500);
    assert(ChannelManager::map(150, 674, calibration, channels, false));
    assert(channels[1] < 1500);
}

void testConfiguredCalibrationMapping() {
    const auto calibration = ControllerConfig::inputCalibration();
    uint16_t channels[12]{};
    assert(ChannelManager::map(4095, 4095, calibration, channels, false));
    assert(channels[0] == 1000 && channels[1] == 1000 && channels[4] == 1000);
    assertReservedCentered(channels);
    assert(ChannelManager::map(0, 0, calibration, channels, true));
    assert(channels[0] == 2000 && channels[1] == 2000 && channels[4] == 2000);
    for (uint16_t neutral = 1888; neutral <= 1904; ++neutral) {
        assert(ChannelManager::map(4095, neutral, calibration, channels, false));
        assert(channels[0] == 1000 && channels[1] == 1500);
    }

    const ChannelManager::Calibration defaults{};
    assert(!ChannelManager::map(0, 0, defaults, channels, true));
    assert(channels[0] == 1000 && channels[1] == 1500 && channels[4] == 1000);
    assertReservedCentered(channels);
}

void testMappingReversedAndInvalidCalibration() {
    const ChannelManager::Calibration reversed{1100, 100, 1200, 700, 200, true};
    uint16_t channels[12]{};
    assert(ChannelManager::map(100, 1200, reversed, channels, true));
    assert(channels[0] == 2000 && channels[1] == 1000 && channels[4] == 2000);
    assert(ChannelManager::map(1100, 200, reversed, channels, false));
    assert(channels[0] == 1000 && channels[1] == 2000);

    const ChannelManager::Calibration unmeasured{100, 1100, 200, 700, 1200, false};
    assert(!ChannelManager::map(1100, 1200, unmeasured, channels, true));
    assert(channels[0] == 1000 && channels[1] == 1500 && channels[4] == 1000);
    assertReservedCentered(channels);

    const ChannelManager::Calibration badCenter{100, 1100, 200, 1300, 1200, true};
    assert(!ChannelManager::map(500, 500, badCenter, channels, true));
    assert(!ChannelManager::map(500, 500, reversed, nullptr, true));
    const ChannelManager::Calibration noTravel{100, 100, 200, 700, 1200, true};
    assert(!ChannelManager::map(100, 700, noTravel, channels, true));
    const ChannelManager::Calibration defaultCalibration{};
    assert(!ChannelManager::map(100, 700, defaultCalibration, channels, true));
}
}

int main() {
    testMovingAverageAndSchedule();
    testButtonDebounceAndPressLength();
    testPressBoundaryAndTimerWrap();
    testHeldAtBootRequiresRelease();
    testMappingIncreasingAndClamps();
    testConfiguredCalibrationMapping();
    testMappingReversedAndInvalidCalibration();
    return 0;
}
