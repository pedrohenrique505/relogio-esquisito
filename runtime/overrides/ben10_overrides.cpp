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
#include "ps2_recompiled_functions.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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

    // 0x001CA248: _sceSifLoadModule(path, arglen, args, ...).
    // Os módulos padrão da Sony (SIO2MAN/MTAPMAN/PADMAN/MCMAN/MCSERV) já têm
    // HLE no ps2xRuntime; rodá-los no emulador IOP faz threads de SIO2 ficarem
    // em busy-wait queimando ~98% da CPU. Mantemos apenas LIBSD, CDARB e ALASD no IOP.
    void sifLoadModuleFiltered(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t pathAddr = getRegU32(ctx, 4);
        const char *path = reinterpret_cast<const char *>(getConstMemPtr(rdram, pathAddr));
        if (path)
        {
            std::string_view sv(path);
            auto hasIcase = [&](std::string_view needle) {
                if (needle.size() > sv.size()) return false;
                for (size_t i = 0; i + needle.size() <= sv.size(); ++i) {
                    bool ok = true;
                    for (size_t j = 0; j < needle.size(); ++j) {
                        char a = sv[i + j];
                        char b = needle[j];
                        if (a >= 'a' && a <= 'z') a -= 32;
                        if (b >= 'a' && b <= 'z') b -= 32;
                        if (a != b) { ok = false; break; }
                    }
                    if (ok) return true;
                }
                return false;
            };
            if (hasIcase("SIO2MAN.IRX") || hasIcase("MTAPMAN.IRX") ||
                hasIcase("PADMAN.IRX") || hasIcase("MCMAN.IRX") ||
                hasIcase("MCSERV.IRX"))
            {
                static int32_t fakeModId = 100;
                const int32_t id = fakeModId++;
                std::fprintf(stderr, "[ben10] _sceSifLoadModule HLE-skip '%s' -> id=%d\n", path, id);
                ctx->pc = getRegU32(ctx, 31);
                setReturnS32(ctx, id);
                return;
            }
        }
        ctx->pc = getRegU32(ctx, 31);
        ps2_syscalls::SifLoadModule(rdram, ctx, runtime);
    }

    // 0x001C3920: sceCdSearchFile(sceCdlFILE *fp, const char *path).
    // O HLE ps2_stubs::sceCdSearchFile retorna LSNs sintéticos (0x100000+), mas
    // CDARB.IRX no IOP lê setores reais da ISO (BEN10_ISO). Quando BEN10_ISO
    // está definida, consultamos o diretório ISO-9660 real para devolver o LSN exato.
    bool lookupIso9660File(const std::string &isoPath, std::string_view rawPath,
                           uint32_t &outLsn, uint32_t &outSize, std::string &outName)
    {
        std::ifstream f(isoPath, std::ios::binary);
        if (!f)
            return false;
        auto readU32LE = [](const uint8_t *p) -> uint32_t {
            return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
        };
        uint8_t pvd[2048];
        f.seekg(16LL * 2048LL);
        if (!f.read(reinterpret_cast<char *>(pvd), sizeof(pvd)))
            return false;
        uint32_t dirLba = readU32LE(pvd + 156 + 2);
        uint32_t dirSize = readU32LE(pvd + 156 + 10);

        // Normaliza: remove prefixo cdrom0:, barras iniciais e ;1
        std::string norm(rawPath);
        auto up = [](std::string s) {
            for (char &c : s)
                if (c >= 'a' && c <= 'z') c = char(c - 32);
            return s;
        };
        norm = up(norm);
        if (norm.rfind("CDROM0:", 0) == 0)
            norm.erase(0, 7);
        for (char &c : norm)
            if (c == '/') c = '\\';
        while (!norm.empty() && norm.front() == '\\')
            norm.erase(0, 1);
        if (auto semi = norm.find(';'); semi != std::string::npos)
            norm.resize(semi);

        // Separa em componentes de diretório
        std::vector<std::string> parts;
        size_t start = 0;
        while (start < norm.size())
        {
            size_t slash = norm.find('\\', start);
            if (slash == std::string::npos)
            {
                parts.push_back(norm.substr(start));
                break;
            }
            if (slash > start)
                parts.push_back(norm.substr(start, slash - start));
            start = slash + 1;
        }
        if (parts.empty())
            return false;

        for (size_t idx = 0; idx < parts.size(); ++idx)
        {
            const bool isLast = (idx + 1 == parts.size());
            std::vector<uint8_t> buf(dirSize);
            f.seekg(int64_t(dirLba) * 2048LL);
            if (!f.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(dirSize)))
                return false;
            bool matched = false;
            size_t pos = 0;
            while (pos < buf.size())
            {
                uint8_t recLen = buf[pos];
                if (recLen == 0)
                {
                    pos = ((pos / 2048u) + 1u) * 2048u;
                    continue;
                }
                if (pos + 33u > buf.size())
                    break;
                uint8_t nameLen = buf[pos + 32];
                if (pos + 33u + nameLen > buf.size())
                    break;
                std::string entryName(reinterpret_cast<const char *>(buf.data() + pos + 33), nameLen);
                std::string cmpName = up(entryName);
                if (auto semi = cmpName.find(';'); semi != std::string::npos)
                    cmpName.resize(semi);
                if (cmpName == parts[idx])
                {
                    uint32_t lba = readU32LE(buf.data() + pos + 2);
                    uint32_t sz = readU32LE(buf.data() + pos + 10);
                    if (isLast)
                    {
                        outLsn = lba;
                        outSize = sz;
                        outName = entryName;
                        return true;
                    }
                    dirLba = lba;
                    dirSize = sz;
                    matched = true;
                    break;
                }
                pos += recLen;
            }
            if (!matched)
                return false;
        }
        return false;
    }

    void cdSearchFileRealIso(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t fileAddr = getRegU32(ctx, 4);
        const uint32_t pathAddr = getRegU32(ctx, 5);
        const char *isoEnv = std::getenv("BEN10_ISO");
        const char *pathPtr = reinterpret_cast<const char *>(getConstMemPtr(rdram, pathAddr));
        if (isoEnv && *isoEnv && pathPtr && *pathPtr)
        {
            uint32_t lsn = 0, size = 0;
            std::string entryName;
            if (lookupIso9660File(isoEnv, pathPtr, lsn, size, entryName))
            {
                uint8_t *dst = getMemPtr(rdram, fileAddr);
                if (dst)
                {
                    std::memset(dst, 0, 32);
                    std::memcpy(dst + 0, &lsn, 4);
                    std::memcpy(dst + 4, &size, 4);
                    const size_t n = std::min<size_t>(entryName.size(), 15);
                    std::memcpy(dst + 8, entryName.data(), n);
                    dst[8 + n] = '\0';
                    std::fprintf(stderr, "[ben10] sceCdSearchFile(ISO) \"%s\" -> lsn=0x%X size=0x%X\n",
                                 pathPtr, lsn, size);
                    ctx->pc = getRegU32(ctx, 31);
                    setReturnS32(ctx, 1);
                    return;
                }
            }
        }
        ctx->pc = getRegU32(ctx, 31);
        ps2_stubs::sceCdSearchFile(rdram, ctx, runtime);
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
        // Retoma o loop após o checkpoint mantendo a tabela completa do JR em
        // 0x185BFC. A entrada sintética termina antes de destinos como 0x186360.
        runtime.replaceFunction(0x00185BC8u, FUN_00185ba0_0x185ba0);
        // 0x14b820 ficou 8 bytes dentro de FUN_0014b818 (que começa com addiu $sp,+0x10; nop)
        runtime.replaceFunction(0x0014B820u, [](uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt) {
            ctx->r[29] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(getRegU32(ctx, 29) - 0x10u)));
            FUN_0014b818_0x14b818(rdram, ctx, rt);
        });
        // 0x1a4c58: thunk de vtable `j 0x001a5c18; nop` (__pure_virtual)
        runtime.replaceFunction(0x001A4C58u, FUN_001a5c18_0x1a5c18);
        runtime.replaceFunction(0x001C3920u, cdSearchFileRealIso);
        runtime.replaceFunction(0x001C53A8u, ttyWrite);
        runtime.replaceFunction(0x001C82B0u, sifBindRpcBlocking);
        runtime.replaceFunction(0x001C8490u, sifCallRpcTraced);
        runtime.replaceFunction(0x001CA248u, sifLoadModuleFiltered);
    }
} // namespace

PS2_REGISTER_GAME_OVERRIDE("Ben 10: Protector of Earth (USA)", "SLUS_216.61", kEntry, kElfCrc32, apply);
