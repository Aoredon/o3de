/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzTest/AzTest.h>
#include <AzToolsFramework/ComponentMode/EditorComponentModeBus.h>
#include <AzToolsFramework/UnitTest/AzToolsFrameworkTestHelpers.h>
#include <AzToolsFramework/ViewportSelection/EditorPickEntitySelection.h>
#include <AzToolsFramework/ViewportSelection/ViewportEditorModeTracker.h>

namespace UnitTest
{
    using ViewportEditorMode = AzToolsFramework::ViewportEditorMode;
    using ViewportEditorModes = AzToolsFramework::ViewportEditorModes;
    using ViewportEditorModeTracker = AzToolsFramework::ViewportEditorModeTracker;
    using ViewportEditorModeInfo = AzToolsFramework::ViewportEditorModeInfo;
    using ViewportId = ViewportEditorModeInfo::IdType;
    using ViewportEditorModesInterface = AzToolsFramework::ViewportEditorModesInterface;
    using ViewportEditorModeTrackerInterface = AzToolsFramework::ViewportEditorModeTrackerInterface;

    void ActivateModeAndExpectSuccess(ViewportEditorModes& editorModeState, ViewportEditorMode mode)
    {
        const auto result = editorModeState.ActivateMode(mode);
        EXPECT_TRUE(result.IsSuccess());
    }

    void DeactivateModeAndExpectSuccess(ViewportEditorModes& editorModeState, ViewportEditorMode mode)
    {
        const auto result = editorModeState.DeactivateMode(mode);
        EXPECT_TRUE(result.IsSuccess());
    }

    void SetAllModesActive(ViewportEditorModes& editorModeState)
    {
        for (auto mode = 0; mode < ViewportEditorModes::NumEditorModes; mode++)
        {
            ActivateModeAndExpectSuccess(editorModeState, static_cast<ViewportEditorMode>(mode));
        }
    }

    void SetAllModesInactive(ViewportEditorModes& editorModeState)
    {
        for (auto mode = 0; mode < ViewportEditorModes::NumEditorModes; mode++)
        {
            DeactivateModeAndExpectSuccess(editorModeState, static_cast<ViewportEditorMode>(mode));
        }
    }

    void ExpectOnlyModeActive(const ViewportEditorModesInterface& editorModeState, ViewportEditorMode mode)
    {
        for (auto modeIndex = 0; modeIndex < ViewportEditorModes::NumEditorModes; modeIndex++)
        {
            const auto currentMode = static_cast<ViewportEditorMode>(modeIndex);
            const bool expectedActive = (mode == currentMode);
            EXPECT_EQ(editorModeState.IsModeActive(currentMode), expectedActive);
        }
    }

    void ExpectOnlyModeInactive(const ViewportEditorModesInterface& editorModeState, ViewportEditorMode mode)
    {
        for (auto modeIndex = 0; modeIndex < ViewportEditorModes::NumEditorModes; modeIndex++)
        {
            const auto currentMode = static_cast<ViewportEditorMode>(modeIndex);
            const bool expectedActive = (mode != currentMode);
            EXPECT_EQ(editorModeState.IsModeActive(currentMode), expectedActive);
        }
    }

    // Fixture for testing editor mode states
    class ViewportEditorModesTestsFixture
        : public ::testing::Test
    {
    public:
        ViewportEditorModes m_editorModes;
    };

    // Fixture for testing editor mode states with parameterized test arguments
    class ViewportEditorModesTestsFixtureWithParams
        : public ViewportEditorModesTestsFixture
        , public ::testing::WithParamInterface<AzToolsFramework::ViewportEditorMode>
    {
    public:
        void SetUp() override
        {
            m_selectedEditorMode = GetParam();
        }

        ViewportEditorMode m_selectedEditorMode;
    };

    // Fixture for testing the viewport editor mode state tracker
    class ViewportEditorModeTrackerTestFixture
        : public ToolsApplicationFixture
    {
    public:
        ViewportEditorModeTracker m_viewportEditorModeTracker;
    };

    // Subscriber of viewport editor mode notifications for a single viewport that expects a single mode to be activated/deactivated
    class ViewportEditorModeNotificationsBusHandler
        : private AzToolsFramework::ViewportEditorModeNotificationsBus::Handler
    {
     public:
        struct ReceivedEvents
        {
            bool m_onEnter = false;
            bool m_onExit = false;
        };

        using EditModeTracker = AZStd::unordered_map<ViewportEditorMode, ReceivedEvents>;

        ViewportEditorModeNotificationsBusHandler(ViewportId viewportId)
             : m_viewportSubscription(viewportId)
        {
            AzToolsFramework::ViewportEditorModeNotificationsBus::Handler::BusConnect(m_viewportSubscription);
        }

        ~ViewportEditorModeNotificationsBusHandler()
        {
            AzToolsFramework::ViewportEditorModeNotificationsBus::Handler::BusDisconnect();
        }

        ViewportId GetViewportSubscription() const
        {
            return m_viewportSubscription;
        }

        const EditModeTracker& GetEditorModes() const
        {
            return m_editorModes;
        }

        void OnEditorModeActivated([[maybe_unused]]const ViewportEditorModesInterface& editorModeState, ViewportEditorMode mode) override
        {
            m_editorModes[mode].m_onEnter = true;
        }

        void OnEditorModeDeactivated([[maybe_unused]] const ViewportEditorModesInterface& editorModeState, ViewportEditorMode mode) override
        {
            m_editorModes[mode].m_onExit = true;
        }

     private:
         ViewportId m_viewportSubscription;
         EditModeTracker m_editorModes;

    };

    // Fixture for testing viewport editor mode notifications publishing
    class ViewportEditorModePublisherTestFixture
        : public ViewportEditorModeTrackerTestFixture
    {
    public:

        void SetUpEditorFixtureImpl() override
        {
            for (auto mode = 0; mode < ViewportEditorModes::NumEditorModes; mode++)
            {
                m_editorModeHandlers[mode] = AZStd::make_unique<ViewportEditorModeNotificationsBusHandler>(mode);
            }
        }

        void TearDownEditorFixtureImpl() override
        {
            for (auto mode = 0; mode < ViewportEditorModes::NumEditorModes; mode++)
            {
                m_editorModeHandlers[mode].reset();
            }
        }

        AZStd::array<AZStd::unique_ptr<ViewportEditorModeNotificationsBusHandler>, ViewportEditorModes::NumEditorModes> m_editorModeHandlers;
    };

    // Fixture for testing the integration of viewport editor mode state tracker
    class ViewportEditorModeTrackerIntegrationTestFixture
        : public ToolsApplicationFixture
    {
    public:
        void SetUpEditorFixtureImpl() override
        {
            m_viewportEditorModeTracker = AZ::Interface<ViewportEditorModeTrackerInterface>::Get();
            ASSERT_NE(m_viewportEditorModeTracker, nullptr);
            m_viewportEditorModes = m_viewportEditorModeTracker->GetViewportEditorModes({});
        }

        ViewportEditorModeTrackerInterface* m_viewportEditorModeTracker = nullptr;
        const ViewportEditorModesInterface* m_viewportEditorModes = nullptr;
    };

    TEST_F(ViewportEditorModesTestsFixture, NumberOfEditorModesIsEqualTo4)
    {
        EXPECT_EQ(ViewportEditorModes::NumEditorModes, 4);
    }

    TEST_F(ViewportEditorModesTestsFixture, InitialEditorModeStateHasAllInactiveModes)
    {
        for (auto mode = 0; mode < ViewportEditorModes::NumEditorModes; mode++)
        {
            EXPECT_FALSE(m_editorModes.IsModeActive(static_cast<ViewportEditorMode>(mode)));
        }
    }

    TEST_P(ViewportEditorModesTestsFixtureWithParams, SettingModeActiveActivatesOnlyThatMode)
    {
        ActivateModeAndExpectSuccess(m_editorModes, m_selectedEditorMode);
        ExpectOnlyModeActive(m_editorModes, m_selectedEditorMode);
    }

    TEST_P(ViewportEditorModesTestsFixtureWithParams, SettingModeInactiveInactivatesOnlyThatMode)
    {
        SetAllModesActive(m_editorModes);
        DeactivateModeAndExpectSuccess(m_editorModes, m_selectedEditorMode);
        ExpectOnlyModeInactive(m_editorModes, m_selectedEditorMode);
    }

    TEST_P(ViewportEditorModesTestsFixtureWithParams, SettingMultipleModesActiveActivatesAllThoseModesNonMutuallyExclusively)
    {
        for (auto mode = 0; mode < ViewportEditorModes::NumEditorModes - 1; mode++)
        {
            // Given only the selected mode active
            SetAllModesInactive(m_editorModes);
            {
                ActivateModeAndExpectSuccess(m_editorModes, m_selectedEditorMode);
            }

            const auto editorMode = static_cast<ViewportEditorMode>(mode);
            if (editorMode == m_selectedEditorMode)
            {
                continue;
            }

            // When other modes are activated
            ActivateModeAndExpectSuccess(m_editorModes, editorMode);

            for (auto expectedMode = 0; expectedMode < ViewportEditorModes::NumEditorModes; expectedMode++)
            {
                const auto expectedEditorMode = static_cast<ViewportEditorMode>(expectedMode);
                if (expectedEditorMode == editorMode || expectedEditorMode == m_selectedEditorMode)
                {
                    // Expect the activated modes to be active
                    EXPECT_TRUE(m_editorModes.IsModeActive(expectedEditorMode));
                }
                else
                {
                    // Expect the modes not active to be inactive
                    EXPECT_FALSE(m_editorModes.IsModeActive(expectedEditorMode));
                }
            }
        }
    }

    TEST_P(ViewportEditorModesTestsFixtureWithParams, SettingMultipleModesInactiveInactivatesAllThoseModesNonMutuallyExclusively)
    {
        for (auto mode = 0; mode < ViewportEditorModes::NumEditorModes - 1; mode++)
        {
            // Given only the selected mode inactive
            SetAllModesActive(m_editorModes);
            DeactivateModeAndExpectSuccess(m_editorModes, m_selectedEditorMode);

            const auto editorMode = static_cast<ViewportEditorMode>(mode);
            if (editorMode == m_selectedEditorMode)
            {
                continue;
            }

            // When other modes are deactivated
            DeactivateModeAndExpectSuccess(m_editorModes, editorMode);

            for (auto expectedMode = 0; expectedMode < ViewportEditorModes::NumEditorModes; expectedMode++)
            {
                const auto expectedEditorMode = static_cast<ViewportEditorMode>(expectedMode);
                if (expectedEditorMode == editorMode || expectedEditorMode == m_selectedEditorMode)
                {
                    // Expect the deactivated modes to be inactive
                    EXPECT_FALSE(m_editorModes.IsModeActive(expectedEditorMode));
                }
                else
                {
                    // Expects the modes not deactivated to still be active
                    EXPECT_TRUE(m_editorModes.IsModeActive(expectedEditorMode));
                }
            }
        }
    }

    INSTANTIATE_TEST_CASE_P(
        AllEditorModes,
        ViewportEditorModesTestsFixtureWithParams,
        ::testing::Values(
            AzToolsFramework::ViewportEditorMode::Default,
            AzToolsFramework::ViewportEditorMode::Component,
            AzToolsFramework::ViewportEditorMode::Focus,
            AzToolsFramework::ViewportEditorMode::Pick));

    TEST_F(ViewportEditorModesTestsFixture, SettingOutOfBoundsModeActiveReturnsError)
    {
        const auto result = m_editorModes.ActivateMode(static_cast<ViewportEditorMode>(ViewportEditorModes::NumEditorModes));
        EXPECT_FALSE(result.IsSuccess());
    }

    TEST_F(ViewportEditorModesTestsFixture, SettingOutOfBoundsModeInactiveReturnsError)
    {
        const auto result = m_editorModes.DeactivateMode(static_cast<ViewportEditorMode>(ViewportEditorModes::NumEditorModes));
        EXPECT_FALSE(result.IsSuccess());
    }

    TEST_F(ViewportEditorModeTrackerTestFixture, InitialCentralStateTrackerHasNoViewportEditorModess)
    {
        EXPECT_EQ(m_viewportEditorModeTracker.GetTrackedViewportCount(), 0);
    }

    TEST_F(ViewportEditorModeTrackerTestFixture, ActivatingViewportEditorModeForNonExistentIdCreatesViewportEditorModesForThatId)
    {
        // Given a viewport not currently being tracked
        const ViewportId viewportid = 0;
        EXPECT_FALSE(m_viewportEditorModeTracker.IsViewportModeTracked({ viewportid }));
        EXPECT_EQ(m_viewportEditorModeTracker.GetViewportEditorModes({ viewportid }), nullptr);

        // When a mode is activated for that viewport
        const auto editorMode = ViewportEditorMode::Default;
        m_viewportEditorModeTracker.ActivateMode({ viewportid }, editorMode);
        const auto* viewportEditorModeState = m_viewportEditorModeTracker.GetViewportEditorModes({ viewportid });

        // Expect that viewport to now be tracked
        EXPECT_TRUE(m_viewportEditorModeTracker.IsViewportModeTracked({ viewportid }));
        EXPECT_NE(viewportEditorModeState, nullptr);

        // Expect the mode for that viewport to be active
        EXPECT_TRUE(viewportEditorModeState->IsModeActive(editorMode));
    }

    TEST_F(ViewportEditorModeTrackerTestFixture, DeactivatingViewportEditorModeForNonExistentIdCreatesViewportEditorModesForThatIdButReturnsError)
    {
        // Given a viewport not currently being tracked
        const ViewportId viewportid = 0;
        EXPECT_FALSE(m_viewportEditorModeTracker.IsViewportModeTracked({ viewportid }));
        EXPECT_EQ(m_viewportEditorModeTracker.GetViewportEditorModes({ viewportid }), nullptr);

        // When a mode is deactivated for that viewport
        const auto editorMode = ViewportEditorMode::Default;
        const auto expectedErrorMsg = AZStd::string::format(
            "Call to DeactivateMode for mode '%u' on id '%i' without precursor call to ActivateMode", static_cast<AZ::u32>(editorMode), viewportid);
        const auto result = m_viewportEditorModeTracker.DeactivateMode({ viewportid }, editorMode);

        // Expect an error due to no precursor activation of that mode
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_EQ(result.GetError(), expectedErrorMsg);

        // Expect that viewport to now be tracked
        const auto* viewportEditorModeState = m_viewportEditorModeTracker.GetViewportEditorModes({ viewportid });
        EXPECT_TRUE(m_viewportEditorModeTracker.IsViewportModeTracked({ viewportid }));

        // Expect the mode for that viewport to be inactive
        EXPECT_NE(viewportEditorModeState, nullptr);
        EXPECT_FALSE(viewportEditorModeState->IsModeActive(editorMode));
    }

    TEST_F(ViewportEditorModeTrackerTestFixture, GettingNonExistentViewportEditorModesForIdReturnsNull)
    {
        const ViewportId viewportid = 0;
        EXPECT_FALSE(m_viewportEditorModeTracker.IsViewportModeTracked({ viewportid }));
        EXPECT_EQ(m_viewportEditorModeTracker.GetViewportEditorModes({ viewportid }), nullptr);
    }

    TEST_F(ViewportEditorModeTrackerTestFixture, ActivatingViewportEditorModesForExistingIdInThatStateReturnsError)
    {
        // Given a viewport not currently tracked
        const ViewportId viewportid = 0;
        EXPECT_FALSE(m_viewportEditorModeTracker.IsViewportModeTracked({ viewportid }));
        EXPECT_EQ(m_viewportEditorModeTracker.GetViewportEditorModes({ viewportid }), nullptr);

        const auto editorMode = ViewportEditorMode::Default;
        {
            // When the mode is activated for the viewport
            const auto result = m_viewportEditorModeTracker.ActivateMode({ viewportid }, editorMode);

            // Expect no error as there is no duplicate activation
            EXPECT_TRUE(result.IsSuccess());

            // Expect the mode to be active for the viewport
            const auto* viewportEditorModeState = m_viewportEditorModeTracker.GetViewportEditorModes({ viewportid });
            EXPECT_TRUE(m_viewportEditorModeTracker.IsViewportModeTracked({ viewportid }));
            EXPECT_NE(viewportEditorModeState, nullptr);
            EXPECT_TRUE(viewportEditorModeState->IsModeActive(editorMode));
        }
        {
            // When the mode is activated again for the viewport
            const auto result = m_viewportEditorModeTracker.ActivateMode({ viewportid }, editorMode);

            // Expect an error for the duplicate activation
            const auto expectedErrorMsg = AZStd::string::format(
                "Duplicate call to ActivateMode for mode '%u' on id '%i'", static_cast<AZ::u32>(editorMode), viewportid);
            EXPECT_FALSE(result.IsSuccess());
            EXPECT_EQ(result.GetError(), expectedErrorMsg);

            // Expect the mode to still be active for the viewport
            const auto* viewportEditorModeState = m_viewportEditorModeTracker.GetViewportEditorModes({ viewportid });
            EXPECT_TRUE(m_viewportEditorModeTracker.IsViewportModeTracked({ viewportid }));
            EXPECT_NE(viewportEditorModeState, nullptr);
            EXPECT_TRUE(viewportEditorModeState->IsModeActive(editorMode));
        }
    }

    TEST_F(ViewportEditorModeTrackerTestFixture, DeactivatingViewportEditorModesForExistingIdNotInThatStateReturnssError)
    {
        // Given a viewport not currently tracked
        const ViewportId viewportid = 0;
        EXPECT_FALSE(m_viewportEditorModeTracker.IsViewportModeTracked({ viewportid }));
        EXPECT_EQ(m_viewportEditorModeTracker.GetViewportEditorModes({ viewportid }), nullptr);

        const auto editorMode = ViewportEditorMode::Default;
        {
            // When the mode is activated and then deactivated for the viewport
            m_viewportEditorModeTracker.ActivateMode({ viewportid }, editorMode);
            const auto result = m_viewportEditorModeTracker.DeactivateMode({ viewportid }, editorMode);

            // Expect no error as there is no duplicate deactivation
            EXPECT_TRUE(result.IsSuccess());

            // Expect the mode to be inctive for the viewport
            const auto* viewportEditorModeState = m_viewportEditorModeTracker.GetViewportEditorModes({ viewportid });
            EXPECT_TRUE(m_viewportEditorModeTracker.IsViewportModeTracked({ viewportid }));
            EXPECT_NE(viewportEditorModeState, nullptr);
            EXPECT_FALSE(viewportEditorModeState->IsModeActive(editorMode));
        }
        {
            // When the mode is deactivated again for the viewport
            const auto result = m_viewportEditorModeTracker.DeactivateMode({ viewportid }, editorMode);

            // Expect an error for the duplicate deactivation
            const auto expectedErrorMsg = AZStd::string::format(
                "Duplicate call to DeactivateMode for mode '%u' on id '%i'", static_cast<AZ::u32>(editorMode), viewportid);
            EXPECT_FALSE(result.IsSuccess());
            EXPECT_EQ(result.GetError(), expectedErrorMsg);

            // Expect the mode to still be inactive for the viewport
            const auto* viewportEditorModeState = m_viewportEditorModeTracker.GetViewportEditorModes({ viewportid });
            EXPECT_TRUE(m_viewportEditorModeTracker.IsViewportModeTracked({ viewportid }));
            EXPECT_NE(viewportEditorModeState, nullptr);
            EXPECT_FALSE(viewportEditorModeState->IsModeActive(editorMode));
        }
    }

    TEST_F(
        ViewportEditorModePublisherTestFixture,
        ActivatingViewportEditorModesForExistingIdPublishesOnViewportEditorModeActivateEventForAllSubscribers)
    {
        // Given a set of subscribers tracking the editor modes for their exclusive viewport
        for (auto mode = 0; mode < ViewportEditorModes::NumEditorModes; mode++)
        {
            // Expect each subscriber to have received no editor mode state changes
            EXPECT_EQ(m_editorModeHandlers[mode]->GetEditorModes().size(), 0);
        }

        // When each editor mode is activated by the state tracker for a specific viewport
        for (auto mode = 0; mode < ViewportEditorModes::NumEditorModes; mode++)
        {
            const ViewportId viewportId = mode;
            const ViewportEditorMode editorMode = static_cast<ViewportEditorMode>(mode);
            m_viewportEditorModeTracker.ActivateMode({ viewportId }, editorMode);
        }

        for (auto mode = 0; mode < ViewportEditorModes::NumEditorModes; mode++)
        {
            // Expect only the subscribers of each viewport to have received the editor mode activated event
            const ViewportEditorMode editorMode = static_cast<ViewportEditorMode>(mode);
            const auto& editorModes = m_editorModeHandlers[mode]->GetEditorModes();
            EXPECT_EQ(editorModes.size(), 1);
            EXPECT_EQ(editorModes.count(editorMode), 1);
            const auto& expectedEditorModeSet = editorModes.find(editorMode);
            EXPECT_NE(expectedEditorModeSet, editorModes.end());
            EXPECT_TRUE(expectedEditorModeSet->second.m_onEnter);
            EXPECT_FALSE(expectedEditorModeSet->second.m_onExit);
        }
    }

    TEST_F(
        ViewportEditorModePublisherTestFixture,
        DeactivatingViewportEditorModesForExistingIdPublishesOnViewportEditorModeDeactivatingEventForAllSubscribers)
    {
        // Given a set of subscribers tracking the editor modes for their exclusive viewport
        for (auto mode = 0; mode < ViewportEditorModes::NumEditorModes; mode++)
        {
            EXPECT_EQ(m_editorModeHandlers[mode]->GetEditorModes().size(), 0);
        }

        // When each editor mode is activated deactivated by the state tracker for a specific viewport
        for (auto mode = 0; mode < ViewportEditorModes::NumEditorModes; mode++)
        {
            const ViewportId viewportId = mode;
            const ViewportEditorMode editorMode = static_cast<ViewportEditorMode>(mode);
            m_viewportEditorModeTracker.ActivateMode({ viewportId }, editorMode);
            m_viewportEditorModeTracker.DeactivateMode({ viewportId }, editorMode);
        }

        for (auto mode = 0; mode < ViewportEditorModes::NumEditorModes; mode++)
        {
            // Expect only the subscribers of each viewport to have received the editor mode activated and deactivated event
            const ViewportEditorMode editorMode = static_cast<ViewportEditorMode>(mode);
            const auto& editorModes = m_editorModeHandlers[mode]->GetEditorModes();
            EXPECT_EQ(editorModes.size(), 1);
            EXPECT_EQ(editorModes.count(editorMode), 1);
            const auto& expectedEditorModeSet = editorModes.find(editorMode);
            EXPECT_NE(expectedEditorModeSet, editorModes.end());
            EXPECT_TRUE(expectedEditorModeSet->second.m_onEnter);
            EXPECT_TRUE(expectedEditorModeSet->second.m_onExit);
        }
    }

    TEST_F(ViewportEditorModeTrackerIntegrationTestFixture, InitialViewportEditorModeIsDefault)
    {
        ExpectOnlyModeActive(*m_viewportEditorModes, ViewportEditorMode::Default);
    }

    TEST_F(
        ViewportEditorModeTrackerIntegrationTestFixture, EnteringComponentModeAfterInitialStateHasViewportEditorModesDefaultAndComponentModeActive)
    {
        // When component mode is entered
        AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::Broadcast(
            &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::BeginComponentMode,
            AZStd::vector<AzToolsFramework::ComponentModeFramework::EntityAndComponentModeBuilders>{});

        bool inComponentMode = false;
        AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::BroadcastResult(
            inComponentMode, &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::InComponentMode);

        // Expect to be in component mode
        EXPECT_TRUE(inComponentMode);

        // Expect the default and component viewport editor modes to be active
        EXPECT_TRUE(m_viewportEditorModes->IsModeActive(ViewportEditorMode::Default));
        EXPECT_TRUE(m_viewportEditorModes->IsModeActive(ViewportEditorMode::Component));

        // Do not expect the pick and focus viewport editor modes to be active
        EXPECT_FALSE(m_viewportEditorModes->IsModeActive(ViewportEditorMode::Pick));
        EXPECT_FALSE(m_viewportEditorModes->IsModeActive(ViewportEditorMode::Focus));
    }

    TEST_F(
        ViewportEditorModeTrackerIntegrationTestFixture,
        EnteringEditorPickEntitySelectionAfterInitialStateHasOnlyViewportEditorModePickModeActive)
    {
        // When entering pick mode
        using AzToolsFramework::EditorInteractionSystemViewportSelectionRequestBus;
        EditorInteractionSystemViewportSelectionRequestBus::Event(
            AzToolsFramework::GetEntityContextId(), &EditorInteractionSystemViewportSelectionRequestBus::Events::SetHandler,
            [](const AzToolsFramework::EditorVisibleEntityDataCache* entityDataCache,
               [[maybe_unused]] AzToolsFramework::ViewportEditorModeTrackerInterface* viewportEditorModeTracker)
            {
                return AZStd::make_unique<AzToolsFramework::EditorPickEntitySelection>(entityDataCache, viewportEditorModeTracker);
            });

        // Expect only the pick viewport editor mode to be active
        ExpectOnlyModeActive(*m_viewportEditorModes, ViewportEditorMode::Pick);
    }

    // FocusMode integration tests will follow (LYN-6995)

} // namespace UnitTest
