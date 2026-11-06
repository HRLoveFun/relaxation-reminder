#pragma once

#include <cstdint>

namespace rr {

// Square window growth: A(t) = A0 * exp(k * t), capped at 30% of the
// monitor work area. The initial and cap sides are physical pixels.
struct GrowthCurve {
    void Reset(double initial_side_px, double work_width_px,
               double work_height_px, double seconds_to_cap = 60.0);

    double SideAt(uint64_t elapsed_ms) const;
    double initial_side() const { return initial_side_; }
    double cap_side() const { return cap_side_; }
    bool grows() const { return growth_rate_ > 0.0; }

private:
    double initial_side_ = 0.0;
    double cap_side_ = 0.0;
    double growth_rate_ = 0.0;  // exponent per second for the side
};

}  // namespace rr
