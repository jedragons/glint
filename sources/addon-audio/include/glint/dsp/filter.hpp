#pragma once

#include <cmath>

#include <algorithm>
#include <numbers>
#include <span>

namespace aw::dsp {

enum class FilterMode {
    LowPass = 0,
    HighPass = 1,
};

struct OnePoleFilter {
    FilterMode mode {};
    float integrator_gain {};
    float feedback_solve_gain {};
    float integrator_state {};

    inline auto reset(this OnePoleFilter& self) -> void {
        self.integrator_state = 0.0f;
    }

    inline auto set_cutoff(this OnePoleFilter& self, float cutoff_hz, float sample_rate) -> void {
        float cutoff = std::clamp(cutoff_hz / sample_rate, 0.0f, 0.497f);
        self.integrator_gain = std::tan(float(std::numbers::pi) * cutoff);
        self.feedback_solve_gain = 1.0f / (1.0f + self.integrator_gain);
    }

    inline auto process(this OnePoleFilter& self, std::span<float> buf) -> void {
        for (float& sample : buf) {
            sample = self.process_single(sample);
        }
    }

    inline auto process_single(this OnePoleFilter& self, float sample) -> float {
        float result = (self.integrator_gain * sample + self.integrator_state) * self.feedback_solve_gain;
        self.integrator_state = self.integrator_gain * (sample - result) + result;

        switch (self.mode) {
            case FilterMode::LowPass:
                return result;
            case FilterMode::HighPass:
                return sample - result;
            default:
                return 0.0f;
        }
    }
};

} // namespace aw::dsp
