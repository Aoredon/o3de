/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzToolsFramework/API/ViewportEditorModeTrackerNotificationBus.h>
#include <AzToolsFramework/ViewportSelection/ViewportEditorModeTracker.h>

namespace AzToolsFramework
{
    AZ::Outcome<void, AZStd::string> ViewportEditorModes::ActivateMode(ViewportEditorMode mode)
    {
        if (const AZ::u32 modeIndex = static_cast<AZ::u32>(mode);
            modeIndex < NumEditorModes)
        {
            m_editorModes[modeIndex] = true;
            return AZ::Success();
        }
        else
        {
            return AZ::Failure(
                AZStd::string::format("Cannot activate mode %u, mode is not recognized", modeIndex));
        }
    }

    AZ::Outcome<void, AZStd::string> ViewportEditorModes::DeactivateMode(ViewportEditorMode mode)
    {
        if (const AZ::u32 modeIndex = static_cast<AZ::u32>(mode); modeIndex < NumEditorModes)
        {
            m_editorModes[modeIndex] = false;
            return AZ::Success();
        }
        else
        {
            return AZ::Failure(
                AZStd::string::format("Cannot deactivate mode %u, mode is not recognized", modeIndex));
        }
    }

    bool ViewportEditorModes::IsModeActive(ViewportEditorMode mode) const
    {
        return m_editorModes[static_cast<AZ::u32>(mode)];
    }

    AZ::Outcome<void, AZStd::string> ViewportEditorModeTracker::ActivateMode(
        const ViewportEditorModeInfo& viewportEditorModeInfo, ViewportEditorMode mode)
    {
        auto& editorModes = m_viewportEditorModesMap[viewportEditorModeInfo.m_id];
        if (editorModes.IsModeActive(mode))
        {
            return AZ::Failure(AZStd::string::format(
                "Duplicate call to ActivateMode for mode '%u' on id '%i'", static_cast<AZ::u32>(mode), viewportEditorModeInfo.m_id));
        }
        
        if (const auto result = editorModes.ActivateMode(mode);
            !result.IsSuccess())
        {
            return result;
        }

        ViewportEditorModeNotificationsBus::Event(
            viewportEditorModeInfo.m_id, &ViewportEditorModeNotificationsBus::Events::OnEditorModeActivated, editorModes, mode);

        return AZ::Success();
    }

    AZ::Outcome<void, AZStd::string> ViewportEditorModeTracker::DeactivateMode(
        const ViewportEditorModeInfo& viewportEditorModeInfo, ViewportEditorMode mode)
    {
        ViewportEditorModes* editorModes = nullptr;
        bool modeWasActive = true;
        if (m_viewportEditorModesMap.count(viewportEditorModeInfo.m_id))
        {
            editorModes = &m_viewportEditorModesMap.at(viewportEditorModeInfo.m_id);
            if (!editorModes->IsModeActive(mode))
            {
                return AZ::Failure(AZStd::string::format(
                    "Duplicate call to DeactivateMode for mode '%u' on id '%i'", static_cast<AZ::u32>(mode), viewportEditorModeInfo.m_id));
            }
        }
        else
        {
            modeWasActive = false;
            editorModes = &m_viewportEditorModesMap[viewportEditorModeInfo.m_id];
        }

        if(const auto result = editorModes->DeactivateMode(mode);
            !result.IsSuccess())
        {
            return result;
        }

        ViewportEditorModeNotificationsBus::Event(
            viewportEditorModeInfo.m_id, &ViewportEditorModeNotificationsBus::Events::OnEditorModeDeactivated, *editorModes, mode);

        if (modeWasActive)
        {
            return AZ::Success();
        }
        else
        {
            return AZ::Failure(AZStd::string::format(
                "Call to DeactivateMode for mode '%u' on id '%i' without precursor call to ActivateMode", static_cast<AZ::u32>(mode),
                viewportEditorModeInfo.m_id));
        }
    }

    const ViewportEditorModesInterface* ViewportEditorModeTracker::GetViewportEditorModes(const ViewportEditorModeInfo& viewportEditorModeInfo) const
    {
        if (auto editorModes = m_viewportEditorModesMap.find(viewportEditorModeInfo.m_id);
            editorModes != m_viewportEditorModesMap.end())
        {
            return &editorModes->second;
        }
        else
        {
            return nullptr;
        }
    }

    size_t ViewportEditorModeTracker::GetTrackedViewportCount() const
    {
        return m_viewportEditorModesMap.size();
    }

    bool ViewportEditorModeTracker::IsViewportModeTracked(const ViewportEditorModeInfo& viewportEditorModeInfo) const
    {
        return m_viewportEditorModesMap.count(viewportEditorModeInfo.m_id) > 0;
    }
} // namespace AzToolsFramework
