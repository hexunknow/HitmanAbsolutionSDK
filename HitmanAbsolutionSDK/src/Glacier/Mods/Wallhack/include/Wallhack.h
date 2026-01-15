#pragma once

#include <Glacier/ZGameLoopManager.h>
#include <Glacier/Actor/ZActor.h>

#include <ModInterface.h>

class ZActor;

class Wallhack : public ModInterface
{
public:
    Wallhack();
    ~Wallhack() override;
    void Initialize() override;
    void OnEngineInitialized() override;
    void OnDrawMenu() override;
    void OnDrawUI(const bool hasFocus) override;
    void OnDraw3D() override;

private:
    void OnFrameUpdate(const SGameUpdateEvent& updateEvent);
    
    bool IsTargetVisible(const float4& from, const float4& to, ZActor* targetActor);
    void DrawESP(ZActor* actor);
    
    bool isOpen;
    bool enableESP;
    bool enableVisibleCheck;
    bool enableEnemies;
    bool enableCivilians;
};

DECLARE_MOD(Wallhack)
