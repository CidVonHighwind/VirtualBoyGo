#include "ButtonMapping.h"

namespace ButtonMapper
{

uint32_t TranslateToVBBitmask(const uint32_t buttonStates[3], const MappedButton (&vbButtons)[16])
{
    uint32_t vbBits = 0;
    for (uint32_t vbBit = 0; vbBit < 16; ++vbBit)
    {
        const MappedButton &binding = vbButtons[vbBit];
        if (!binding.IsSet)
            continue;
        if (binding.InputDevice < 0 || binding.InputDevice > 2)
            continue;
        if (buttonStates[binding.InputDevice] & ButtonMapping[binding.ButtonIndex])
            vbBits |= (1u << vbBit);
    }
    return vbBits;
}

} // namespace ButtonMapper
