#include <imgui.h>
#include <IconsMaterialDesign.h>
#include <map>
#include <string>

#include <Glacier/ZLevelManager.h>
#include <Glacier/Actor/ZActorManager.h>
#include <Glacier/Player/ZHitman5.h>
#include <Glacier/Camera/ZHM5MainCamera.h>
#include <Glacier/Physics/ZCollisionManager.h>
#include <Glacier/Physics/ZRayQueryInput.h>
#include <Glacier/Physics/ZRayQueryOutput.h>
#include <Glacier/Physics/ERayDetailLevel.h>
#include <Wallhack.h>
#include <Global.h>
#include <SDK.h>
#include <Renderer/DirectXRenderer.h>

Wallhack::Wallhack()
{
    isOpen = false;
    enableESP = true;
    enableVisibleCheck = true;
    enableEnemies = true;
    enableCivilians = true;
}

Wallhack::~Wallhack()
{
    const ZMemberDelegate<Wallhack, void(const SGameUpdateEvent&)> delegate(this, &Wallhack::OnFrameUpdate);
    GameLoopManager->UnregisterForFrameUpdate(delegate);
}

void Wallhack::Initialize()
{
    ModInterface::Initialize();
}

void Wallhack::OnEngineInitialized()
{
    const ZMemberDelegate<Wallhack, void(const SGameUpdateEvent&)> delegate(this, &Wallhack::OnFrameUpdate);
    GameLoopManager->RegisterForFrameUpdate(delegate, 1);
}

void Wallhack::OnDrawMenu()
{
    if (ImGui::Button(ICON_MD_VISIBILITY " Wallhack"))
    {
        isOpen = !isOpen;
    }
}

void Wallhack::OnDrawUI(const bool hasFocus)
{
    if (!hasFocus || !isOpen)
    {
        return;
    }

    ImGui::PushFont(SDK::GetInstance().GetBoldFont());
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);

    const bool isWindowVisible = ImGui::Begin(ICON_MD_VISIBILITY " Wallhack", &isOpen);

    ImGui::PushFont(SDK::GetInstance().GetRegularFont());

    if (isWindowVisible)
    {
        ImGui::Checkbox("ESP", &enableESP);
        ImGui::Checkbox("Visible Check", &enableVisibleCheck);
        ImGui::Separator();
        ImGui::Checkbox("Enemies", &enableEnemies);
        ImGui::Checkbox("Civilians", &enableCivilians);
    }

    ImGui::PopFont();
    ImGui::End();
    ImGui::PopFont();
}

void Wallhack::OnDraw3D()
{
    if (!enableESP)
    {
        return;
    }

    ZHitman5* hitman = LevelManager->GetHitman().GetRawPointer();
    if (!hitman)
    {
        return;
    }

    ZHM5MainCamera* camera = hitman->GetMainCamera();
    if (!camera)
    {
        return;
    }

    float4 camPos = camera->GetWorldPosition();

    TArrayRef<TEntityRef<ZActor>> actors = ActorManager->GetAliveActors();

    for (size_t i = 0; i < actors.Size(); ++i)
    {
        ZActor* actor = actors[i].GetRawPointer();

        if (!actor || actor->IsDead())
        {
            continue;
        }

        EActorType type = *reinterpret_cast<EActorType*>(reinterpret_cast<uintptr_t>(actor) + 0x214);
        
        bool shouldDraw = false;
        if (type == eAT_Guard && enableEnemies)
        {
            shouldDraw = true;
        }
        else if (type == eAT_Civilian && enableCivilians)
        {
            shouldDraw = true;
        }

        if (shouldDraw && enableESP)
        {
            DrawESP(actor);
        }
    }
}

void Wallhack::DrawESP(ZActor* actor)
{
    if (!actor)
    {
        return;
    }

    SDK& sdk = SDK::GetInstance();
    ZHitman5* hitman = LevelManager->GetHitman().GetRawPointer();
    if (!hitman)
    {
        return;
    }

    ZHM5MainCamera* camera = hitman->GetMainCamera();
    if (!camera)
    {
        return;
    }

    float4 camPos = camera->GetWorldPosition();

    SMatrix obbMatrix;
    float4 obbSize;
    actor->GetCharacterOBB(obbMatrix, obbSize);

    float halfWidth = obbSize.x;
    float halfDepth = obbSize.y;
    float halfHeight = obbSize.z;

    SVector3 min(-halfWidth, -halfDepth, -halfHeight);
    SVector3 max(halfWidth, halfDepth, halfHeight);

    SVector4 color;
    if (enableVisibleCheck)
    {
        float4 actorPos = obbMatrix.Trans;
        bool isVisible = IsTargetVisible(camPos, actorPos, actor);
        if (isVisible)
        {
            color = SVector4(0.0f, 1.0f, 0.0f, 1.0f); // Green
        }
        else
        {
            color = SVector4(1.0f, 0.0f, 0.0f, 1.0f); // Red
        }
    }
    else
    {
        color = SVector4(1.0f, 1.0f, 1.0f, 1.0f); // White
    }

    sdk.GetDirectXRenderer()->DrawOBB3D(min, max, obbMatrix, color);
}

bool Wallhack::IsTargetVisible(const float4& from, const float4& to, ZActor* targetActor)
{
    if (!CollisionManager)
    {
        return true;
    }

    ZRayQueryInput rayInput(from, to, RAYDETAILS_MESH);
    ZRayQueryOutput rayOutput{};

    if (!CollisionManager->RayCastClosestHit(rayInput, &rayOutput))
    {
        return true;
    }

    ZEntityRef blocking = rayOutput.GetBlockingEntity();

    if (!blocking.GetEntityTypePtrPtr())
    {
        return true;
    }

    ZActor* blockingActor = blocking.QueryInterfacePtr<ZActor>();

    return (blockingActor == targetActor);
}

void Wallhack::OnFrameUpdate(const SGameUpdateEvent& updateEvent)
{
}

DEFINE_MOD(Wallhack);
