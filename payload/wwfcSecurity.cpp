#include "import/mkwNet.hpp"
#include "import/mkwNetEventHandler.hpp"
#include "import/mkwNetItemHandler.hpp"
#include "import/mkwNetRoomHandler.hpp"
#include "import/mkwRegistry.hpp"
#include "import/mkwSystem.hpp"
#include "wwfcPatch.hpp"
#include <cstddef>

namespace wwfc::Security
{

WWFC_DEFINE_PATCH = {
#if ADDRESS_DWCi_GetGPBuddyAdditionalMsg
    // SERVER TO CLIENT VULNERABILITY
    // CVE-ID: CVE-2023-45887
    // https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2023-45887
    Patch::CallWithCTR(
        WWFC_PATCH_LEVEL_CRITICAL, //
        ADDRESS_DWCi_GetGPBuddyAdditionalMsg + 0x90, //
        ASM_LAMBDA(
            // clang-format off
            cmplwi    r31, 0xF; // The destination buffer is of size 0x10; leave room for the null terminator
            mflr      r9;
            mr        r5, r31;
            mr        r4, r28;
            mr        r3, r27;
            ble+      L_ValidLength;

            addi      r9, r9, -0x1C; // return -1
            mtctr     r9;
            bctr;

        L_ValidLength:
            b         memcpy;
            // clang-format on
        )
    ),
#endif
};

// SERVER|CLIENT TO CLIENT VULNERABILITY
// Match command stack buffer overflow. This exists in nearly every Wii game.
WWFC_DEFINE_PATCH = {
#if ADDRESS_PATCH_SECURITY_GT2MATCHCOMMAND
    // CLIENT TO CLIENT VULNERABILITY
    // The peer to peer match command exploit
    Patch::CallWithCTR(
        WWFC_PATCH_LEVEL_CRITICAL, //
        ADDRESS_PATCH_SECURITY_GT2MATCHCOMMAND - 0x24, // 0x800E591C
        ASM_LAMBDA(
            // clang-format off
            lbz     r5, 0x11(sp);
            addi    r0, r5, 0x14;
            cmplw   r31, r0;
            bnelr-; // Error "Got wrong data size GT2 command."

            // Check the maximum length to prevent a buffer overflow
            cmplwi  r5, 0x80;
            bgtlr-; // Error "Got wrong data size GT2 command."

            // OK, jump to the copy routine
            mflr    r12;
            addi    r12, r12, 0x14;
            mtctr   r12;
            bctr;
            // clang-format on
        )
    ),
#endif

#if ADDRESS_PATCH_SECURITY_QR2MATCHCOMMAND
    // SERVER TO CLIENT VULNERABILITY
    // The QR2/MASTER server-sent match command exploit
    Patch::CallWithCTR(
        WWFC_PATCH_LEVEL_CRITICAL, //
        ADDRESS_PATCH_SECURITY_QR2MATCHCOMMAND, // 0x800E5AC8
        ASM_LAMBDA(
            // clang-format off
            lbz     r5, 0x11(sp);
            // Check the maximum length to prevent a buffer overflow
            cmplwi  r5, 0x80;
            bgt-    L_SBCommandError;

            // OK, copy the data to the stack
            addi    r3, r1, 0x1C;
            addi    r4, r28, 0x14;
            // Call and return
            b       memcpy@local;

        L_SBCommandError:;
            // Jump to "Got different version SBcommand." error
            mflr    r12;
            subi    r12, r12, 0x24;
            mtctr   r12;
            bctr;
            // clang-format on
        )
    ),
#endif
};

#if RMC || RMCN

// SERVER TO CLIENT VULNERABILITY
// Patch for Mario Kart Wii friend status (locstring) stack buffer overflow.
// Located in DWC_GetFriendStatusData, this one is a bit annoying because it
// could exist in other games, it just depends on the size the caller is
// expecting.
WWFC_DEFINE_PATCH = {
    Patch::WriteASM(
        WWFC_PATCH_LEVEL_CRITICAL, //
        RMCX_PORT(
            0x800CE220, 0x800CE180, 0x800CE140, 0x800CE280, // Disc
            0x800B5D98, 0x800B5D08, 0x800B5CE8, 0x800B5E08 // Channel
        ),
        1, ASM_LAMBDA(li r6, 0x10)
    ),
};

#endif

#if RMC

static bool
IsPacketSizeValid(mkw::Net::RACEPacket::EType packetType, u8 packetSize)
{
    using namespace mkw::Net;

    if (packetType >= RACEPacket::RaceHeader1 &&
        packetType <= RACEPacket::Event) {
        if (packetSize == 0) {
            return true;
        }
    }

    extern u32 packetBufferSizes[sizeof(RACEPacket::sizes)] AT(
        RMCXD_PORT(0x8089A194, 0x80895AC4, 0x808992F4, 0x808885CC)
    );

    switch (packetType) {
    case RACEPacket::Header: {
        if (packetSize < sizeof(RACEPacket) ||
            packetSize > packetBufferSizes[RACEPacket::Header]) {
            return false;
        }

        return true;
    }
    case RACEPacket::RaceHeader1: {
        if (packetSize < 0x28 ||
            packetSize > packetBufferSizes[RACEPacket::RaceHeader1]) {
            return false;
        }

        return true;
    }
    case RACEPacket::RaceHeader2: {
        if (packetSize < 0x28 ||
            packetSize > packetBufferSizes[RACEPacket::RaceHeader2]) {
            return false;
        }

        return true;
    }
    case RACEPacket::RoomSelect: {
        // 'Room' packet
        if (packetSize < sizeof(SelectHandler::Packet)) {
            return packetSize == sizeof(RoomHandler::Packet);
        }

        // 'Select' packet
        if (packetSize > packetBufferSizes[RACEPacket::RoomSelect]) {
            return false;
        }

        return true;
    }
    case RACEPacket::RaceData: {
        if (packetSize < 0x40 ||
            packetSize > packetBufferSizes[RACEPacket::RaceData] ||
            packetSize % 0x40) {
            return false;
        }

        return true;
    }
    case RACEPacket::User: {
        if (packetSize < sizeof(UserHandler::Packet) ||
            packetSize > packetBufferSizes[RACEPacket::User]) {
            return false;
        }

        return true;
    }
    case RACEPacket::Item: {
        if (packetSize < sizeof(ItemHandler::Packet) ||
            packetSize > packetBufferSizes[RACEPacket::Item] ||
            packetSize % sizeof(ItemHandler::Packet)) {
            return false;
        }

        return true;
    }
    case RACEPacket::Event: {
        if (packetSize < sizeof(EventHandler::Packet::eventInfo) ||
            packetSize > packetBufferSizes[RACEPacket::Event]) {
            return false;
        }

        return true;
    }
    default: {
        return false;
    }
    }
}

static bool IsHeaderPacketDataValid(
    const void* /* packet */, u8 /* packetSize */, u8 /* playerAid */
)
{
    return true;
}

static bool IsRaceHeader1PacketDataValid(
    const void* /* packet */, u8 /* packetSize */, u8 /* playerAid */
)
{
    return true;
}

static bool IsRaceHeader2PacketDataValid(
    const void* /* packet */, u8 /* packetSize */, u8 /* playerAid */
)
{
    return true;
}

static bool
IsRoomSelectPacketDataValid(const void* packet, u8 packetSize, u8 playerAid)
{
    using namespace mkw::Net;
    using namespace mkw::Registry;
    using namespace mkw::System;

    // 'Room' packet
    if (packetSize == sizeof(RoomHandler::Packet)) {
        const RoomHandler::Packet* roomPacket =
            reinterpret_cast<const RoomHandler::Packet*>(packet);

        if (roomPacket->event == RoomHandler::Packet::Event::StartRoom) {
            RKNetController::ConnectionInfo& connectionInfo =
                RKNetController::Instance()->currentConnectionInfo();

            // Ensure that guests can't start rooms
            if (playerAid != connectionInfo.hostAid) {
                return false;
            }
        }
    }
    // 'Select' packet
    else {
        if (!RKNetController::Instance()->inVanillaMatch()) {
            return true;
        }

        RaceConfig::Scenario* scenario;
        if (static_cast<RKScene::SceneID>(
                RKSystem::Instance().sceneManager()->getCurrentSceneID()
            ) == RKScene::SceneID::Race) {
            scenario = &RaceConfig::Instance()->raceScenario();
        } else {
            scenario = &RaceConfig::Instance()->menuScenario();
        }

        const SelectHandler::Packet* selectPacket =
            reinterpret_cast<const SelectHandler::Packet*>(packet);
        for (size_t n = 0; n < sizeof(SelectHandler::Packet::player) /
                                   sizeof(SelectHandler::Packet::Player);
             n++) {
            SelectHandler::Packet::Player player = selectPacket->player[n];

            SelectHandler::Packet::Character selectedCharacter =
                player.character;
            SelectHandler::Packet::Vehicle selectedVehicle = player.vehicle;
            if (selectedCharacter !=
                    SelectHandler::Packet::Character::NotSelected ||
                selectedVehicle !=
                    SelectHandler::Packet::Vehicle::NotSelected) {
                Character character = static_cast<Character>(selectedCharacter);
                Vehicle vehicle = static_cast<Vehicle>(selectedVehicle);
                if (scenario->isOnlineVersusRace()) {
                    if (!IsCombinationValidVS(character, vehicle)) {
                        return false;
                    }
                } else /* if (scenario->isOnlineBattle()) */ {
                    if (!IsCombinationValidBT(character, vehicle)) {
                        return false;
                    }
                }
            }

            SelectHandler::Packet::CourseVote courseVote =
                selectPacket->player[n].courseVote;
            if (courseVote == SelectHandler::Packet::CourseVote::NotSelected ||
                courseVote == SelectHandler::Packet::CourseVote::Random) {
                continue;
            }
            Course course = static_cast<Course>(courseVote);
            if (scenario->isOnlineVersusRace()) {
                if (!IsRaceCourse(course)) {
                    return false;
                }
            } else /* if (scenario->isOnlineBattle()) */ {
                if (!IsBattleCourse(course)) {
                    return false;
                }
            }
        }

        if (selectPacket->selectedCourse !=
            SelectHandler::Packet::SelectedCourse::NotSelected) {
            Course course = static_cast<Course>(selectPacket->selectedCourse);
            if (scenario->isOnlineVersusRace()) {
                if (!IsRaceCourse(course)) {
                    return false;
                }
            } else /* if (scenario->isOnlineBattle()) */ {
                if (!IsBattleCourse(course)) {
                    return false;
                }
            }
        }
    }

    return true;
}

static bool IsRaceDataPacketDataValid(
    const void* /* packet */, u8 /* packetSize */, u8 /* playerAid */
)
{
    return true;
}

static bool IsUserPacketDataValid(
    const void* packet, u8 /* packetSize */, u8 /* playerAid */
)
{
    using namespace mkw::Net;

    const UserHandler::Packet* userPacket =
        reinterpret_cast<const UserHandler::Packet*>(packet);

    if (!userPacket->isMiiGroupCountValid()) {
        return false;
    }

    if (!userPacket->isVersusRatingValid()) {
        return false;
    }

    if (!userPacket->isBattleRatingValid()) {
        return false;
    }

    return true;
}

static bool
IsItemPacketDataValid(const void* packet, u8 packetSize, u8 /* playerAid */)
{
    using namespace mkw::Item;
    using namespace mkw::Net;
    using namespace mkw::System;

    if (!RKNetController::Instance()->inVanillaMatch()) {
        return true;
    }

    if (static_cast<RKScene::SceneID>(
            RKSystem::Instance().sceneManager()->getCurrentSceneID()
        ) != RKScene::SceneID::Race) {
        return true;
    }

    RaceConfig::Scenario* scenario = &RaceConfig::Instance()->raceScenario();

    for (u8 n = 0; n < (packetSize >> 3); n++) {
        const ItemHandler::Packet* itemPacket =
            reinterpret_cast<const ItemHandler::Packet*>(
                reinterpret_cast<const char*>(packet) +
                (sizeof(ItemHandler::Packet) * n)
            );

        ItemBox heldItem = static_cast<ItemBox>(itemPacket->heldItem);
        ItemBox trailedItem = static_cast<ItemBox>(itemPacket->trailedItem);

        if (scenario->isOnlineVersusRace()) {
            if (!IsHeldItemValidVS(heldItem)) {
                return false;
            }
            if (!IsTrailedItemValidVS(trailedItem)) {
                return false;
            }
        } else /* if (scenario->isOnlineBattle()) */ {
            if (scenario->isBalloonBattle()) {
                if (!IsHeldItemValidBB(heldItem)) {
                    return false;
                }
                if (!IsTrailedItemValidBB(trailedItem)) {
                    return false;
                }
            } else /* if (scenario->isCoinRunners()) */ {
                if (!IsHeldItemValidCR(heldItem)) {
                    return false;
                }
                if (!IsTrailedItemValidCR(trailedItem)) {
                    return false;
                }
            }
        }
    }

    return true;
}

static bool IsEventPacketDataValid(
    const void* packet, u8 packetSize, u8 /* playerAid */
)
{
    using namespace mkw::Net;
    using namespace mkw::System;

    if (static_cast<RKScene::SceneID>(
            RKSystem::Instance().sceneManager()->getCurrentSceneID()
        ) != RKScene::SceneID::Race) {
        return true;
    }

    const EventHandler::Packet* eventPacket =
        reinterpret_cast<const EventHandler::Packet*>(packet);

    // Always ensure that the packet does not contain any invalid item objects,
    // as this can cause a buffer overflow to occur.
    if (eventPacket->containsInvalidItemObject()) {
        return false;
    }

    if (!RKNetController::Instance()->inVanillaMatch()) {
        return true;
    }

    if (!eventPacket->isValid(packetSize)) {
        return false;
    }

    return true;
}

typedef bool (*IsPacketDataValid
)(const void* packet, u8 packetSize, u8 playerAid);

static std::array<IsPacketDataValid, sizeof(mkw::Net::RACEPacket::sizes)>
    s_isPacketDataValid{
        IsHeaderPacketDataValid,      IsRaceHeader1PacketDataValid,
        IsRaceHeader2PacketDataValid, IsRoomSelectPacketDataValid,
        IsRaceDataPacketDataValid,    IsUserPacketDataValid,
        IsItemPacketDataValid,        IsEventPacketDataValid,
    };

// CLIENT TO CLIENT VULNERABILITY
// Patch for Mario Kart Wii RACE exploit. This was the first RCE exploit
// discovered in a Wii game. Originally discovered by XeR, but then
// rediscovered by Star, who reported the exploit and then released it.
// CVE-ID: CVE-2023-35856
// https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2023-35856
static bool IsRACEPacketValid(
    const mkw::Net::RACEPacket* racePacket, u32 racePacketSize, u8 playerAid
)
{
    using namespace mkw::Net;

    if (racePacketSize < sizeof(RACEPacket)) {
        return false;
    }

    u32 expectedPacketSize = 0;
    for (size_t n = 0; n < sizeof(RACEPacket::sizes); n++) {
        RACEPacket::EType packetType = static_cast<RACEPacket::EType>(n);
        u8 packetSize = racePacket->sizes[n];

        if (!IsPacketSizeValid(packetType, packetSize)) {
            return false;
        }

        expectedPacketSize += packetSize;
    }

    if (racePacketSize < expectedPacketSize) {
        return false;
    }

    expectedPacketSize = 0;
    for (size_t n = 0; n < sizeof(RACEPacket::sizes); n++) {
        const IsPacketDataValid isPacketDataValid = s_isPacketDataValid[n];
        const void* packet =
            reinterpret_cast<const char*>(racePacket) + expectedPacketSize;
        u8 packetSize = racePacket->sizes[n];

        if (packetSize != 0 &&
            !isPacketDataValid(packet, packetSize, playerAid)) {
            return false;
        }

        expectedPacketSize += packetSize;
    }

    return true;
}

WWFC_DEFINE_PATCH = {Patch::BranchWithCTR( //
    WWFC_PATCH_LEVEL_CRITICAL, //
    RMCXD_PORT(0x80658604, 0x8065417C, 0x80657C70, 0x8064691C), //
    [](mkw::Net::RKNetController* rkNetController,
       mkw::Net::RACEPacket* racePacket, u32 packetSize, u32 _,
       u8 playerAid) -> void {
        if (!IsRACEPacketValid(racePacket, packetSize, playerAid)) {
            LONGCALL int DWC_CloseConnectionHard(u8 playerAid)
                AT(RMCXD_PORT(0x800D2000, 0x800D1F60, 0x800D1F20, 0x800D2060));
            DWC_CloseConnectionHard(playerAid);

            return;
        }

        rkNetController->processRACEPacket(playerAid, racePacket, packetSize);
    }
)};

#endif

} // namespace wwfc::Security
