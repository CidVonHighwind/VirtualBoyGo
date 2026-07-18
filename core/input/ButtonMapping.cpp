#include "input/ButtonMapping.h"

namespace ButtonMapper
{

uint32_t TranslateToVBBitmask(const uint32_t buttonStates[3], const MappedButtons (&vbButtons)[16])
{
    uint32_t vbBits = 0;
    for (uint32_t vbBit = 0; vbBit < 16; ++vbBit)
    {
        for (const MappedButton &binding : vbButtons[vbBit].Buttons)
        {
            if (!binding.IsSet)
                continue;
            if (binding.InputDevice < 0 || binding.InputDevice > 2)
                continue;
            if (buttonStates[binding.InputDevice] & ButtonMapping[binding.ButtonIndex])
            {
                vbBits |= (1u << vbBit);
                break; // either binding is enough - no need to check the other
            }
        }
    }
    return vbBits;
}

} // namespace ButtonMapper
