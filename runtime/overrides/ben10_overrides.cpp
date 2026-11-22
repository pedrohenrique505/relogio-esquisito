// Game overrides de Ben 10: Protector of Earth (SLUS_216.61, NTSC-U v1.01).
//
// Convenção dos handlers: ctx->pc = $ra ANTES de chamar qualquer HLE do runtime,
// como fazem os stubs gerados pelo ps2_recomp.

#include "game_overrides.h"
#ifdef BEN10_HAS_PARALLEL_GS
#include "ben10_parallel_gs.h"
#endif
#include "ps2_runtime.h"
#include "ps2_iop_transport.h"
#include "ps2_runtime_calls.h"
#include "ps2_stubs.h"
#include "ps2_syscalls.h"
#include "runtime/ee_scheduler.h"
#include "runtime/ps2_memory.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace
{
    constexpr uint32_t kEntry = 0x00100008u;
    constexpr uint32_t kElfCrc32 = 0xD7C918F3u;

    // 0x001C53A8: write(fd, buf, len) da libkernel para fd 1/2 (stdout/stderr).
    // Evita travamento na TTY DECI2 sem devkit; envia direto para o host.
    void ttyWrite(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        (void)runtime;
        const int32_t fd = static_cast<int32_t>(getRegU32(ctx, 4));
        const uint32_t buf = getRegU32(ctx, 5);
        const int32_t len = static_cast<int32_t>(getRegU32(ctx, 6));

        const uint8_t *host = (fd == 1 || fd == 2) ? getConstMemPtr(rdram, buf) : nullptr;
        if (!host || len < 0)
        {
            setReturnS32(ctx, -1);
        }
        else
        {
            std::fwrite(host, 1, static_cast<size_t>(len), fd == 2 ? stderr : stdout);
            std::fflush(fd == 2 ? stderr : stdout);
            setReturnS32(ctx, len);
        }
        ctx->pc = getRegU32(ctx, 31);
    }

    // 0x001C82B0: sceSifBindRpc(client, sid, mode).
    // No bind bloqueante (mode 0), avança o tempo EE/IOP até o servidor IOP registrar o SID.
    void sifBindRpcBlocking(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t sid = getRegU32(ctx, 5);
        const uint32_t mode = getRegU32(ctx, 6);

        if (runtime && (mode & 1u) == 0u)
        {
            constexpr uint64_t kSliceCycles = 294912000ull / 1000ull; // 1 ms de EE
            constexpr int kMaxSlices = 2000;
            int slices = 0;
            while (!PS2IopTransport::canBindRpc(runtime, sid) && slices < kMaxSlices)
            {
                runtime->eeScheduler().accountCycles(static_cast<uint32_t>(kSliceCycles));
                ++slices;
            }
            if (slices > 0)
            {
                std::fprintf(stderr, "[ben10] sceSifBindRpc sid=0x%X: esperou %d ms pelo IOP%s\n",
                             sid, slices, slices >= kMaxSlices ? " (TIMEOUT)" : "");
            }
        }

        ctx->pc = getRegU32(ctx, 31);
        ps2_stubs::sceSifBindRpc(rdram, ctx, runtime);
    }

    // Diagnóstico (BEN10_TRACE_RPC=1): registra chamadas sceSifCallRpc.
    void sifCallRpcTraced(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        static const bool enabled = [] {
            const char *v = std::getenv("BEN10_TRACE_RPC");
            return v && *v && *v != '0';
        }();
        static uint64_t entries = 0;
        ++entries;

        const uint32_t client = getRegU32(ctx, 4);
        const uint32_t fno = getRegU32(ctx, 5);
        const uint32_t mode = getRegU32(ctx, 6);
        const uint32_t ra = getRegU32(ctx, 31);
        const auto t0 = std::chrono::steady_clock::now();

        if (enabled && (entries <= 60 || entries % 2000 == 0))
        {
            std::fprintf(stderr, "[ben10:rpc] -> #%llu client=0x%X fno=0x%X mode=%u ra=0x%X\n",
                         static_cast<unsigned long long>(entries), client, fno, mode, ra);
        }

        ctx->pc = ra;
        ps2_syscalls::sceSifCallRpc(rdram, ctx, runtime);

        if (enabled && (entries <= 60 || entries % 2000 == 0))
        {
            const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now() - t0)
                                .count();
            std::fprintf(stderr, "[ben10:rpc] <- #%llu client=0x%X fno=0x%X ret=%d %lldus\n",
                         static_cast<unsigned long long>(entries), client, fno,
                         static_cast<int32_t>(getRegU32(ctx, 2)), static_cast<long long>(us));
        }
    }

    struct SdkBinding
    {
        uint32_t address;
        const char *handler;
    };

    constexpr SdkBinding kSdkBindings[] = {
#include "ben10_sdk_bindings.inc"
    };

    void configureDiscImage()
    {
        const char *iso = std::getenv("BEN10_ISO");
        if (!iso || !*iso)
        {
            return;
        }
        PS2Runtime::IoPaths paths = PS2Runtime::getIoPaths();
        paths.cdImage = iso;
        PS2Runtime::setIoPaths(paths);
        std::fprintf(stderr, "[ben10] cdImage = %s\n", iso);
    }

    std::optional<PS2Runtime::RecompiledFunction> resolveHandler(std::string_view name)
    {
        const std::string_view syscall = ps2_runtime_calls::resolveSyscallName(name);
        if (!syscall.empty())
        {
#define BEN10_RESOLVE_SYSCALL(n) \
    if (syscall == std::string_view{#n}) return &ps2_syscalls::n;
            PS2_SYSCALL_LIST(BEN10_RESOLVE_SYSCALL)
#undef BEN10_RESOLVE_SYSCALL
        }
        const std::string_view stub = ps2_runtime_calls::resolveStubName(name);
        if (!stub.empty())
        {
#define BEN10_RESOLVE_STUB(n) \
    if (stub == std::string_view{#n}) return &ps2_stubs::n;
            PS2_STUB_LIST(BEN10_RESOLVE_STUB)
#undef BEN10_RESOLVE_STUB
        }
        return std::nullopt;
    }

    std::unordered_map<uint32_t, PS2Runtime::RecompiledFunction> &sdkHandlers()
    {
        static std::unordered_map<uint32_t, PS2Runtime::RecompiledFunction> handlers;
        return handlers;
    }

    void sdkTrampoline(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t entry = ctx->pc;
        const auto it = sdkHandlers().find(entry);
        ctx->pc = getRegU32(ctx, 31);
        if (it == sdkHandlers().end())
        {
            std::fprintf(stderr, "[ben10] trampolim sem handler para 0x%08X\n", entry);
            setReturnS32(ctx, -1);
            return;
        }
        it->second(rdram, ctx, runtime);
    }

    void apply(PS2Runtime &runtime)
    {
#ifdef BEN10_HAS_PARALLEL_GS
        ben10_parallel_gs::installIfRequested(runtime);
#endif
        configureDiscImage();

        size_t bound = 0;
        for (const SdkBinding &b : kSdkBindings)
        {
            const auto handler = resolveHandler(b.handler);
            if (!handler)
            {
                std::fprintf(stderr, "[ben10] handler '%s' não existe no runtime (0x%08X)\n", b.handler, b.address);
                continue;
            }
            sdkHandlers()[b.address] = *handler;
            if (runtime.replaceFunction(b.address, sdkTrampoline))
            {
                ++bound;
            }
        }
        std::fprintf(stderr, "[ben10] %zu/%zu SDK bindings aplicados\n", bound, std::size(kSdkBindings));

        // Overrides específicos (depois dos bindings genéricos do SDK)
        runtime.replaceFunction(0x001C53A8u, ttyWrite);
        runtime.replaceFunction(0x001C82B0u, sifBindRpcBlocking);
        runtime.replaceFunction(0x001C8490u, sifCallRpcTraced);
    }
} // namespace

PS2_REGISTER_GAME_OVERRIDE("Ben 10: Protector of Earth (USA)", "SLUS_216.61", kEntry, kElfCrc32, apply);
