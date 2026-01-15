#include "Glacier/ZGlobalBoneRegistry.h"

#include <Function.h>
#include <Global.h>

int ZGlobalBoneRegistry::GetBoneID(char const* pBoneName) const
{
    return Function::CallMethodAndReturn<int, const ZGlobalBoneRegistry*, char const*>(BaseAddress + 0x86990, this, pBoneName);
}
