#if RMC

#  include "import/mkw/net/selectHandler.hpp"
#  include "import/mkw/net/userHandler.hpp"
#  include "import/mkw/ui/page/friendRoomPage.hpp"
#  include "import/mkw/ui/page/wifiFriendMenuPage.hpp"
#  include "import/mkw/ui/page/wifiMenuPage.hpp"
#  include "wwfcOpenHostMessages.hpp"
#  include "wwfcPatch.hpp"

namespace wwfc::mkw::Net
{

u32 NetController::s_reportedAids = 0x00000000;
u32 SelectHandler::s_kickTimerFrames = 0;

} // namespace wwfc::mkw::Net

namespace wwfc::mkw::UI
{

OpenHostPage::State OpenHostPage::s_state = OpenHostPage::State::Previous;
MenuInputManager::Handler<OpenHostPage>* OpenHostPage::s_onOption = nullptr;
YesNoPage::Handler<OpenHostPage>* OpenHostPage::s_onYesOrNo = nullptr;
bool OpenHostPage::s_openHostEnabled = false;
bool OpenHostPage::s_sentOpenHostValue = false;
OpenHostMessages;

const wchar_t* WifiMenuPage::s_messageOfTheDay =
    L"Welcome to\nRetro Wi-Fi Connection!";
wchar_t WifiMenuPage::s_messageOfTheDayBuffer[256] = {};
bool WifiMenuPage::s_hasSeenMessageOfTheDay = false;

} // namespace wwfc::mkw::UI

namespace wwfc::mkw::Feature
{

static GameSpy::GPResult
GetMessageOfTheDay(GameSpy::GPResult gpResult, const char* message)
{
    using namespace wwfc::mkw::UI;

    const char loginChallenge2Message[] = "\\lc\\2";
    if (std::strncmp(
            message, loginChallenge2Message, sizeof(loginChallenge2Message) - 1
        )) {
        return gpResult;
    }

    const char motdKey[] = "\\wl:motd\\";
    char value[1024];
    if (!GameSpy::gpiValueForKey(message, motdKey, value, sizeof(value))) {
        return gpResult;
    }

    wchar_t* messageOfTheDayBuffer = WifiMenuPage::MessageOfTheDayBuffer();
    s32 messageOfTheDayBufferSize =
        static_cast<s32>(WifiMenuPage::MessageOfTheDayBufferSize());
    s32 messageOfTheDayLength = DWC::DWC_Base64Decode(
        value, std::strlen(value),
        reinterpret_cast<char*>(messageOfTheDayBuffer),
        messageOfTheDayBufferSize
    );
    if (messageOfTheDayLength == -1 ||
        messageOfTheDayLength == messageOfTheDayBufferSize) {
        return gpResult;
    }
    messageOfTheDayBuffer[messageOfTheDayLength / sizeof(wchar_t)] = L'\0';

    WifiMenuPage::SetMessageOfTheDay(messageOfTheDayBuffer);

    return gpResult;
}

// Get the Message Of The Day from the "Login Challenge 2" message
WWFC_DEFINE_PATCH = Patch::BranchWithCTR(
    WWFC_PATCH_LEVEL_FEATURE,
    RMCXD_PORT(0x80101074, 0x80100FD4, 0x80100F94, 0x801010EC, DEMOTODO), //
    ASM_LAMBDA(
        ( : ASM_IMPORT(i, GetMessageOfTheDay)),
        // clang-format off
            mr        r4, r26;

            bl        _restgpr_26;
            lwz       r0, 0x2D4(r1);
            mtlr      r0;
            addi      r1, r1, 0x2D0;
            
            b         %[GetMessageOfTheDay];
        // clang-format on
    )
);

// Display the Message Of The Day when a client connects to the server
WWFC_DEFINE_PATCH = Patch::BranchWithCTR(
    WWFC_PATCH_LEVEL_FEATURE,
    RMCXD_PORT(0x8064BCD4, 0x806189C0, 0x8064B340, 0x80639FEC, DEMOTODO), //
    ASM_LAMBDA(
        ( : ASM_IMPORT_AS(
            i, UI::WifiMenuPage_showMessageOfTheDay, ShowMessageOfTheDay
        )),
        // clang-format off
            mr        r3, r31;
            
            lwz       r31, 0x0C(r1);
            lwz       r0, 0x14(r1);
            mtlr      r0;
            addi      r1, r1, 0x10;
            
            b         %[ShowMessageOfTheDay];
        // clang-format on
    )
);

// Allow users to open rooms without having any friends added
WWFC_DEFINE_PATCH = 
    Patch::CallWithCTR( //
        WWFC_PATCH_LEVEL_FEATURE, //
        RMCXD_PORT(0x8064D358, 0x8061A044, 0x8064C9C4, 0x8063B670, DEMOTODO), //
        [](mkw::UI::WifiFriendMenuPage* /* wifiFriendMenuPage */,
           void* /* pushButton */) -> int {
    constexpr int friendsAdded = 1;

    return friendsAdded;
}
    );

// Prevent clients from stalling rooms
WWFC_DEFINE_PATCH = 
    Patch::CallWithCTR( //
        WWFC_PATCH_LEVEL_FEATURE, //
        RMCXD_PORT(0x806579A0, 0x80653518, 0x8065700C, 0x80645CB8, DEMOTODO), //
        [](mkw::Net::NetController* netController) -> void {
    using namespace mkw::Net;

    netController->sendRacePacket();

    UserHandler::Instance()->calc();

    SelectHandler* selectHandler = SelectHandler::Instance();
    if (selectHandler) {
        selectHandler->processKicks();
    }
}
    );

// Clear the flag that indicates whether an aid was reported to the server
// when closing the connection to them.
WWFC_DEFINE_PATCH = 
    Patch::BranchWithCTR( //
        WWFC_PATCH_LEVEL_FEATURE, //
        RMCXD_PORT(0x806588C8, 0x80654440, 0x80657F34, 0x80646BE0, DEMOTODO), //
        ASM_LAMBDA((:ASM_IMPORT_AS(i, mkw::Net::NetController::ClearReportedAid, ClearReportedAid)),
            // clang-format off
            mr        r3, r28;

            lwz       r28, 0x10(r1);
            mtlr      r0;
            addi      r1, r1, 0x20;

            b         %[ClearReportedAid];
            // clang-format on
        )
    );

// Reset the timer that is used to detect if clients are stalling the room
WWFC_DEFINE_PATCH = 
    Patch::CallWithCTR( //
        WWFC_PATCH_LEVEL_FEATURE, //
        RMCXD_PORT(0x8065FF34, 0x80657FF8, 0x8065F5A0, 0x8064E24C, DEMOTODO), //
        [](mkw::Net::SelectHandler* selectHandler,
           mkw::Net::NetController* netController) -> mkw::Net::SelectHandler* {
    using namespace mkw::Net;

    netController->_29B0 = 0;
    netController->_29B4 = 0;
    netController->_29B8 = 0;
    netController->_29BC = 0;

    SelectHandler::ResetKickTimer();

    return selectHandler;
}
    );

// Report information about the upcoming match to the server
// Note: Conflicts with pulsar track repick prevention, reimplemented in
// rr-pulsar as opposed to the payload.
/*
WWFC_DEFINE_PATCH = {
    Patch::CallWithCTR( //
        WWFC_PATCH_LEVEL_FEATURE, //
        RMCXD_PORT(0x8066148C, 0x80659550, 0x80660AF8, 0x8064F7A4, DEMOTODO), //
        []() -> void {
            using namespace mkw::Net;

            SelectHandler* selectHandler = SelectHandler::Instance();
            selectHandler->decideCourse();
            selectHandler->initPlayerIdsToPlayerAids();

            SelectHandler::Packet::SelectedCourse selectedCourse =
                selectHandler->sendPacket().selectedCourse;
            SelectHandler::Packet::EngineClass engineClass =
                selectHandler->sendPacket().engineClass;

            wwfc::GPReport::ReportU32(
                "wl:mkw_select_course", static_cast<u32>(selectedCourse)
            );
            wwfc::GPReport::ReportU32(
                "wl:mkw_select_cc", static_cast<u32>(engineClass)
            );
        }
    ),
};
*/

// Allow the "Open Host" feature to be enabled via the press of a button
WWFC_DEFINE_PATCH = Patch::WritePointer(
    WWFC_PATCH_LEVEL_FEATURE,
    RMCXD_PORT(0x808B9008, 0x808BABF8, 0x808B8158, 0x808A7470, DEMOTODO), //
    mkw::UI::FriendRoomPage_onActivate
);
WWFC_DEFINE_PATCH = Patch::WritePointer(
    WWFC_PATCH_LEVEL_FEATURE,
    RMCXD_PORT(0x808B900C, 0x808BABFC, 0x808B815C, 0x808A7474, DEMOTODO), //
    mkw::UI::FriendRoomPage_onDeactivate
);
WWFC_DEFINE_PATCH = Patch::WritePointer(
    WWFC_PATCH_LEVEL_FEATURE,
    RMCXD_PORT(0x808B902C, 0x808BAC1C, 0x808B817C, 0x808A7494, DEMOTODO), //
    mkw::UI::FriendRoomPage_onRefocus
);
WWFC_DEFINE_PATCH = Patch::WritePointer(
    WWFC_PATCH_LEVEL_FEATURE,
    RMCXD_PORT(0x808BFE7C, 0x808B97CC, 0x808BEFCC, 0x808AE2EC, DEMOTODO), //
    mkw::UI::WifiFriendMenu_onActivate
);
WWFC_DEFINE_PATCH = Patch::WritePointer(
    WWFC_PATCH_LEVEL_FEATURE,
    RMCXD_PORT(0x808BFE80, 0x808B97D0, 0x808BEFD0, 0x808AE2F0, DEMOTODO), //
    mkw::UI::WifiFriendMenu_onDeactivate
);
WWFC_DEFINE_PATCH = Patch::WritePointer(
    WWFC_PATCH_LEVEL_FEATURE,
    RMCXD_PORT(0x808BFEA0, 0x808B97F0, 0x808BEFF0, 0x808AE310, DEMOTODO), //
    mkw::UI::WifiFriendMenu_onRefocus
);

// Fix VR limit in serverbrowser requests [ppeb]
// 30000 VR
const u8 LIMIT[2] = {0x75, 0x30};
// EV
WWFC_DEFINE_PATCH = Patch::Write( //
        WWFC_PATCH_LEVEL_FEATURE, //
        RMCXD_PORT(0x80659342, 0x80654eba, 0x806589ae, 0x8064765a, DEMOTODO), //
        LIMIT
    );
WWFC_DEFINE_PATCH = Patch::Write( //
        WWFC_PATCH_LEVEL_FEATURE, //
        RMCXD_PORT(0x8065934a, 0x80654ec2, 0x806589b6, 0x80647662, DEMOTODO), //
        LIMIT
    );
// EB
WWFC_DEFINE_PATCH = Patch::Write( //
        WWFC_PATCH_LEVEL_FEATURE, //
        RMCXD_PORT(0x80659422, 0x80654f9a, 0x80658a8e, 0x8064773a, DEMOTODO), //
        LIMIT
    );
WWFC_DEFINE_PATCH = Patch::Write( //
        WWFC_PATCH_LEVEL_FEATURE, //
        RMCXD_PORT(0x8065942a, 0x80654fa2, 0x80658a96, 0x80647742, DEMOTODO), //
        LIMIT
    );

} // namespace wwfc::mkw::Feature

#endif // RMC
