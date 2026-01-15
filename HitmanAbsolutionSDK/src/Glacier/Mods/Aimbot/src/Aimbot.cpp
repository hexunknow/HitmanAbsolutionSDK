#include <imgui.h>
#include <imgui_internal.h>
#include <IconsMaterialDesign.h>
#include <cmath>
#include <Windows.h>

#include <Glacier/ZLevelManager.h>
#include <Glacier/Actor/ZActorManager.h>
#include <Glacier/Player/ZHitman5.h>
#include <Glacier/Camera/ZHM5MainCamera.h>
#include <Glacier/Camera/ZCameraEntity.h>
#include <Glacier/Render/ZSpatialEntity.h>
#include <Glacier/Physics/ZCollisionManager.h>
#include <Glacier/Physics/ZRayQueryInput.h>
#include <Glacier/Physics/ZRayQueryOutput.h>
#include <Glacier/Physics/ERayDetailLevel.h>
#include <Glacier/Engine/ZApplicationEngineWin32.h>
#include <Glacier/ZGlobalBoneRegistry.h>

#include <Aimbot.h>
#include <Global.h>
#include <Hooks.h>

#define PI 3.14159265358979323846f

Aimbot::Aimbot()
{
    isOpen = false;
    isAimbotEnabled = false;
    maxDistance = 50.0f;
    fovLimit = 10.0f;
    ignoreWalls = false;
    aimSmoothness = 0.3f;
    targetGuards = true;
    targetCivilians = false;
    showESP = false;
    showFOVCircle = true;
    targetingMode = 1;
    aimPosition = 0;
    autoSwitchTarget = true;
    currentTarget = nullptr;
    frameCounter = 0;
    targetUpdateInterval = 1;
    hasLastPosition = false;
    lastTargetPosition.x = 0.0f;
    lastTargetPosition.y = 0.0f;
    lastTargetPosition.z = 0.0f;
    lastTargetPosition.w = 0.0f;
    targetVelocity.x = 0.0f;
    targetVelocity.y = 0.0f;
    targetVelocity.z = 0.0f;
    targetVelocity.w = 0.0f;
    
    toggleAction = ZInputAction("ToggleAimbot");
}

Aimbot::~Aimbot()
{
    const ZMemberDelegate<Aimbot, void(const SGameUpdateEvent&)> delegate(this, &Aimbot::OnFrameUpdate);
    GameLoopManager->UnregisterForFrameUpdate(delegate);
}

void Aimbot::Initialize()
{
    ModInterface::Initialize();
}

void Aimbot::OnEngineInitialized()
{
    const ZMemberDelegate<Aimbot, void(const SGameUpdateEvent&)> delegate(this, &Aimbot::OnFrameUpdate);
    GameLoopManager->RegisterForFrameUpdate(delegate, 1);
    
    AddBindings();
}

void Aimbot::OnDrawMenu()
{
    if (ImGui::Button(ICON_MD_MY_LOCATION " Aimbot"))
    {
        isOpen = !isOpen;
    }
}

void Aimbot::OnDrawUI(const bool hasFocus)
{
    if (showFOVCircle && isAimbotEnabled)
    {
        DrawFOVCircle();
    }
    
    if (!hasFocus || !isOpen)
    {
        return;
    }
    
    ImGui::PushFont(SDK::GetInstance().GetBoldFont());
    ImGui::SetNextWindowSize(ImVec2(450, 550), ImGuiCond_FirstUseEver);
    
    const bool isWindowVisible = ImGui::Begin(ICON_MD_MY_LOCATION " Aimbot", &isOpen);
    
    ImGui::PushFont(SDK::GetInstance().GetRegularFont());
    
    if (isWindowVisible)
    {
        ImGui::Checkbox("Enable Aimbot", &isAimbotEnabled);
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Hold RIGHT MOUSE BUTTON to aim");
        ImGui::Separator();
        
        ImGui::Text("Targeting Mode");
        ImGui::RadioButton("Distance-based", &targetingMode, 0);
        ImGui::SameLine();
        ImGui::RadioButton("FOV-based", &targetingMode, 1);
        
        ImGui::Separator();
        ImGui::SliderFloat("FOV Limit", &fovLimit, 10.0f, 60.0f, "%.0f");
        ImGui::SliderFloat("Max Distance", &maxDistance, 10.0f, 100.0f, "%.1f m");
        
        ImGui::Separator();
        ImGui::Text("Target Types");
        ImGui::Checkbox("Target Guards", &targetGuards);
        ImGui::Checkbox("Target Civilians", &targetCivilians);
        
        ImGui::Separator();
        ImGui::Text("Aim Position");
        ImGui::RadioButton("Head", &aimPosition, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Chest", &aimPosition, 1);
        
        ImGui::Separator();
        ImGui::SliderFloat("Aim Speed", &aimSmoothness, 0.0f, 1.0f, "%.2f");
        ImGui::TextWrapped("0.0 = Instant snap, 1.0 = Very slow");
        ImGui::TextWrapped("Recommended: 0.2-0.4 for smooth aiming");
        
        ImGui::Separator();
        ImGui::Checkbox("Auto Switch Target", &autoSwitchTarget);
        
        ImGui::Separator();
        ImGui::Checkbox("Show FOV Circle (Semi Correct)", &showFOVCircle);
        
        ImGui::Separator();
        ImGui::Checkbox("Ignore Walls", &ignoreWalls);
        
        ImGui::Separator();
        if (currentTarget)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "LOCKED: %s", currentTarget->GetActorName().ToCString());
        }
        else
        {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No target");
        }
    }
    
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopFont();
}

void Aimbot::DrawFOVCircle()
{
    ImGuiIO& io = ImGui::GetIO();
    float screenWidth = io.DisplaySize.x;
    float screenHeight = io.DisplaySize.y;
    
    if (screenWidth <= 0.0f || screenHeight <= 0.0f)
        return;
    
    float centerX = screenWidth / 2.0f;
    float centerY = screenHeight / 2.0f;
    
    float cameraFOV = 60.0f;
    float aspectRatio = screenWidth / screenHeight;
    ZHitman5* hitman = LevelManager->GetHitman().GetRawPointer();
    if (hitman)
    {
        ZHM5MainCamera* camera = hitman->GetMainCamera();
        if (camera)
        {
            cameraFOV = camera->GetFovYDeg();
            if (cameraFOV <= 0.0f || !isfinite(cameraFOV))
                cameraFOV = 60.0f;
            
            float camAspect = camera->GetAspectWByH();
            if (camAspect > 0.0f && isfinite(camAspect))
                aspectRatio = camAspect;
        }
    }
    
    float aimbotFOV = fovLimit;
    if (aimbotFOV < 10.0f) aimbotFOV = 10.0f;
    if (aimbotFOV > 60.0f) aimbotFOV = 60.0f;
    
    float aimbotFOVRad = (aimbotFOV * PI) / 180.0f;
    float cameraFOVRad = (cameraFOV * PI) / 180.0f;
    
    if (aimbotFOVRad >= (PI / 2.0f) - 0.01f)
        aimbotFOVRad = (PI / 2.0f) - 0.01f;
    if (cameraFOVRad >= (PI / 2.0f) - 0.01f)
        cameraFOVRad = (PI / 2.0f) - 0.01f;
    if (aimbotFOVRad < 0.001f)
        aimbotFOVRad = 0.001f;
    if (cameraFOVRad < 0.001f)
        cameraFOVRad = 0.001f;
    
    float tanAimbotHalf = tanf(aimbotFOVRad / 2.0f);
    float tanCameraHalf = tanf(cameraFOVRad / 2.0f);
    
    if (tanCameraHalf <= 0.0f || !isfinite(tanCameraHalf))
        tanCameraHalf = tanf((60.0f * PI / 180.0f) / 2.0f);
    if (tanAimbotHalf <= 0.0f || !isfinite(tanAimbotHalf))
        tanAimbotHalf = tanf((10.0f * PI / 180.0f) / 2.0f);
    
    float radius = (tanAimbotHalf / tanCameraHalf) * (screenHeight / 2.0f);
    
    float screenDiagonal = sqrtf(screenWidth * screenWidth + screenHeight * screenHeight);
    float maxRadius = screenDiagonal / 2.0f;
    
    if (radius > maxRadius) 
        radius = maxRadius;
    if (radius < 5.0f) 
        radius = 5.0f;
    if (!isfinite(radius)) 
        radius = 50.0f;
    
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    if (!drawList)
        return;
    
    ImU32 circleColor = IM_COL32(255, 255, 255, 180);
    drawList->AddCircle(ImVec2(centerX, centerY), radius, circleColor, 64, 2.0f);
    
    float crossSize = 10.0f;
    drawList->AddLine(ImVec2(centerX - crossSize, centerY), ImVec2(centerX + crossSize, centerY), circleColor, 2.0f);
    drawList->AddLine(ImVec2(centerX, centerY - crossSize), ImVec2(centerX, centerY + crossSize), circleColor, 2.0f);
}

void Aimbot::OnDraw3D()
{
}

void Aimbot::OnFrameUpdate(const SGameUpdateEvent& updateEvent)
{
    if (toggleAction.Digital())
    {
        isAimbotEnabled = !isAimbotEnabled;
    }
    
    if (!isAimbotEnabled)
    {
        currentTarget = nullptr;
        hasLastPosition = false;
        return;
    }
    
    bool rmb = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    
    if (!rmb)
    {
        currentTarget = nullptr;
        hasLastPosition = false;
        return;
    }
    
    ZHitman5* hitman = LevelManager->GetHitman().GetRawPointer();
    
    if (!hitman || hitman->IsDead())
    {
        currentTarget = nullptr;
        hasLastPosition = false;
        return;
    }
    
    ZActor* previousTarget = currentTarget;
    if (currentTarget)
    {
        if (currentTarget->IsDead() || !IsTargetValid(currentTarget))
            {
                currentTarget = nullptr;
                hasLastPosition = false;
            }
        }
    
    if (!currentTarget)
        {
            currentTarget = FindBestTarget();
            if (currentTarget != previousTarget)
            {
                hasLastPosition = false;
        }
    }
    
    if (currentTarget && !currentTarget->IsDead())
    {
        ApplyAimToTarget(currentTarget);
    }
}

ZActor* Aimbot::FindBestTarget()
{
    ZHitman5* hitman = LevelManager->GetHitman().GetRawPointer();
    if (!hitman) return nullptr;
    
    ZHM5MainCamera* camera = hitman->GetMainCamera();
    if (!camera) return nullptr;
    
    TArrayRef<TEntityRef<ZActor>> actors = ActorManager->GetAliveActors();
    
    float4 camPos = camera->GetWorldPosition();
    SMatrix camMatrix = camera->GetObjectToWorldMatrix();
    
    float4 camForward;
    camForward.x = -camMatrix.ZAxis.x;
    camForward.y = -camMatrix.ZAxis.y;
    camForward.z = -camMatrix.ZAxis.z;
    camForward.w = 0.0f;
    
    float fwdLen = sqrtf(camForward.x * camForward.x + camForward.y * camForward.y + camForward.z * camForward.z);
    if (fwdLen > 0.001f)
    {
        camForward.x /= fwdLen;
        camForward.y /= fwdLen;
        camForward.z /= fwdLen;
    }
    else
    {
        camForward.x = -camMatrix.YAxis.x;
        camForward.y = -camMatrix.YAxis.y;
        camForward.z = -camMatrix.YAxis.z;
        camForward.w = 0.0f;
        fwdLen = sqrtf(camForward.x * camForward.x + camForward.y * camForward.y + camForward.z * camForward.z);
        if (fwdLen > 0.001f)
        {
            camForward.x /= fwdLen;
            camForward.y /= fwdLen;
            camForward.z /= fwdLen;
        }
    }
    
    ZActor* best = nullptr;
    float bestScore = FLT_MAX;
    
    for (size_t i = 0; i < actors.Size(); ++i)
    {
        ZActor* actor = actors[i].GetRawPointer();
        
        if (!actor || actor->IsDead() || !IsTargetValid(actor))
            continue;
        
        float4 actorPos;
        
        if (aimPosition == 0)
        {
            if (GlobalBoneRegistry)
            {
                int headBoneID = GlobalBoneRegistry->GetBoneID("Head");
                if (headBoneID >= 0)
                {
                    actorPos = actor->GetBoneWorldPosition(headBoneID);
                }
                else
                {
                    SMatrix obbMatrix;
                    float4 obbSize;
                    actor->GetCharacterOBB(obbMatrix, obbSize);
                    float4 obbCenter = obbMatrix.Trans;
                    float fullHeight = obbSize.z * 2.0f;
                    actorPos.x = obbCenter.x;
                    actorPos.y = obbCenter.y;
                    actorPos.z = obbCenter.z + (fullHeight * 0.4f);
                    actorPos.w = 0.0f;
                }
            }
            else
            {
                SMatrix obbMatrix;
                float4 obbSize;
                actor->GetCharacterOBB(obbMatrix, obbSize);
                float4 obbCenter = obbMatrix.Trans;
                float fullHeight = obbSize.z * 2.0f;
                actorPos.x = obbCenter.x;
                actorPos.y = obbCenter.y;
                actorPos.z = obbCenter.z + (fullHeight * 0.4f);
                actorPos.w = 0.0f;
            }
        }
        else
        {
            if (GlobalBoneRegistry)
            {
                int chestBoneID = GlobalBoneRegistry->GetBoneID("Spine1");
                
                if (chestBoneID >= 0)
                {
                    actorPos = actor->GetBoneWorldPosition(chestBoneID);
                }
                else
                {
                    SMatrix obbMatrix;
                    float4 obbSize;
                    actor->GetCharacterOBB(obbMatrix, obbSize);
                    float4 obbCenter = obbMatrix.Trans;
                    float fullHeight = obbSize.z * 2.0f;
                    actorPos.x = obbCenter.x;
                    actorPos.y = obbCenter.y;
                    actorPos.z = obbCenter.z + (fullHeight * 0.2f);
                    actorPos.w = 0.0f;
                }
            }
            else
            {
                SMatrix obbMatrix;
                float4 obbSize;
                actor->GetCharacterOBB(obbMatrix, obbSize);
                float4 obbCenter = obbMatrix.Trans;
                float fullHeight = obbSize.z * 2.0f;
                actorPos.x = obbCenter.x;
                actorPos.y = obbCenter.y;
                actorPos.z = obbCenter.z + (fullHeight * 0.2f);
                actorPos.w = 0.0f;
            }
        }
        
        float dist = CalculateDistance(camPos, actorPos);
        if (dist > maxDistance)
            continue;
        
        float angle = CalculateFOVAngle(camPos, camForward, actorPos);
        
        float safeFOVLimit = fovLimit;
        if (safeFOVLimit < 5.0f) safeFOVLimit = 5.0f;
        if (safeFOVLimit > 60.0f) safeFOVLimit = 60.0f;
        
        if (angle > safeFOVLimit)
            continue;
        
        if (!ignoreWalls && !IsTargetVisible(camPos, actorPos, actor))
                continue;
        
        float score = (targetingMode == 0) ? dist : angle;
        
        if (score < bestScore)
        {
            bestScore = score;
            best = actor;
        }
    }
    
    return best;
}

bool Aimbot::IsTargetVisible(const float4& from, const float4& to, ZActor* targetActor)
{
    if (!CollisionManager)
        return true;
    
    ZRayQueryInput rayInput(from, to, RAYDETAILS_MESH);
    ZRayQueryOutput rayOutput{};
    
    if (!CollisionManager->RayCastClosestHit(rayInput, &rayOutput))
        return true;
    
    ZEntityRef blocking = rayOutput.GetBlockingEntity();
    
    if (!blocking.GetEntityTypePtrPtr())
        return true;
    
    ZActor* blockingActor = blocking.QueryInterfacePtr<ZActor>();
    
    return (blockingActor == targetActor);
}

void Aimbot::ApplyAimToTarget(ZActor* target)
{
    if (!target) return;
    
    ZHitman5* hitman = LevelManager->GetHitman().GetRawPointer();
    if (!hitman) return;
    
    ZHM5MainCamera* camera = hitman->GetMainCamera();
    if (!camera) return;
    
    float4 camPos = camera->GetWorldPosition();
    
    float4 targetPos;
    
    if (aimPosition == 0)
    {
        if (GlobalBoneRegistry)
        {
            int headBoneID = GlobalBoneRegistry->GetBoneID("Head");
            if (headBoneID >= 0)
            {
                targetPos = target->GetBoneWorldPosition(headBoneID);
            }
            else
            {
                SMatrix obbMatrix;
                float4 obbSize;
                target->GetCharacterOBB(obbMatrix, obbSize);
                float4 obbCenter = obbMatrix.Trans;
                float fullHeight = obbSize.z * 2.0f;
                targetPos.x = obbCenter.x;
                targetPos.y = obbCenter.y;
                targetPos.z = obbCenter.z + (fullHeight * 0.4f);
                targetPos.w = 0.0f;
            }
        }
        else
        {
            SMatrix obbMatrix;
            float4 obbSize;
            target->GetCharacterOBB(obbMatrix, obbSize);
            float4 obbCenter = obbMatrix.Trans;
            float fullHeight = obbSize.z * 2.0f;
            targetPos.x = obbCenter.x;
            targetPos.y = obbCenter.y;
            targetPos.z = obbCenter.z + (fullHeight * 0.4f);
            targetPos.w = 0.0f;
        }
    }
    else
    {
        if (GlobalBoneRegistry)
        {
            int chestBoneID = GlobalBoneRegistry->GetBoneID("Spine1");
            
            if (chestBoneID >= 0)
            {
                targetPos = target->GetBoneWorldPosition(chestBoneID);
            }
            else
            {
                SMatrix obbMatrix;
                float4 obbSize;
                target->GetCharacterOBB(obbMatrix, obbSize);
                float4 obbCenter = obbMatrix.Trans;
                float fullHeight = obbSize.z * 2.0f;
                targetPos.x = obbCenter.x;
                targetPos.y = obbCenter.y;
                targetPos.z = obbCenter.z + (fullHeight * 0.2f);
                targetPos.w = 0.0f;
            }
        }
        else
        {
            SMatrix obbMatrix;
            float4 obbSize;
            target->GetCharacterOBB(obbMatrix, obbSize);
            float4 obbCenter = obbMatrix.Trans;
            float fullHeight = obbSize.z * 2.0f;
            targetPos.x = obbCenter.x;
            targetPos.y = obbCenter.y;
            targetPos.z = obbCenter.z + (fullHeight * 0.2f);
            targetPos.w = 0.0f;
        }
    }
    
    const float deltaTime = 0.016f;
    if (hasLastPosition)
    {
        float4 velocity;
        velocity.x = (targetPos.x - lastTargetPosition.x) / deltaTime;
        velocity.y = (targetPos.y - lastTargetPosition.y) / deltaTime;
        velocity.z = (targetPos.z - lastTargetPosition.z) / deltaTime;
        velocity.w = 0.0f;
        
        float velocitySmoothing = 0.7f;
        targetVelocity.x = targetVelocity.x * velocitySmoothing + velocity.x * (1.0f - velocitySmoothing);
        targetVelocity.y = targetVelocity.y * velocitySmoothing + velocity.y * (1.0f - velocitySmoothing);
        targetVelocity.z = targetVelocity.z * velocitySmoothing + velocity.z * (1.0f - velocitySmoothing);
        
        float predictionTime = 0.05f;
        targetPos.x += targetVelocity.x * predictionTime;
        targetPos.y += targetVelocity.y * predictionTime;
        targetPos.z += targetVelocity.z * predictionTime;
    }
    else
    {
        targetVelocity.x = 0.0f;
        targetVelocity.y = 0.0f;
        targetVelocity.z = 0.0f;
    }
    
    lastTargetPosition = targetPos;
    hasLastPosition = true;
    
    float4 targetDirection;
    targetDirection.x = targetPos.x - camPos.x;
    targetDirection.y = targetPos.y - camPos.y;
    targetDirection.z = targetPos.z - camPos.z;
    targetDirection.w = 0.0f;
    
    float len = sqrtf(targetDirection.x * targetDirection.x + targetDirection.y * targetDirection.y + targetDirection.z * targetDirection.z);
    
    if (len < 0.01f) return;
    
    targetDirection.x /= len;
    targetDirection.y /= len;
    targetDirection.z /= len;
    
    SMatrix currentMatrix = camera->GetObjectToWorldMatrix();
    float4 currentForward;
    currentForward.x = -currentMatrix.ZAxis.x;
    currentForward.y = -currentMatrix.ZAxis.y;
    currentForward.z = -currentMatrix.ZAxis.z;
    currentForward.w = 0.0f;
    
    float currentLen = sqrtf(currentForward.x * currentForward.x + currentForward.y * currentForward.y + currentForward.z * currentForward.z);
    if (currentLen > 0.001f)
    {
        currentForward.x /= currentLen;
        currentForward.y /= currentLen;
        currentForward.z /= currentLen;
    }
    else
    {
        currentForward = targetDirection;
    }
    
    float distance = CalculateDistance(camPos, targetPos);
    float distanceFactor = 1.0f;
    if (distance > 0.0f)
    {
        distanceFactor = 1.0f - (distance / maxDistance) * 0.3f;
        if (distanceFactor < 0.7f) distanceFactor = 0.7f;
        if (distanceFactor > 1.0f) distanceFactor = 1.0f;
    }
    
    if (aimSmoothness > 0.0f)
    {
        float dot = currentForward.x * targetDirection.x + currentForward.y * targetDirection.y + currentForward.z * targetDirection.z;
        if (dot > 1.0f) dot = 1.0f;
        if (dot < -1.0f) dot = -1.0f;
        
        float angle = acosf(dot);
        
        float angleFactor = 1.0f;
        if (angle > 0.01f)
        {
            angleFactor = 1.0f + (angle * 2.0f);
            if (angleFactor > 2.0f) angleFactor = 2.0f;
        }
        
        float effectiveSmoothness = aimSmoothness * distanceFactor * angleFactor;
        if (effectiveSmoothness > 0.95f) effectiveSmoothness = 0.95f;
        
        float lerpFactor = 1.0f - effectiveSmoothness;
        
        float4 smoothedDirection;
        smoothedDirection.x = currentForward.x + (targetDirection.x - currentForward.x) * lerpFactor;
        smoothedDirection.y = currentForward.y + (targetDirection.y - currentForward.y) * lerpFactor;
        smoothedDirection.z = currentForward.z + (targetDirection.z - currentForward.z) * lerpFactor;
        smoothedDirection.w = 0.0f;
        
        len = sqrtf(smoothedDirection.x * smoothedDirection.x + smoothedDirection.y * smoothedDirection.y + smoothedDirection.z * smoothedDirection.z);
        if (len > 0.001f)
        {
            smoothedDirection.x /= len;
            smoothedDirection.y /= len;
            smoothedDirection.z /= len;
        }
        else
        {
            smoothedDirection = targetDirection;
        }
        
        camera->SetCameraDirection(smoothedDirection);
    }
    else
    {
        camera->SetCameraDirection(targetDirection);
    }
}

bool Aimbot::IsTargetValid(ZActor* actor)
{
    if (!actor) return false;
    
    EActorType type = *reinterpret_cast<EActorType*>(reinterpret_cast<uintptr_t>(actor) + 0x214);
    
    if (type == eAT_Guard && targetGuards)
        return true;
    
    if (type == eAT_Civilian && targetCivilians)
        return true;
    
    return false;
}

float Aimbot::CalculateDistance(const float4& p1, const float4& p2)
{
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    float dz = p2.z - p1.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

float Aimbot::CalculateFOVAngle(const float4& camPos, const float4& camFwd, const float4& targetPos)
{
    float4 toTarget;
    toTarget.x = targetPos.x - camPos.x;
    toTarget.y = targetPos.y - camPos.y;
    toTarget.z = targetPos.z - camPos.z;
    
    float len = sqrtf(toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z);
    
    if (len < 0.001f)
        return 180.0f;
    
    toTarget.x /= len;
    toTarget.y /= len;
    toTarget.z /= len;
    
    float dot = camFwd.x * toTarget.x + camFwd.y * toTarget.y + camFwd.z * toTarget.z;
    
    if (dot > 1.0f) dot = 1.0f;
    if (dot < -1.0f) dot = -1.0f;
    
    float angle = acosf(dot) * (180.0f / PI);
    
    if (!isfinite(angle) || angle < 0.0f || angle > 180.0f)
        return 180.0f;
    
    return angle;
}


DEFINE_MOD(Aimbot);
