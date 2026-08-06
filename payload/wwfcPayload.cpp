#include "wwfcPayload.hpp"
#include "wwfcLibC.hpp"
#include "wwfcLog.hpp"
#include "wwfcLogin.hpp"
#include "wwfcPatch.hpp"
#include "wwfcSupport.hpp"
#include "wwfcTypes.h"
#include "wwfcVersion.h"
#include <wwfcError.h>
#include <wwfcTypes.h>

namespace wwfc::Payload
{

// Define low level symbols

/**
 * Payload entry point. Applies global offset table and fixup relocations, and
 * then calls wwfc_payload_entry_no_got.
 */
s32 Entry(wwfc_payload* payload) asm("wwfc_payload_entry");

/**
 * Payload entry point. Does not apply global offset table and fixup
 * relocations. Automatically called by wwfc_payload_entry.
 */
s32 EntryAfterGOT(wwfc_payload* payload) asm("wwfc_payload_entry_no_got");

/**
 * Payload entry point for consumers that already applied the patch list.
 */
s32 StaticEntry(wwfc_payload* payload) asm("wwfc_static_payload_entry");

s32 FunctionExec(wwfc_function_t function, ...) asm("wwfc_function_exec");

// Symbols provided in the linker script
extern u32 GOTStart asm("_G_GOTStart");
extern u32 GOTEnd asm("_G_GOTEnd");
extern u32 FixupStart asm("_G_FixupStart");
extern u32 FixupEnd asm("_G_FixupEnd");
extern wwfc_patch PatchStart asm("_G_WWFCPatchStart");
extern wwfc_patch PatchEnd asm("_G_WWFCPatchEnd");
extern u32 CTORSStart asm("__CTORS_START__");
extern u32 CTORSEnd asm("__CTORS_END__");
extern u8 ExecutableEnd asm("_G_ExecutableEnd");
extern u8 PayloadEnd asm("_G_End");

[[gnu::section(".wwfc_static_consumer_info")]]
constinit const wwfc_static_consumer_info_ex StaticConsumerInfo = {
    .format_version = 1,
    .patch_free_entry_point = &StaticEntry,
    .ctors_end = &CTORSEnd,
    .executable_end = &ExecutableEnd,
};

[[gnu::section("wwfc_header")]]
constinit const wwfc_payload_ex Header = {
    .header =
        {
            .magic =
                {'W', 'W', 'F', 'C', '/', 'P', 'a', 'y', 'l', 'o', 'a', 'd'},
            .end = &PayloadEnd,
            .signature = {},
        },
    .salt = {},
    .info = {
        .format_version = 3,
        .format_version_compat = 1,
        .name = WWFC_PAYLOAD_NAME,
        .version = (WWFC_PAYLOAD_MAJOR << 24) | (WWFC_PAYLOAD_MINOR << 12) |
                   WWFC_PAYLOAD_BETA,
        .got_start = &GOTStart,
        .got_end = &GOTEnd,
        .fixup_start = &FixupStart,
        .fixup_end = &FixupEnd,
        .patch_list_offset = &PatchStart,
        .patch_list_end = &PatchEnd,
        .entry_point = &Entry,
        .entry_point_no_got = &EntryAfterGOT,
        .function_exec = &FunctionExec,
        .must_be_zero = {},
        .build_timestamp = __TIMESTAMP__,
        .static_consumer_info = &StaticConsumerInfo,
    },
};

/**
 * Payload entry point. Applies global offset table and fixup relocations, and
 * then calls wwfc_payload_entry_no_got.
 */
ASM_FUNCTION( //
    s32 Entry(wwfc_payload* payload), (),
    // clang-format off
    addi    r6, r3, (_G_GOTStart - 4)@l;
    addi    r7, r3, (_G_GOTEnd - 4)@l;
    addi    r8, r3, (_G_FixupEnd - 4)@l;
L%=LoopStart:;
    cmplw   r6, r7;
    bge-    L%=LoopFixupStart;
    lwzu    r9, 0x4(r6);
    rlwinm. r0, r9, 0, 0x80000000;
    bne-    L%=LoopStart;
    add     r9, r9, r3;
    stw     r9, 0(r6);
    b       L%=LoopStart;
L%=LoopFixupStart:;
    cmplw   r6, r8;
    bge-    L%=LoopFixupEnd;
    lwzu    r9, 0x4(r6);
    lwzx    r10, r9, r3;
    rlwinm. r0, r10, 0, 0x80000000;
    bne-    L%=LoopFixupStart;
    add     r10, r10, r3;
    stwx    r10, r9, r3;
    b       L%=LoopFixupStart;
L%=LoopFixupEnd:;
    b       wwfc_payload_entry_no_got;
    // clang-format on
)

extern struct LoMem {
    u32 discId;
    u16 makerCode;
    u8 discNumber;
    u8 discVersion;
} g_LoMem AT(0x80000000);

static void CallCtors(const wwfc_payload* const payload)
{
    void (*ctor)();

    for (const u32* ctorAddress = &CTORSStart; ctorAddress < &CTORSEnd;
         ++ctorAddress) {
        u32 ctorOffset = *ctorAddress;

        if (ctorOffset == 0x00000000 || ctorOffset == 0xFFFFFFFF) {
            continue;
        }

        ctor = (decltype(ctor)) (reinterpret_cast<const char*>(payload) +
                                 ctorOffset);
        (*ctor)();
    }
}

/**
 * Payload entry point. Does not apply global offset table and fixup
 * relocations. Automatically called by wwfc_payload_entry.
 */
static s32 InitializePayload(wwfc_payload* payload, const bool applyPatchList)
{
#if WWFC_TITLE_TYPE == WWFC_TITLE_TYPE_DISC
    // Verify that the current game is the one this payload is built for
    if (g_LoMem.discId != WWFC_GAME_ID) {
        return WL_ERROR_PAYLOAD_GAME_MISMATCH;
    }

    // Check that the disc version matches
    // Don't check for Brawl US or JP because they are identical except for one
    // byte
#  if !RSBED01 && !RSBED02 && !RSBJD00 && !RSBJD01
    if (g_LoMem.discVersion != WWFC_TITLE_VERSION) {
        return WL_ERROR_PAYLOAD_GAME_MISMATCH;
    }
#  endif
#endif

    // "Anticheat"
    // Check for the presence of the gecko codehandler, and halt online
    // connections if it is found
    if (*(u32*) 0x80001920 != 0x0 && *(u32*) 0x80001920 != 0x3C6000D0) {
        if (*(u32*) 0x80001920 != 0xFEE00090 &&
            *(u32*) 0x80001920 != 0x3C841000) {
            return WL_ERROR_PAYLOAD_STAGE1_WAITING;
        }
        if (*(u32*) 0x802588F8 != 0x0 || *(u32*) 0x8000629C != 0x4E800020 ||
            *(u32*) 0x80259198 == 0x9421FF98) {
            return WL_ERROR_PAYLOAD_STAGE1_WAITING;
        }
    }

    WWFC_LOG_NOTICE_FMT(
        "Payload version %u.%u.%u", payload->info.version >> 24,
        (payload->info.version >> 12) & 0xFFF, payload->info.version & 0xFFF
    );

    CallCtors(payload);

    if (applyPatchList) {
        Patch::ApplyPatchList(
            reinterpret_cast<u32>(payload), &PatchStart,
            std::distance(&PatchStart, &PatchEnd)
        );
    }

    Support::ChangeAuthURL();
    Login::Init();

    return WL_ERROR_PAYLOAD_OK;
}

s32 EntryAfterGOT(wwfc_payload* payload)
{
    return InitializePayload(payload, true);
}

s32 StaticEntry(wwfc_payload* payload)
{
    return InitializePayload(payload, false);
}

s32 FunctionExec(wwfc_function_t function, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, function);

    switch (function) {
    case WWFC_FUNCTION_APPLY_PATCH: {
        wwfc_patch* patch = __builtin_va_arg(args, wwfc_patch*);

        Patch::ApplyPatch(reinterpret_cast<u32>(&Header), *patch);
        return 0;
    }

    case WWFC_FUNCTION_GET_VALUE: {
        const wwfc_key_t key = __builtin_va_arg(args, wwfc_key_t);

        switch (key) {
#define CASE(_KEY, _VALUE)                                                     \
    case _KEY:                                                                 \
        return _VALUE

            CASE(
                WWFC_KEY_ENABLE_AGGRESSIVE_PACKET_CHECKS,
                g_enableAggressivePacketChecks
            );
#if RMC
            CASE(
                WWFC_KEY_MKW_ENABLE_EVENT_ITEM_ID_CHECK,
                g_enableEventItemIdCheck
            );
            CASE(WWFC_KEY_MKW_ENABLE_ULTRA_UNCUT, g_enableUltraUncut);
#endif
#undef CASE
        }
        return -1;
    }

    case WWFC_FUNCTION_SET_VALUE: {
        const wwfc_key_t key = __builtin_va_arg(args, wwfc_key_t);

        switch (key) {
#define CASE(_KEY, _VALUE)                                                     \
    case _KEY:                                                                 \
        _VALUE =                                                               \
            __builtin_va_arg(args, std::remove_cvref_t<decltype(_VALUE)>);     \
        return 0

            CASE(
                WWFC_KEY_ENABLE_AGGRESSIVE_PACKET_CHECKS,
                g_enableAggressivePacketChecks
            );
#if RMC
            CASE(
                WWFC_KEY_MKW_ENABLE_EVENT_ITEM_ID_CHECK,
                g_enableEventItemIdCheck
            );
            CASE(WWFC_KEY_MKW_ENABLE_ULTRA_UNCUT, g_enableUltraUncut);
#endif
#undef CASE
        }
        return -1;
    }
    }

    return -1;
}

} // namespace wwfc::Payload
