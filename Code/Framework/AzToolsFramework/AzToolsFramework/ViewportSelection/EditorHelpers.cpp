/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "EditorHelpers.h"

#include <AzCore/Console/Console.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzFramework/Viewport/CameraState.h>
#include <AzFramework/Viewport/ViewportScreen.h>
#include <AzFramework/Visibility/BoundsBus.h>
#include <AzToolsFramework/FocusMode/FocusModeInterface.h>
#include <AzToolsFramework/API/EditorViewportIconDisplayInterface.h>
#include <AzToolsFramework/ToolsComponents/EditorEntityIconComponentBus.h>
#include <AzToolsFramework/Viewport/ViewportMessages.h>
#include <AzToolsFramework/Viewport/ViewportTypes.h>
#include <AzToolsFramework/ViewportSelection/EditorSelectionUtil.h>
#include <AzToolsFramework/ViewportSelection/EditorVisibleEntityDataCache.h>

AZ_CVAR(
    bool,
    ed_visibility_showAggregateEntitySelectionBounds,
    false,
    nullptr,
    AZ::ConsoleFunctorFlags::Null,
    "Display the aggregate selection bounds for a given entity (the union of all component Aabbs)");
AZ_CVAR(
    bool,
    ed_visibility_showAggregateEntityTransformedLocalBounds,
    false,
    nullptr,
    AZ::ConsoleFunctorFlags::Null,
    "Display the aggregate transformed local bounds for a given entity (the union of all local component Aabbs)");
AZ_CVAR(
    bool,
    ed_visibility_showAggregateEntityWorldBounds,
    false,
    nullptr,
    AZ::ConsoleFunctorFlags::Null,
    "Display the aggregate world bounds for a given entity (the union of all world component Aabbs)");

namespace AzToolsFramework
{
    AZ_CLASS_ALLOCATOR_IMPL(EditorHelpers, AZ::SystemAllocator, 0)

    static const int s_iconSize = 36; // icon display size (in pixels)
    static const float s_iconMinScale = 0.1f; // minimum scale for icons in the distance
    static const float s_iconMaxScale = 1.0f; // maximum scale for icons near the camera
    static const float s_iconCloseDist = 3.f; // distance at which icons are at maximum scale
    static const float s_iconFarDist = 40.f; // distance at which icons are at minimum scale

    // helper function to wrap EBus call to check if helpers are being displayed
    // note: the ['?'] icon in the top right of the editor
    static bool HelpersVisible()
    {
        bool helpersVisible = false;
        EditorRequestBus::BroadcastResult(helpersVisible, &EditorRequests::DisplayHelpersVisible);
        return helpersVisible;
    }

    // calculate the icon scale based on how far away it is (distanceSq) from a given point
    // note: this is mostly likely distance from the camera
    static float GetIconScale(const float distSq)
    {
        AZ_PROFILE_FUNCTION(AzToolsFramework);

        return s_iconMinScale +
            (s_iconMaxScale - s_iconMinScale) *
            (1.0f - AZ::GetClamp(AZ::GetMax(0.0f, sqrtf(distSq) - s_iconCloseDist) / s_iconFarDist, 0.0f, 1.0f));
    }

    static void DisplayComponents(
        const AZ::EntityId entityId, const AzFramework::ViewportInfo& viewportInfo, AzFramework::DebugDisplayRequests& debugDisplay)
    {
        AZ_PROFILE_FUNCTION(AzToolsFramework);

        const AZ::Entity* entity = AZ::Interface<AZ::ComponentApplicationRequests>::Get()->FindEntity(entityId);
        AzFramework::EntityDebugDisplayEventBus::Event(
            entityId, &AzFramework::EntityDebugDisplayEvents::DisplayEntityViewport, viewportInfo, debugDisplay);

        if (ed_visibility_showAggregateEntitySelectionBounds)
        {
            if (const AZ::Aabb aabb = AzToolsFramework::CalculateEditorEntitySelectionBounds(entityId, viewportInfo); aabb.IsValid())
            {
                debugDisplay.SetColor(AZ::Colors::Orange);
                debugDisplay.DrawWireBox(aabb.GetMin(), aabb.GetMax());
            }
        }

        if (ed_visibility_showAggregateEntityTransformedLocalBounds)
        {
            AZ::Transform worldFromLocal = entity->GetTransform()->GetWorldTM();

            if (const AZ::Aabb localAabb = AzFramework::CalculateEntityLocalBoundsUnion(entity); localAabb.IsValid())
            {
                const AZ::Aabb worldAabb = localAabb.GetTransformedAabb(worldFromLocal);
                debugDisplay.SetColor(AZ::Colors::Turquoise);
                debugDisplay.DrawWireBox(worldAabb.GetMin(), worldAabb.GetMax());
            }
        }

        if (ed_visibility_showAggregateEntityWorldBounds)
        {
            if (const AZ::Aabb worldAabb = AzFramework::CalculateEntityWorldBoundsUnion(entity); worldAabb.IsValid())
            {
                debugDisplay.SetColor(AZ::Colors::Magenta);
                debugDisplay.DrawWireBox(worldAabb.GetMin(), worldAabb.GetMax());
            }
        }
    }

    EditorHelpers::EditorHelpers(const EditorVisibleEntityDataCache* entityDataCache)
        : m_entityDataCache(entityDataCache)
    {
        m_focusModeInterface = AZ::Interface<FocusModeInterface>::Get();
        AZ_Assert(
            m_focusModeInterface,
            "EditorHelpers - "
            "Focus Mode Interface could not be found. "
            "Check that it is being correctly initialized.");
    }

    AZ::EntityId EditorHelpers::HandleMouseInteraction(
        const AzFramework::CameraState& cameraState, const ViewportInteraction::MouseInteractionEvent& mouseInteraction)
    {
        AZ_PROFILE_FUNCTION(AzToolsFramework);

        const int viewportId = mouseInteraction.m_mouseInteraction.m_interactionId.m_viewportId;

        const bool helpersVisible = HelpersVisible();

        // selecting new entities
        AZ::EntityId entityIdUnderCursor;
        float closestDistance = std::numeric_limits<float>::max();
        for (size_t entityCacheIndex = 0; entityCacheIndex < m_entityDataCache->VisibleEntityDataCount(); ++entityCacheIndex)
        {
            const AZ::EntityId entityId = m_entityDataCache->GetVisibleEntityId(entityCacheIndex);

            if (m_entityDataCache->IsVisibleEntityLocked(entityCacheIndex) || !m_entityDataCache->IsVisibleEntityVisible(entityCacheIndex))
            {
                continue;
            }

            // 2d screen space selection - did we click an icon
            if (helpersVisible)
            {
                // some components choose to hide their icons (e.g. meshes)
                if (!m_entityDataCache->IsVisibleEntityIconHidden(entityCacheIndex))
                {
                    const AZ::Vector3& entityPosition = m_entityDataCache->GetVisibleEntityPosition(entityCacheIndex);

                    // selecting based on 2d icon - should only do it when visible and not selected
                    const AzFramework::ScreenPoint screenPosition = AzFramework::WorldToScreen(entityPosition, cameraState);

                    const float distSqFromCamera = cameraState.m_position.GetDistanceSq(entityPosition);
                    const auto iconRange = static_cast<float>(GetIconScale(distSqFromCamera) * s_iconSize * 0.5f);
                    const auto screenCoords = mouseInteraction.m_mouseInteraction.m_mousePick.m_screenCoordinates;

                    if (screenCoords.m_x >= screenPosition.m_x - iconRange && screenCoords.m_x <= screenPosition.m_x + iconRange &&
                        screenCoords.m_y >= screenPosition.m_y - iconRange && screenCoords.m_y <= screenPosition.m_y + iconRange)
                    {
                        entityIdUnderCursor = entityId;
                        break;
                    }
                }
            }

            using AzFramework::ViewportInfo;
            // check if components provide an aabb
            if (const AZ::Aabb aabb = CalculateEditorEntitySelectionBounds(entityId, ViewportInfo{ viewportId }); aabb.IsValid())
            {
                // coarse grain check
                if (AabbIntersectMouseRay(mouseInteraction.m_mouseInteraction, aabb))
                {
                    // if success, pick against specific component
                    if (PickEntity(entityId, mouseInteraction.m_mouseInteraction, closestDistance, viewportId))
                    {
                        entityIdUnderCursor = entityId;
                    }
                }
            }
        }

        // Verify if the entity Id corresponds to an entity that is focused; if not, halt selection.
        if (!m_focusModeInterface->IsInFocusSubTree(entityIdUnderCursor))
        {
            return AZ::EntityId();
        }

        return entityIdUnderCursor;
    }

    void EditorHelpers::DisplayHelpers(
        const AzFramework::ViewportInfo& viewportInfo,
        const AzFramework::CameraState& cameraState,
        AzFramework::DebugDisplayRequests& debugDisplay,
        const AZStd::function<bool(AZ::EntityId)>& showIconCheck)
    {
        AZ_PROFILE_FUNCTION(AzToolsFramework);

        if (HelpersVisible())
        {
            for (size_t entityCacheIndex = 0; entityCacheIndex < m_entityDataCache->VisibleEntityDataCount(); ++entityCacheIndex)
            {
                const AZ::EntityId entityId = m_entityDataCache->GetVisibleEntityId(entityCacheIndex);

                if (!m_entityDataCache->IsVisibleEntityVisible(entityCacheIndex))
                {
                    continue;
                }

                // notify components to display
                DisplayComponents(entityId, viewportInfo, debugDisplay);

                if (m_entityDataCache->IsVisibleEntityIconHidden(entityCacheIndex) ||
                    (m_entityDataCache->IsVisibleEntitySelected(entityCacheIndex) && !showIconCheck(entityId)))
                {
                    continue;
                }

                int iconTextureId = 0;
                EditorEntityIconComponentRequestBus::EventResult(
                    iconTextureId, entityId, &EditorEntityIconComponentRequests::GetEntityIconTextureId);

                const AZ::Vector3& entityPosition = m_entityDataCache->GetVisibleEntityPosition(entityCacheIndex);
                const float distSqFromCamera = cameraState.m_position.GetDistanceSq(entityPosition);

                const float iconScale = GetIconScale(distSqFromCamera);
                const float iconSize = s_iconSize * iconScale;

                using ComponentEntityAccentType = Components::EditorSelectionAccentSystemComponent::ComponentEntityAccentType;
                const AZ::Color iconHighlight = [this, entityCacheIndex]()
                {
                    if (m_entityDataCache->IsVisibleEntityLocked(entityCacheIndex))
                    {
                        return AZ::Color(AZ::u8(100), AZ::u8(100), AZ::u8(100), AZ::u8(255));
                    }

                    if (m_entityDataCache->GetVisibleEntityAccent(entityCacheIndex) == ComponentEntityAccentType::Hover)
                    {
                        return AZ::Color(AZ::u8(255), AZ::u8(120), AZ::u8(0), AZ::u8(204));
                    }

                    return AZ::Color(1.0f, 1.0f, 1.0f, 1.0f);
                }();

                EditorViewportIconDisplay::Get()->DrawIcon({ viewportInfo.m_viewportId, iconTextureId, iconHighlight, entityPosition,
                                                             EditorViewportIconDisplayInterface::CoordinateSpace::WorldSpace,
                                                             AZ::Vector2{ iconSize, iconSize } });
            }
        }
    }
} // namespace AzToolsFramework
