#include "coord_converter.h"
#include <iostream>
#include <iomanip>

namespace ctrl {

void CoordConverter::dump() const
{
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "════════ CoordConverter ════════\n";
    std::cout << "  Camera:     " << camWidth() << "x" << camHeight()
              << " @" << camFps() << "fps\n";
    std::cout << "  uCenter:    " << uCenter << " px\n";
    std::cout << "  alphaX:     " << alphaX << " cm/px\n";
    std::cout << "  pulses/deg: " << pulsesPerDeg << "\n";
    std::cout << "  pipeLen:    " << pipeLen << " cm\n";
    std::cout << "  hingeH:     " << hingeH << " cm\n";
    std::cout << "  gravity:    " << gravity << " cm/s^2\n";
    std::cout << "  beta:       " << beta << "\n";
    std::cout << "  plant K:    " << computePlantGain()
              << " px/s^2/cm\n";
    std::cout << "════════════════════════════════\n";
}

} // namespace ctrl
