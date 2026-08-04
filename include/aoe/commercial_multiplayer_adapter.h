#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Stable SDK-independent C ABI. A locally built platform adapter exports
// aoe_commercial_multiplayer_adapter_v1 and returns this table.
typedef struct AoeCommercialMultiplayerAdapterV1 {
    uint32_t abi_version;
    void* context;
    void (*destroy)(void*);
    uint64_t (*user_id)(void*);
    const char* (*display_name)(void*);
    size_t (*ticket)(void*, uint8_t*, size_t);
    int (*authenticate)(void*, uint64_t, const uint8_t*, size_t);
    uint64_t (*create_lobby)(void*, size_t);
    int (*join_lobby)(void*, uint64_t);
    void (*leave_lobby)(void*, uint64_t);
    int (*set_metadata)(void*, uint64_t, const char*, const char*);
    int (*transfer_owner)(void*, uint64_t, uint64_t);
    int (*send)(void*, uint64_t, const void*, size_t, int);
    size_t (*receive)(void*, uint64_t*, void*, size_t, int);
} AoeCommercialMultiplayerAdapterV1;

typedef AoeCommercialMultiplayerAdapterV1*
(*AoeCreateCommercialMultiplayerAdapterV1)(void);

#ifdef __cplusplus
}
#endif
