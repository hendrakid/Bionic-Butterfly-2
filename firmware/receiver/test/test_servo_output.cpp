#include "ServoOutput.h"

#include <cassert>
#include <cmath>
#include <limits>

class FakeDriver final : public Butterfly::ServoDriver {
public:
    bool attach(uint8_t pin, uint32_t, uint8_t) override {
        ++attachCount;
        return !failAttach || pin != failPin;
    }
    bool writeDuty(uint8_t pin, uint32_t duty) override {
        ++writeCount;
        lastPin = pin;
        lastDuty = duty;
        duties[pin == 4 ? 0 : 1] = duty;
        const double pulse = duty * 20000.0 / 16384.0;
        assert(pulse >= (pin == 4 ? 1000.0 : 1100.0));
        assert(pulse <= (pin == 4 ? 2100.0 : 2050.0));
        return !failWrite;
    }
    void detach(uint8_t) override { ++detachCount; }
    int attachCount = 0;
    int writeCount = 0;
    int detachCount = 0;
    bool failAttach = false;
    bool failWrite = false;
    uint8_t failPin = 0;
    uint8_t lastPin = 0;
    uint32_t lastDuty = 0;
    uint32_t duties[2]{};
};

namespace {
Butterfly::ServoConfig validConfig() {
    Butterfly::ServoConfig c{};
    c.commissioningEnabled = true;
    c.measurementsConfirmed = true;
    c.updateIntervalMs = 20;
    c.servos[0] = {-30.0F, 40.0F, 1000, 1500, 2100, 5.0F, -20.0F, false, 100.0F};
    c.servos[1] = {-25.0F, 35.0F, 1100, 1550, 2050, -3.0F, 25.0F, true, 50.0F};
    return c;
}

void testClosedGatePerformsNoIo() {
    FakeDriver driver;
    Butterfly::ServoConfig config{};
    Butterfly::ServoOutput output(driver, config);
    assert(!output.commissioned());
    assert(!output.begin(4, 5, 0));
    assert(driver.attachCount == 0 && driver.writeCount == 0);
    config = validConfig();
    config.commissioningEnabled = false;
    Butterfly::ServoOutput closed(driver, config);
    assert(closed.configurationValid() && !closed.begin(4, 5, 0));
    config.commissioningEnabled = true;
    config.measurementsConfirmed = false;
    Butterfly::ServoOutput unmeasured(driver, config);
    assert(!unmeasured.begin(4, 5, 0));
    assert(unmeasured.faulted());
    assert(driver.attachCount == 0 && driver.writeCount == 0);
}

void testSafeInitialPulseRampAndClamp() {
    FakeDriver driver;
    auto config = validConfig();
    Butterfly::ServoOutput output(driver, config);
    assert(output.begin(4, 5, 100));
    assert(output.ready() && driver.attachCount == 2 && driver.writeCount == 2);
    assert(output.lastCommandedAnglesDeg()[0] == -20.0F);
    assert(output.lastCommandedAnglesDeg()[1] == 25.0F);
    assert(!output.begin(4, 5, 100));
    assert(driver.attachCount == 2);
    const float target[2] = {100.0F, 100.0F};
    assert(output.update(true, target, 119));
    assert(driver.writeCount == 2);
    assert(output.update(true, target, 120));
    assert(output.lastCommandedAnglesDeg()[0] == 40.0F);
    assert(output.lastCommandedAnglesDeg()[1] == -25.0F); // reversed then clamped
    const float ignored[2] = {0.0F, 0.0F};
    // A delayed loop must not turn the safe ramp into an unbounded jump.
    assert(output.update(false, ignored, 1120));
    assert(output.lastCommandedAnglesDeg()[0] == 38.0F);
    assert(output.lastCommandedAnglesDeg()[1] == -24.0F);
    for (unsigned i = 0; i < 60; ++i) {
        assert(output.update(false, nullptr, 1140 + i * 20));
    }
    assert(output.lastCommandedAnglesDeg()[0] == -20.0F);
    assert(output.lastCommandedAnglesDeg()[1] == 25.0F);
}

void testInitFailureLatchesFault() {
    FakeDriver driver;
    driver.failAttach = true;
    driver.failPin = 5;
    Butterfly::ServoOutput output(driver, validConfig());
    assert(!output.begin(4, 5, 0));
    assert(output.faulted() && !output.ready());
    assert(driver.writeCount == 0 && driver.detachCount == 1);
    assert(!output.begin(4, 5, 20));
    assert(driver.attachCount == 2);
    FakeDriver initialWriteFailure;
    initialWriteFailure.failWrite = true;
    Butterfly::ServoOutput failed(initialWriteFailure, validConfig());
    assert(!failed.begin(4, 5, 0));
    assert(!failed.ready() && failed.faulted() && initialWriteFailure.detachCount == 2);
}

void testMappingDirectionTrimAndValidation() {
    auto config = validConfig();
    assert(Butterfly::validate(config));
    assert(Butterfly::physicalAngle(config.servos[0], 10.0F) == 15.0F);
    assert(Butterfly::physicalAngle(config.servos[1], 10.0F) == -13.0F);
    assert(Butterfly::angleToPulse(config.servos[0], -30.0F) == 1000);
    assert(Butterfly::angleToPulse(config.servos[0], 40.0F) == 2100);
    config.servos[0].safeAngleDeg = 100.0F;
    assert(!Butterfly::validate(config));
    config = validConfig();
    config.servos[1].slewDegPerSecond = std::numeric_limits<float>::infinity();
    assert(!Butterfly::validate(config));
    config = validConfig();
    config.servos[0].centerPulseUs = config.servos[0].pulseMinUs;
    assert(!Butterfly::validate(config));
    config = validConfig();
    config.servos[0].trimDeg = std::numeric_limits<float>::quiet_NaN();
    assert(!Butterfly::validate(config));
}

void testRolloverAndCommandFaultSafeRamp() {
    FakeDriver driver;
    Butterfly::ServoOutput output(driver, validConfig());
    const uint32_t start = std::numeric_limits<uint32_t>::max() - 10U;
    assert(output.begin(4, 5, start));
    const float target[2] = {0.0F, 0.0F};
    assert(output.update(true, target, start + 20U));
    assert(output.lastCommandedAnglesDeg()[0] == 5.0F);
    const float invalid[2] = {100.0F, std::numeric_limits<float>::quiet_NaN()};
    // Validate both commands before either channel can move toward an armed target.
    assert(!output.update(true, invalid, start + 40U));
    assert(output.faulted() && !output.ready());
    assert(output.lastCommandedAnglesDeg()[0] == 3.0F);
    assert(output.lastCommandedAnglesDeg()[1] == -2.0F);
    assert(!output.update(true, target, start + 60U));
    assert(output.lastCommandedAnglesDeg()[0] == 1.0F);
    driver.failWrite = true;
    assert(!output.update(false, nullptr, start + 80U));
    assert(driver.detachCount == 2);
    const int writes = driver.writeCount;
    assert(!output.update(false, nullptr, start + 100U));
    assert(driver.writeCount == writes);
}
}

int main() {
    testClosedGatePerformsNoIo();
    testSafeInitialPulseRampAndClamp();
    testInitFailureLatchesFault();
    testMappingDirectionTrimAndValidation();
    testRolloverAndCommandFaultSafeRamp();
}
