/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "EditorDefs.h"

#include "EditorPreferencesPageViewportCamera.h"

#include <AzCore/std/sort.h>
#include <AzFramework/Input/Buses/Requests/InputDeviceRequestBus.h>
#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <AzQtComponents/Components/StyleManager.h>
#include <EditorModularViewportCameraComposerBus.h>

// Editor
#include "EditorViewportSettings.h"
#include "Settings.h"

static AZStd::vector<AZStd::string> GetInputNamesByDevice(const AzFramework::InputDeviceId inputDeviceId)
{
    AzFramework::InputDeviceRequests::InputChannelIdSet availableInputChannelIds;
    AzFramework::InputDeviceRequestBus::Event(
        inputDeviceId, &AzFramework::InputDeviceRequests::GetInputChannelIds, availableInputChannelIds);

    AZStd::vector<AZStd::string> inputChannelNames;
    for (const AzFramework::InputChannelId& inputChannelId : availableInputChannelIds)
    {
        inputChannelNames.push_back(inputChannelId.GetName());
    }

    AZStd::sort(inputChannelNames.begin(), inputChannelNames.end());

    return inputChannelNames;
}

static AZStd::vector<AZStd::string> GetEditorInputNames()
{
    // function static to defer having to call GetInputNamesByDevice for every CameraInputSettings member
    static bool inputNamesGenerated = false;
    static AZStd::vector<AZStd::string> inputNames;

    if (!inputNamesGenerated)
    {
        AZStd::vector<AZStd::string> keyboardInputNames = GetInputNamesByDevice(AzFramework::InputDeviceKeyboard::Id);
        AZStd::vector<AZStd::string> mouseInputNames = GetInputNamesByDevice(AzFramework::InputDeviceMouse::Id);

        inputNames.insert(inputNames.end(), mouseInputNames.begin(), mouseInputNames.end());
        inputNames.insert(inputNames.end(), keyboardInputNames.begin(), keyboardInputNames.end());

        inputNamesGenerated = true;
    }

    return inputNames;
}

void CEditorPreferencesPage_ViewportCamera::Reflect(AZ::SerializeContext& serialize)
{
    serialize.Class<CameraMovementSettings>()
        ->Version(3)
        ->Field("TranslateSpeed", &CameraMovementSettings::m_translateSpeed)
        ->Field("RotateSpeed", &CameraMovementSettings::m_rotateSpeed)
        ->Field("BoostMultiplier", &CameraMovementSettings::m_boostMultiplier)
        ->Field("ScrollSpeed", &CameraMovementSettings::m_scrollSpeed)
        ->Field("DollySpeed", &CameraMovementSettings::m_dollySpeed)
        ->Field("PanSpeed", &CameraMovementSettings::m_panSpeed)
        ->Field("RotateSmoothing", &CameraMovementSettings::m_rotateSmoothing)
        ->Field("RotateSmoothness", &CameraMovementSettings::m_rotateSmoothness)
        ->Field("TranslateSmoothing", &CameraMovementSettings::m_translateSmoothing)
        ->Field("TranslateSmoothness", &CameraMovementSettings::m_translateSmoothness)
        ->Field("CaptureCursorLook", &CameraMovementSettings::m_captureCursorLook)
        ->Field("PivotYawRotationInverted", &CameraMovementSettings::m_pivotYawRotationInverted)
        ->Field("PanInvertedX", &CameraMovementSettings::m_panInvertedX)
        ->Field("PanInvertedY", &CameraMovementSettings::m_panInvertedY);

    serialize.Class<CameraInputSettings>()
        ->Version(2)
        ->Field("TranslateForward", &CameraInputSettings::m_translateForwardChannelId)
        ->Field("TranslateBackward", &CameraInputSettings::m_translateBackwardChannelId)
        ->Field("TranslateLeft", &CameraInputSettings::m_translateLeftChannelId)
        ->Field("TranslateRight", &CameraInputSettings::m_translateRightChannelId)
        ->Field("TranslateUp", &CameraInputSettings::m_translateUpChannelId)
        ->Field("TranslateDown", &CameraInputSettings::m_translateDownChannelId)
        ->Field("Boost", &CameraInputSettings::m_boostChannelId)
        ->Field("Pivot", &CameraInputSettings::m_pivotChannelId)
        ->Field("FreeLook", &CameraInputSettings::m_freeLookChannelId)
        ->Field("FreePan", &CameraInputSettings::m_freePanChannelId)
        ->Field("PivotLook", &CameraInputSettings::m_pivotLookChannelId)
        ->Field("PivotDolly", &CameraInputSettings::m_pivotDollyChannelId)
        ->Field("PivotPan", &CameraInputSettings::m_pivotPanChannelId);

    serialize.Class<CEditorPreferencesPage_ViewportCamera>()
        ->Version(1)
        ->Field("CameraMovementSettings", &CEditorPreferencesPage_ViewportCamera::m_cameraMovementSettings)
        ->Field("CameraInputSettings", &CEditorPreferencesPage_ViewportCamera::m_cameraInputSettings);

    if (AZ::EditContext* editContext = serialize.GetEditContext())
    {
        const float minValue = 0.0001f;
        editContext->Class<CameraMovementSettings>("Camera Movement Settings", "")
            ->DataElement(
                AZ::Edit::UIHandlers::SpinBox, &CameraMovementSettings::m_translateSpeed, "Camera Movement Speed", "Camera movement speed")
            ->Attribute(AZ::Edit::Attributes::Min, minValue)
            ->DataElement(
                AZ::Edit::UIHandlers::SpinBox, &CameraMovementSettings::m_rotateSpeed, "Camera Rotation Speed", "Camera rotation speed")
            ->Attribute(AZ::Edit::Attributes::Min, minValue)
            ->DataElement(
                AZ::Edit::UIHandlers::SpinBox, &CameraMovementSettings::m_boostMultiplier, "Camera Boost Multiplier",
                "Camera boost multiplier to apply to movement speed")
            ->Attribute(AZ::Edit::Attributes::Min, minValue)
            ->DataElement(
                AZ::Edit::UIHandlers::SpinBox, &CameraMovementSettings::m_scrollSpeed, "Camera Scroll Speed",
                "Camera movement speed while using scroll/wheel input")
            ->Attribute(AZ::Edit::Attributes::Min, minValue)
            ->DataElement(
                AZ::Edit::UIHandlers::SpinBox, &CameraMovementSettings::m_dollySpeed, "Camera Dolly Speed",
                "Camera movement speed while using mouse motion to move in and out")
            ->Attribute(AZ::Edit::Attributes::Min, minValue)
            ->DataElement(
                AZ::Edit::UIHandlers::SpinBox, &CameraMovementSettings::m_panSpeed, "Camera Pan Speed",
                "Camera movement speed while panning using the mouse")
            ->Attribute(AZ::Edit::Attributes::Min, minValue)
            ->DataElement(
                AZ::Edit::UIHandlers::CheckBox, &CameraMovementSettings::m_rotateSmoothing, "Camera Rotate Smoothing",
                "Is camera rotation smoothing enabled or disabled")
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ::Edit::PropertyRefreshLevels::EntireTree)
            ->DataElement(
                AZ::Edit::UIHandlers::SpinBox, &CameraMovementSettings::m_rotateSmoothness, "Camera Rotate Smoothness",
                "Amount of camera smoothing to apply while rotating the camera")
            ->Attribute(AZ::Edit::Attributes::Min, minValue)
            ->Attribute(AZ::Edit::Attributes::Visibility, &CameraMovementSettings::RotateSmoothingVisibility)
            ->DataElement(
                AZ::Edit::UIHandlers::CheckBox, &CameraMovementSettings::m_translateSmoothing, "Camera Translate Smoothing",
                "Is camera translation smoothing enabled or disabled")
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ::Edit::PropertyRefreshLevels::EntireTree)
            ->DataElement(
                AZ::Edit::UIHandlers::SpinBox, &CameraMovementSettings::m_translateSmoothness, "Camera Translate Smoothness",
                "Amount of camera smoothing to apply while translating the camera")
            ->Attribute(AZ::Edit::Attributes::Min, minValue)
            ->Attribute(AZ::Edit::Attributes::Visibility, &CameraMovementSettings::TranslateSmoothingVisibility)
            ->DataElement(
                AZ::Edit::UIHandlers::CheckBox, &CameraMovementSettings::m_pivotYawRotationInverted, "Camera Pivot Yaw Inverted",
                "Inverted yaw rotation while pivoting")
            ->DataElement(
                AZ::Edit::UIHandlers::CheckBox, &CameraMovementSettings::m_panInvertedX, "Invert Pan X",
                "Invert direction of pan in local X axis")
            ->DataElement(
                AZ::Edit::UIHandlers::CheckBox, &CameraMovementSettings::m_panInvertedY, "Invert Pan Y",
                "Invert direction of pan in local Y axis")
            ->DataElement(
                AZ::Edit::UIHandlers::CheckBox, &CameraMovementSettings::m_captureCursorLook, "Camera Capture Look Cursor",
                "Should the cursor be captured (hidden) while performing free look");

        editContext->Class<CameraInputSettings>("Camera Input Settings", "")
            ->DataElement(
                AZ::Edit::UIHandlers::ComboBox, &CameraInputSettings::m_translateForwardChannelId, "Translate Forward",
                "Key/button to move the camera forward")
            ->Attribute(AZ::Edit::Attributes::StringList, &GetEditorInputNames)
            ->DataElement(
                AZ::Edit::UIHandlers::ComboBox, &CameraInputSettings::m_translateBackwardChannelId, "Translate Backward",
                "Key/button to move the camera backward")
            ->Attribute(AZ::Edit::Attributes::StringList, &GetEditorInputNames)
            ->DataElement(
                AZ::Edit::UIHandlers::ComboBox, &CameraInputSettings::m_translateLeftChannelId, "Translate Left",
                "Key/button to move the camera left")
            ->Attribute(AZ::Edit::Attributes::StringList, &GetEditorInputNames)
            ->DataElement(
                AZ::Edit::UIHandlers::ComboBox, &CameraInputSettings::m_translateRightChannelId, "Translate Right",
                "Key/button to move the camera right")
            ->Attribute(AZ::Edit::Attributes::StringList, &GetEditorInputNames)
            ->DataElement(
                AZ::Edit::UIHandlers::ComboBox, &CameraInputSettings::m_translateUpChannelId, "Translate Up",
                "Key/button to move the camera up")
            ->Attribute(AZ::Edit::Attributes::StringList, &GetEditorInputNames)
            ->DataElement(
                AZ::Edit::UIHandlers::ComboBox, &CameraInputSettings::m_translateDownChannelId, "Translate Down",
                "Key/button to move the camera down")
            ->Attribute(AZ::Edit::Attributes::StringList, &GetEditorInputNames)
            ->DataElement(
                AZ::Edit::UIHandlers::ComboBox, &CameraInputSettings::m_boostChannelId, "Boost",
                "Key/button to move the camera more quickly")
            ->Attribute(AZ::Edit::Attributes::StringList, &GetEditorInputNames)
            ->DataElement(
                AZ::Edit::UIHandlers::ComboBox, &CameraInputSettings::m_pivotChannelId, "Pivot",
                "Key/button to begin the camera pivot behavior")
            ->Attribute(AZ::Edit::Attributes::StringList, &GetEditorInputNames)
            ->DataElement(
                AZ::Edit::UIHandlers::ComboBox, &CameraInputSettings::m_freeLookChannelId, "Free Look",
                "Key/button to begin camera free look")
            ->Attribute(AZ::Edit::Attributes::StringList, &GetEditorInputNames)
            ->DataElement(
                AZ::Edit::UIHandlers::ComboBox, &CameraInputSettings::m_freePanChannelId, "Free Pan", "Key/button to begin camera free pan")
            ->Attribute(AZ::Edit::Attributes::StringList, &GetEditorInputNames)
            ->DataElement(
                AZ::Edit::UIHandlers::ComboBox, &CameraInputSettings::m_pivotLookChannelId, "Pivot Look",
                "Key/button to begin camera pivot look")
            ->Attribute(AZ::Edit::Attributes::StringList, &GetEditorInputNames)
            ->DataElement(
                AZ::Edit::UIHandlers::ComboBox, &CameraInputSettings::m_pivotDollyChannelId, "Pivot Dolly",
                "Key/button to begin camera pivot dolly")
            ->Attribute(AZ::Edit::Attributes::StringList, &GetEditorInputNames)
            ->DataElement(
                AZ::Edit::UIHandlers::ComboBox, &CameraInputSettings::m_pivotPanChannelId, "Pivot Pan",
                "Key/button to begin camera pivot pan")
            ->Attribute(AZ::Edit::Attributes::StringList, &GetEditorInputNames);

        editContext->Class<CEditorPreferencesPage_ViewportCamera>("Viewport Preferences", "Viewport Preferences")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20))
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &CEditorPreferencesPage_ViewportCamera::m_cameraMovementSettings,
                "Camera Movement Settings", "Camera Movement Settings")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &CEditorPreferencesPage_ViewportCamera::m_cameraInputSettings, "Camera Input Settings",
                "Camera Input Settings");
    }
}

CEditorPreferencesPage_ViewportCamera::CEditorPreferencesPage_ViewportCamera()
{
    InitializeSettings();
    m_icon = QIcon(":/res/Camera.svg");
}

const char* CEditorPreferencesPage_ViewportCamera::GetCategory()
{
    return "Viewports";
}

const char* CEditorPreferencesPage_ViewportCamera::GetTitle()
{
    return "Camera";
}

QIcon& CEditorPreferencesPage_ViewportCamera::GetIcon()
{
    return m_icon;
}

void CEditorPreferencesPage_ViewportCamera::OnCancel()
{
    // noop
}

bool CEditorPreferencesPage_ViewportCamera::OnQueryCancel()
{
    return true;
}

void CEditorPreferencesPage_ViewportCamera::OnApply()
{
    SandboxEditor::SetCameraTranslateSpeed(m_cameraMovementSettings.m_translateSpeed);
    SandboxEditor::SetCameraRotateSpeed(m_cameraMovementSettings.m_rotateSpeed);
    SandboxEditor::SetCameraBoostMultiplier(m_cameraMovementSettings.m_boostMultiplier);
    SandboxEditor::SetCameraScrollSpeed(m_cameraMovementSettings.m_scrollSpeed);
    SandboxEditor::SetCameraDollyMotionSpeed(m_cameraMovementSettings.m_dollySpeed);
    SandboxEditor::SetCameraPanSpeed(m_cameraMovementSettings.m_panSpeed);
    SandboxEditor::SetCameraRotateSmoothness(m_cameraMovementSettings.m_rotateSmoothness);
    SandboxEditor::SetCameraRotateSmoothingEnabled(m_cameraMovementSettings.m_rotateSmoothing);
    SandboxEditor::SetCameraTranslateSmoothness(m_cameraMovementSettings.m_translateSmoothness);
    SandboxEditor::SetCameraTranslateSmoothingEnabled(m_cameraMovementSettings.m_translateSmoothing);
    SandboxEditor::SetCameraCaptureCursorForLook(m_cameraMovementSettings.m_captureCursorLook);
    SandboxEditor::SetCameraPivotYawRotationInverted(m_cameraMovementSettings.m_pivotYawRotationInverted);
    SandboxEditor::SetCameraPanInvertedX(m_cameraMovementSettings.m_panInvertedX);
    SandboxEditor::SetCameraPanInvertedY(m_cameraMovementSettings.m_panInvertedY);

    SandboxEditor::SetCameraTranslateForwardChannelId(m_cameraInputSettings.m_translateForwardChannelId);
    SandboxEditor::SetCameraTranslateBackwardChannelId(m_cameraInputSettings.m_translateBackwardChannelId);
    SandboxEditor::SetCameraTranslateLeftChannelId(m_cameraInputSettings.m_translateLeftChannelId);
    SandboxEditor::SetCameraTranslateRightChannelId(m_cameraInputSettings.m_translateRightChannelId);
    SandboxEditor::SetCameraTranslateUpChannelId(m_cameraInputSettings.m_translateUpChannelId);
    SandboxEditor::SetCameraTranslateDownChannelId(m_cameraInputSettings.m_translateDownChannelId);
    SandboxEditor::SetCameraTranslateBoostChannelId(m_cameraInputSettings.m_boostChannelId);
    SandboxEditor::SetCameraPivotChannelId(m_cameraInputSettings.m_pivotChannelId);
    SandboxEditor::SetCameraFreeLookChannelId(m_cameraInputSettings.m_freeLookChannelId);
    SandboxEditor::SetCameraFreePanChannelId(m_cameraInputSettings.m_freePanChannelId);
    SandboxEditor::SetCameraPivotLookChannelId(m_cameraInputSettings.m_pivotLookChannelId);
    SandboxEditor::SetCameraPivotDollyChannelId(m_cameraInputSettings.m_pivotDollyChannelId);
    SandboxEditor::SetCameraPivotPanChannelId(m_cameraInputSettings.m_pivotPanChannelId);

    SandboxEditor::EditorModularViewportCameraComposerNotificationBus::Broadcast(
        &SandboxEditor::EditorModularViewportCameraComposerNotificationBus::Events::OnEditorModularViewportCameraComposerSettingsChanged);
}

void CEditorPreferencesPage_ViewportCamera::InitializeSettings()
{
    m_cameraMovementSettings.m_translateSpeed = SandboxEditor::CameraTranslateSpeed();
    m_cameraMovementSettings.m_rotateSpeed = SandboxEditor::CameraRotateSpeed();
    m_cameraMovementSettings.m_boostMultiplier = SandboxEditor::CameraBoostMultiplier();
    m_cameraMovementSettings.m_scrollSpeed = SandboxEditor::CameraScrollSpeed();
    m_cameraMovementSettings.m_dollySpeed = SandboxEditor::CameraDollyMotionSpeed();
    m_cameraMovementSettings.m_panSpeed = SandboxEditor::CameraPanSpeed();
    m_cameraMovementSettings.m_rotateSmoothness = SandboxEditor::CameraRotateSmoothness();
    m_cameraMovementSettings.m_rotateSmoothing = SandboxEditor::CameraRotateSmoothingEnabled();
    m_cameraMovementSettings.m_translateSmoothness = SandboxEditor::CameraTranslateSmoothness();
    m_cameraMovementSettings.m_translateSmoothing = SandboxEditor::CameraTranslateSmoothingEnabled();
    m_cameraMovementSettings.m_captureCursorLook = SandboxEditor::CameraCaptureCursorForLook();
    m_cameraMovementSettings.m_pivotYawRotationInverted = SandboxEditor::CameraPivotYawRotationInverted();
    m_cameraMovementSettings.m_panInvertedX = SandboxEditor::CameraPanInvertedX();
    m_cameraMovementSettings.m_panInvertedY = SandboxEditor::CameraPanInvertedY();

    m_cameraInputSettings.m_translateForwardChannelId = SandboxEditor::CameraTranslateForwardChannelId().GetName();
    m_cameraInputSettings.m_translateBackwardChannelId = SandboxEditor::CameraTranslateBackwardChannelId().GetName();
    m_cameraInputSettings.m_translateLeftChannelId = SandboxEditor::CameraTranslateLeftChannelId().GetName();
    m_cameraInputSettings.m_translateRightChannelId = SandboxEditor::CameraTranslateRightChannelId().GetName();
    m_cameraInputSettings.m_translateUpChannelId = SandboxEditor::CameraTranslateUpChannelId().GetName();
    m_cameraInputSettings.m_translateDownChannelId = SandboxEditor::CameraTranslateDownChannelId().GetName();
    m_cameraInputSettings.m_boostChannelId = SandboxEditor::CameraTranslateBoostChannelId().GetName();
    m_cameraInputSettings.m_pivotChannelId = SandboxEditor::CameraPivotChannelId().GetName();
    m_cameraInputSettings.m_freeLookChannelId = SandboxEditor::CameraFreeLookChannelId().GetName();
    m_cameraInputSettings.m_freePanChannelId = SandboxEditor::CameraFreePanChannelId().GetName();
    m_cameraInputSettings.m_pivotLookChannelId = SandboxEditor::CameraPivotLookChannelId().GetName();
    m_cameraInputSettings.m_pivotDollyChannelId = SandboxEditor::CameraPivotDollyChannelId().GetName();
    m_cameraInputSettings.m_pivotPanChannelId = SandboxEditor::CameraPivotPanChannelId().GetName();
}
