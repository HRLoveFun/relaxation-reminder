#include "growth.h"

#include <algorithm>
#include <cmath>

namespace rr {

void GrowthCurve::Reset(double initial_side_px, double work_width_px,
                        double work_height_px, double seconds_to_cap) {
    initial_side_ = std::max(1.0, initial_side_px);

    const double work_width = std::max(1.0, work_width_px);
    const double work_height = std::max(1.0, work_height_px);

    // Cap area is 30% of the work area; the window stays square, so the cap
    // side is its square root, additionally clamped to the monitor.
    double cap = std::sqrt(0.30 * work_width * work_height);
    cap = std::min(cap, std::min(work_width, work_height));
    cap = std::max(cap, initial_side_);

    cap_side_ = cap;
    if (cap_side_ > initial_side_ && seconds_to_cap > 0.0) {
        // The side grows as exp(rate * t); the area therefore grows as its
        // square, reaching the cap at seconds_to_cap.
        growth_rate_ = std::log(cap_side_ / initial_side_) / seconds_to_cap;
    } else {
        growth_rate_ = 0.0;
    }
}

double GrowthCurve::SideAt(uint64_t elapsed_ms) const {
    if (growth_rate_ <= 0.0) {
        return initial_side_;
    }
    const double seconds = static_cast<double>(elapsed_ms) / 1000.0;
    const double side = initial_side_ * std::exp(growth_rate_ * seconds);
    return std::min(side, cap_side_);
}

}  // namespace rr
