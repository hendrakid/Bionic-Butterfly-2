#include "ButterflyController.h"

#include <RadioProtocol.h>
#include <cassert>
#include <cmath>
#include <limits>

namespace {
bool near(float a, float b, float tolerance = 0.001F) { return std::fabs(a - b) < tolerance; }
Butterfly::MotionConfig config(Butterfly::Waveform waveform = Butterfly::Waveform::Sine) {
    return {-30.0F, 40.0F, 10.0F, 1.0F, 0.0F, waveform};
}

void testFrequencyAndZeroHold() {
    ButterflyController control(config());
    assert(control.begin(10));
    assert(control.update(true, 1000, 1500, 20));
    assert(near(control.frequencyHz(), 0.0F));
    assert(near(control.anglesDeg()[0], 0.0F) && near(control.anglesDeg()[1], 0.0F));
    assert(control.update(true, 1500, 1500, 120));
    assert(near(control.frequencyHz(), 2.5F));
    assert(control.update(true, 2000, 1500, 220));
    assert(near(control.frequencyHz(), 5.0F));
}

void testOppositePhaseDifferentialAndContinuity() {
    ButterflyController control(config());
    assert(control.begin(0));
    assert(control.update(true, 1500, 2000, 0));
    assert(control.update(true, 1500, 2000, 100));
    assert(near(control.phaseCycles(), 0.25F));
    assert(near(control.anglesDeg()[0], 50.0F));
    assert(near(control.anglesDeg()[1], -20.0F));
    const float phase = control.phaseCycles();
    assert(control.update(true, 2000, 1000, 100));
    assert(near(control.phaseCycles(), phase));
    assert(near(control.differentialDeg(), -10.0F));
}

void testTriangleAndInvalidInput() {
    ButterflyController control(config(Butterfly::Waveform::Triangle));
    assert(control.begin(0));
    assert(control.update(true, 1500, 1500, 0));
    assert(control.update(true, 1500, 1500, 100));
    assert(near(control.anglesDeg()[0], 40.0F));
    assert(near(control.anglesDeg()[1], -30.0F));
    assert(!control.update(true, 999, 1500, 101));
    assert(near(control.anglesDeg()[0], 0.0F) && near(control.anglesDeg()[1], 0.0F));
    auto invalid = config();
    invalid.upperDeg = std::numeric_limits<float>::quiet_NaN();
    ButterflyController failed(invalid);
    assert(!failed.begin(0) && !failed.update(true, 1500, 1500, 100));
    invalid = config();
    invalid.differentialLimitDeg = 31.0F;
    assert(!Butterfly::validate(invalid));
}

void testRolloverDisarmGainTrimAndNegativeHalf() {
    auto c = config();
    c.steeringGain = 2.0F;
    c.steeringTrimDeg = 5.0F;
    ButterflyController control(c);
    const uint32_t start = std::numeric_limits<uint32_t>::max() - 10U;
    assert(control.begin(start));
    assert(control.update(true, 1500, 2000, start));
    assert(near(control.differentialDeg(), 10.0F));
    assert(control.update(true, 1500, 1500, start + 300U));
    assert(near(control.phaseCycles(), 0.75F));
    assert(near(control.differentialDeg(), 5.0F));
    assert(near(control.anglesDeg()[0], -35.0F));
    assert(near(control.anglesDeg()[1], 35.0F));
    assert(control.update(false, 1500, 2000, start + 301U));
    const float phase = control.phaseCycles();
    assert(control.update(true, 1000, 1500, start + 1000U));
    assert(near(control.phaseCycles(), phase));
    assert(near(control.anglesDeg()[0], 0.0F));
    assert(control.update(true, 2000, 1500, start + 1100U));
    assert(near(control.phaseCycles(), phase));
}
}

int main() {
    testFrequencyAndZeroHold();
    testOppositePhaseDifferentialAndContinuity();
    testTriangleAndInvalidInput();
    testRolloverDisarmGainTrimAndNegativeHalf();
}
