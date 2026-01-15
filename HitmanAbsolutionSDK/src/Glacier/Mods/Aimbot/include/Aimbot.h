#pragma once

#include <Glacier/ZGameLoopManager.h>
#include <Glacier/Input/ZInputAction.h>
#include <Glacier/Actor/ZActor.h>

#include <ModInterface.h>

class ZActor;

class Aimbot : public ModInterface
{
public:
    Aimbot();
    ~Aimbot() override;
    void Initialize() override;
    void OnEngineInitialized() override;
    void OnDrawMenu() override;
    void OnDrawUI(const bool hasFocus) override;
    void OnDraw3D() override;

private:
    void OnFrameUpdate(const SGameUpdateEvent& updateEvent);
    
    // Core aimbot functions
    ZActor* FindBestTarget();
    bool IsTargetVisible(const float4& from, const float4& to, ZActor* targetActor);
    void ApplyAimToTarget(ZActor* target);
    bool IsTargetValid(ZActor* actor);
    float CalculateDistance(const float4& pos1, const float4& pos2);
    float CalculateFOVAngle(const float4& cameraPos, const float4& cameraForward, const float4& targetPos);
    void DrawFOVCircle();
    
    // UI window
    bool isOpen;
    
    // Aimbot settings
    bool isAimbotEnabled;
    float maxDistance;
    float fovLimit;
    bool ignoreWalls;
    float aimSmoothness;
    bool targetGuards;
    bool targetCivilians;
    bool showESP;
    bool showFOVCircle;
    
    // Targeting mode: 0 = Distance-based, 1 = FOV-based
    int targetingMode;
    
    // Aim position: 0 = Head, 1 = Chest, 2 = Auto
    int aimPosition;
    
    // Auto switch target when current dies
    bool autoSwitchTarget;
    
    // Current target
    ZActor* currentTarget;
    
    // Performance optimization
    int frameCounter;
    int targetUpdateInterval;
    
    // Target prediction for smooth tracking
    float4 lastTargetPosition;
    float4 targetVelocity;
    bool hasLastPosition;
    
    // Input action
    ZInputAction toggleAction;
};

DECLARE_MOD(Aimbot)
