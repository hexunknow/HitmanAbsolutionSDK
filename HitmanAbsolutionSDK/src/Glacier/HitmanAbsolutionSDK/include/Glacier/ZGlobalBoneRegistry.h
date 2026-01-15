#pragma once

#include "IComponentInterface.h"

class HitmanAbsolutionSDK_API ZGlobalBoneRegistry : public IComponentInterface
{
public:
    int GetBoneID(char const* pBoneName) const;
};
