// Script mods for the multiplayer (TU6): a GSC loader and the script functions Bot Warfare needs.
// Ported from mw2nx's mw2mp_gsc.cpp (which follows CoD Xe's mp_tu6); the addresses are the same default_mp.xex's.
//
//   <mods root>/<mod name>/...      the game's script paths inside it, e.g. maps/mp/gametypes/_callbacksetup.gsc
//   mods root:  $MW2_MODS, else <app>/Contents/Resources/mods (macOS) or <exe folder>/mods
//   mod name:   $MW2_MOD, else "bot_warfare"  ("none" turns the mod off)
//
// - Scr_AddSourceBuffer (8229F2C8) reads each script by its game path; a file at that path in the mod replaces the
//   game's or adds a new one.
// - Scr_GetFunction / Scr_GetMethod (82254C38 / 82254D88) are asked for every script function the compiler does not
//   know; ours answer first: printconsole, fileread, filewrite, fileexists, openfile, readstream, closefile,
//   addtestclient and the bot methods botaction, botstop, botmovement, botmeleeparams, botremoteangles, botangles.
// - SV_BotUserMove (822C3208) builds a test client's usercmd each server frame, here from what the bot methods set.
//
// This replaces the runtime's own bots (runtime/bots/bots.cpp is a stub that keeps the settings menu linking).
#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <string>
#include <system_error>
#include <strings.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include "ppc_recomp_shared.h"
#include "../title.h"

#if defined(MW2_TITLE_MP) && !defined(MW2_VERSION_TU0)

// The hooked functions: their originals, then (further down) their replacements.
PPC_FUNC_IMPL(__imp__sub_8229F2C8);  // Scr_AddSourceBuffer(filename, extFilename) -> char*
PPC_FUNC_IMPL(__imp__sub_82254C38);  // Scr_GetFunction(const char** name, int* type)
PPC_FUNC_IMPL(__imp__sub_82254D88);  // Scr_GetMethod(const char** name, int* type)
PPC_FUNC_IMPL(__imp__sub_822C3208);  // SV_BotUserMove(client_t*)
PPC_FUNC_IMPL(__imp__sub_822C3B10);  // SV_IsClientBot(clientNum)
PPC_FUNC_IMPL(__imp__sub_82261410);  // G_SelectWeaponIndex(clientNum, weaponIndex)
PPC_FUNC_IMPL(__imp__sub_822A36E8);  // Scr_ShutdownSystem(sys)

namespace
{

// Game functions that are only called.
#define GAME_FN(name, address) [[maybe_unused]] PPCFunc* const name = sub_##address
GAME_FN(Hunk_AllocateTempMemoryHighInternal, 822FDF08);  // (size)
GAME_FN(Scr_RegisterFunction, 822963C0);                 // (func, name)
GAME_FN(Scr_GetNumParam, 822ADC88);                      // ()
GAME_FN(Scr_GetString, 822B33A8);                        // (i)
GAME_FN(Scr_GetInt, 822B2D70);                           // (i)
GAME_FN(Scr_GetFloat, 822B30D0);                         // (i) -> f1
GAME_FN(Scr_AddInt, 822ADD18);                           // (v)
GAME_FN(Scr_AddUndefined, 822ADE98);                     // ()
GAME_FN(Scr_AddString, 822ADFF0);                        // (s)
GAME_FN(Scr_AddEntity, 82259A60);                        // (ent)
GAME_FN(Scr_Error, 822AE470);                            // (msg)
GAME_FN(Scr_ParamError, 822AE600);                       // (i, msg)
GAME_FN(GetPlayerEntity, 8223F540);                      // (entref) -> gentity*
GAME_FN(Com_Printf, 8227F448);                           // (channel, fmt, ...)
GAME_FN(SV_AddTestClient, 822BDCB8);                     // () -> gentity*
GAME_FN(Info_SetValueForKey, 823167D8);                  // (s, key, value)
GAME_FN(SV_UserinfoChanged, 822BBED8);                   // (client_t*)
GAME_FN(SV_ClientThink, 822BC778);                       // (client_t*, usercmd_s*)

// Guest data (TU6).
constexpr uint32_t kGEntities = 0x82E2A580, kGEntitySize = 640;  // g_entities
constexpr uint32_t kLevel = 0x82FF2F08;                           // level_locals_t: gclient_s* clients at +0
constexpr uint32_t kGClientSize = 0x3700;                         // gclient_s, playerState_s at +0
constexpr uint32_t kSvs = 0x8367A010;                             // time +0, clientCount +8, clients +0xC
constexpr uint32_t kClientSize = 0x97F80;                         // client_t
constexpr uint32_t kClientDeltaMessage = 0x8, kClientOutgoingSequence = 0x18, kClientUserinfo = 0x650,
                   kClientGentity = 0x21294;
constexpr uint32_t kPsDeltaAngles = 0x60, kPsViewAngles = 0x10C, kPsOffHand = 0x2A8, kPsWeapon = 0x2B4,
                   kPsPrimaryForAlt = 0x2B8;
constexpr uint32_t kSvBotUserMoveActivateCheck = 0x82263650;  // Player_ActivateHoldCmd's return from SV_IsClientBot
constexpr int kMaxClients = 18;

void Log(const char* fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    std::fprintf(stderr, "[gsc] ");
    std::vfprintf(stderr, fmt, ap);
    std::fprintf(stderr, "\n");
    va_end(ap);
}

// The guest keeps everything big-endian.
uint32_t Load32(uint8_t* base, uint32_t a)
{
    uint32_t v;
    std::memcpy(&v, base + a, 4);
    return __builtin_bswap32(v);
}
float LoadF(uint8_t* base, uint32_t a)
{
    const uint32_t v = Load32(base, a);
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}
template <typename T>
void Store(uint8_t* base, uint32_t a, T v)
{
    if constexpr (sizeof(T) == 1)
    {
        base[a] = uint8_t(v);
    }
    else if constexpr (sizeof(T) == 2)
    {
        uint16_t u;
        std::memcpy(&u, &v, 2);
        u = __builtin_bswap16(u);
        std::memcpy(base + a, &u, 2);
    }
    else
    {
        static_assert(sizeof(T) == 4);
        uint32_t u;
        std::memcpy(&u, &v, 4);
        u = __builtin_bswap32(u);
        std::memcpy(base + a, &u, 4);
    }
}
const char* GuestStr(uint8_t* base, uint32_t a) { return a ? reinterpret_cast<const char*>(base + a) : ""; }

// Guest calls from a hook: scratch memory below the caller's stack pointer, the callee's frame below that.
struct Guest
{
    PPCContext& parent;
    uint8_t* base;
    uint32_t top;
    Guest(PPCContext& ctx, uint8_t* b) : parent(ctx), base(b), top(ctx.r1.u32 - 0x100) {}
    uint32_t Alloc(uint32_t n)
    {
        top = (top - n) & ~15u;
        std::memset(base + top, 0, n);
        return top;
    }
    uint32_t Str(const char* s)
    {
        const uint32_t n = uint32_t(std::strlen(s)) + 1, a = Alloc(n);
        std::memcpy(base + a, s, n);
        return a;
    }
    struct Result
    {
        uint64_t r3;
        double f1;
    };
    Result Call(PPCFunc* fn, uint64_t r3 = 0, uint64_t r4 = 0, uint64_t r5 = 0)
    {
        PPCContext c = parent;
        c.r1.u32 = top - 0x80;
        c.r3.u64 = r3;
        c.r4.u64 = r4;
        c.r5.u64 = r5;
        fn(c, base);
        return {c.r3.u64, c.f1.f64};
    }
    uint32_t Call32(PPCFunc* fn, uint64_t r3 = 0, uint64_t r4 = 0, uint64_t r5 = 0)
    {
        return uint32_t(Call(fn, r3, r4, r5).r3);
    }
    // Scr_Error does not return in the game; if it ever does, the caller must stop right after.
    void Error(const char* msg) { Call(Scr_Error, Str(msg)); }
    void ParamError(uint32_t i, const char* msg) { Call(Scr_ParamError, i, Str(msg)); }
    uint32_t NumParam() { return Call32(Scr_GetNumParam); }
    const char* String(uint32_t i) { return GuestStr(base, Call32(Scr_GetString, i)); }
    int32_t Int(uint32_t i) { return int32_t(Call32(Scr_GetInt, i)); }
    float Float(uint32_t i) { return float(Call(Scr_GetFloat, i).f1); }
    void AddInt(int32_t v) { Call(Scr_AddInt, uint32_t(v)); }
    void AddUndefined() { Call(Scr_AddUndefined); }
    void AddString(const char* s) { Call(Scr_AddString, Str(s)); }
};

// --- Mod files ------------------------------------------------------------------------------------------------------

std::filesystem::path ExeDir()
{
#if defined(__APPLE__)
    char buf[4096];
    uint32_t n = sizeof(buf);
    if (_NSGetExecutablePath(buf, &n) != 0) return {};
    std::error_code ec;
    const auto real = std::filesystem::canonical(buf, ec);
    return (ec ? std::filesystem::path(buf) : real).parent_path();
#else
    std::error_code ec;
    return std::filesystem::read_symlink("/proc/self/exe", ec).parent_path();
#endif
}

// The mod's folder, empty when there is none.
const std::string& ModDir()
{
    static const std::string dir = []() -> std::string
    {
        std::string name = "bot_warfare";
        if (const char* e = std::getenv("MW2_MOD")) name = e;
        if (name.empty() || name == "none") return {};
        if (name.find_first_of("/\\:") != std::string::npos || name == "." || name == "..")
        {
            Log("MW2_MOD \"%s\": not a folder name", name.c_str());
            return {};
        }
        std::filesystem::path root;
        if (const char* e = std::getenv("MW2_MODS"))
        {
            root = e;
        }
        else
        {
#if defined(__APPLE__)
            root = ExeDir().parent_path() / "Resources" / "mods";
#else
            root = ExeDir() / "mods";
#endif
        }
        const auto path = root / name;
        std::error_code ec;
        if (!std::filesystem::is_directory(path, ec))
        {
            Log("mod \"%s\": no folder %s", name.c_str(), path.string().c_str());
            return {};
        }
        Log("mod \"%s\" (%s)", name.c_str(), path.string().c_str());
        return path.string();
    }();
    return dir;
}

// A path inside the mod for a script-supplied relative path; false for none, absolute or escaping ones.
bool ModPath(const char* relative, char (&out)[512])
{
    const std::string& dir = ModDir();
    if (dir.empty() || !*relative || *relative == '/' || *relative == '\\' || std::strchr(relative, ':') ||
        std::strstr(relative, ".."))
    {
        return false;
    }
    const int n = std::snprintf(out, sizeof(out), "%s/%s", dir.c_str(), relative);
    return n > 0 && n < int(sizeof(out));
}

// --- Script functions -----------------------------------------------------------------------------------------------

FILE* g_stream = nullptr;               // openfile/readstream/closefile: one file at a time
constexpr uint32_t kTextSize = 65536;   // fileread's contents (64 KiB at most)

void CloseStream()
{
    if (g_stream) std::fclose(g_stream);
    g_stream = nullptr;
}

void GScr_PrintConsole(PPCContext& ctx, uint8_t* base)
{
    Guest g(ctx, base);
    if (g.NumParam() != 1) return g.Error("Usage: printconsole(<string>)");
    const char* text = g.String(0);
    Log("%s", text);
    g.Call(Com_Printf, 0, g.Str("%s"), g.Str(text));
}

void GScr_FileWrite(PPCContext& ctx, uint8_t* base)
{
    Guest g(ctx, base);
    if (g.NumParam() != 3) return g.Error("Usage: filewrite(<file>, <contents>, <mode>)");
    const char* mode = g.String(2);
    const char* fmode = !strcasecmp(mode, "write") ? "wb" : !strcasecmp(mode, "append") ? "ab" : nullptr;
    if (!fmode) return g.ParamError(2, "filewrite: mode must be \"write\" or \"append\"");
    char path[512];
    bool ok = false;
    if (ModPath(g.String(0), path))
    {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
        if (FILE* f = std::fopen(path, fmode))
        {
            const char* contents = g.String(1);
            const size_t n = std::strlen(contents);
            ok = std::fwrite(contents, 1, n, f) == n;
            std::fclose(f);
        }
    }
    g.AddInt(ok);
}

void GScr_FileRead(PPCContext& ctx, uint8_t* base)
{
    Guest g(ctx, base);
    if (g.NumParam() != 1) return g.Error("Usage: fileread(<file>)");
    char path[512];
    FILE* f = ModPath(g.String(0), path) ? std::fopen(path, "rb") : nullptr;
    // Scr_AddString copies the text into the script strings, so the temporary hunk is enough for the buffer.
    const uint32_t guest = f ? g.Call32(Hunk_AllocateTempMemoryHighInternal, kTextSize) : 0;
    if (!guest)
    {
        if (f) std::fclose(f);
        return g.AddUndefined();
    }
    char* text = reinterpret_cast<char*>(base + guest);
    const size_t n = std::fread(text, 1, kTextSize - 1, f);
    std::fclose(f);
    text[n] = '\0';
    g.Call(Scr_AddString, guest);
}

void GScr_FileExists(PPCContext& ctx, uint8_t* base)
{
    Guest g(ctx, base);
    if (g.NumParam() != 1) return g.Error("Usage: fileexists(<file>)");
    char path[512];
    std::error_code ec;
    g.AddInt(ModPath(g.String(0), path) && std::filesystem::is_regular_file(path, ec));
}

void GScr_OpenFile(PPCContext& ctx, uint8_t* base)
{
    Guest g(ctx, base);
    if (g.NumParam() != 2) return g.Error("Usage: openfile(<file>, <mode>)");
    char path[512];
    if (strcasecmp(g.String(1), "read") || g_stream || !ModPath(g.String(0), path) || !(g_stream = std::fopen(path, "r")))
    {
        return g.AddInt(-1);
    }
    g.AddInt(1);
}

void GScr_ReadStream(PPCContext& ctx, uint8_t* base)
{
    Guest g(ctx, base);
    if (g.NumParam() != 0) return g.Error("Usage: readstream()");
    if (!g_stream) return g.AddUndefined();
    const uint32_t guest = g.Alloc(4096);
    if (!std::fgets(reinterpret_cast<char*>(base + guest), 4096, g_stream)) return g.AddUndefined();
    g.Call(Scr_AddString, guest);
}

void GScr_CloseFile(PPCContext& ctx, uint8_t* base)
{
    Guest g(ctx, base);
    if (g.NumParam() != 0) return g.Error("Usage: closefile()");
    if (!g_stream) return g.AddInt(-1);
    const int rc = std::fclose(g_stream);
    g_stream = nullptr;
    g.AddInt(rc);
}

// --- Bots -----------------------------------------------------------------------------------------------------------

struct BotInput
{
    bool active, has_move, has_angles, has_remote_angles;
    uint32_t buttons;
    uint16_t weapon;
    int8_t forward, right;
    int8_t remote_angles[2];
    float angles[3];
    float melee_yaw;
    uint8_t melee_dist;
};
BotInput g_bots[kMaxClients];

int8_t ClampMove(int v) { return int8_t(v < -127 ? -127 : v > 127 ? 127 : v); }
uint32_t PlayerState(uint8_t* base, int num) { return Load32(base, kLevel) + uint32_t(num) * kGClientSize; }

// GetPlayerEntity checks the entity is a player (or raises the script error); the slot is its entity number.
// -1 after an error: the caller returns.
int BotSlot(Guest& g, const char* usage, uint32_t params)
{
    const uint32_t ent = g.Call32(GetPlayerEntity, g.parent.r3.u32);
    if (g.NumParam() != params)
    {
        g.Error(usage);
        return -1;
    }
    const int num = ent >= kGEntities ? int((ent - kGEntities) / kGEntitySize) : -1;
    if (num < 0 || num >= kMaxClients)
    {
        g.Error("bot method: not a client");
        return -1;
    }
    return num;
}

struct BotAction
{
    const char* name;
    uint32_t buttons;
};
constexpr BotAction kBotActions[] = {
    {"gostand", 1u << 10},  {"gocrouch", 1u << 9},    {"goprone", 1u << 8},     {"fire", 1u << 0},
    {"attack", 1u << 0},    {"melee", 1u << 2},       {"frag", 1u << 14},       {"smoke", 1u << 15},
    {"reload", 1u << 4},    {"sprint", 1u << 1},      {"leanleft", 1u << 6},    {"leanright", 1u << 7},
    {"ads", 0x80800},       {"speed_throw", 0x80800}, {"holdbreath", 1u << 13}, {"usereload", 1u << 5},
    {"activate", 1u << 3},  {"use", 1u << 3},         {"remote", 1u << 20},     {"crouch", 1u << 9},
    {"prone", 1u << 8},
};

void PlayerCmd_BotAction(PPCContext& ctx, uint8_t* base)
{
    Guest g(ctx, base);
    const int num = BotSlot(g, "Usage: <bot> botAction(<action>);", 1);
    if (num < 0) return;
    const char* action = g.String(0);
    if (action[0] != '+' && action[0] != '-') return g.ParamError(0, "Sign for bot action must be '+' or '-'.");
    for (const auto& a : kBotActions)
    {
        if (!strcasecmp(action + 1, a.name))
        {
            g_bots[num].buttons = action[0] == '+' ? g_bots[num].buttons | a.buttons : g_bots[num].buttons & ~a.buttons;
            g_bots[num].active = true;
            return;
        }
    }
    g.ParamError(0, "Unknown bot action.");
}

void PlayerCmd_BotStop(PPCContext& ctx, uint8_t* base)
{
    Guest g(ctx, base);
    const int num = BotSlot(g, "Usage: <bot> botStop();", 0);
    if (num < 0) return;
    const uint32_t ps = PlayerState(base, num);
    BotInput& b = g_bots[num];
    b = BotInput{};
    b.active = true;
    for (int i = 0; i < 3; ++i) b.angles[i] = LoadF(base, ps + kPsViewAngles + 4 * i);
    b.weapon = uint16_t(Load32(base, ps + kPsWeapon));
}

void PlayerCmd_BotMovement(PPCContext& ctx, uint8_t* base)
{
    Guest g(ctx, base);
    const int num = BotSlot(g, "Usage: <bot> botMovement(<forward>, <right>);", 2);
    if (num < 0) return;
    g_bots[num].forward = ClampMove(g.Int(0));
    g_bots[num].right = ClampMove(g.Int(1));
    g_bots[num].active = g_bots[num].has_move = true;
}

void PlayerCmd_BotMeleeParams(PPCContext& ctx, uint8_t* base)
{
    Guest g(ctx, base);
    const int num = BotSlot(g, "Usage: <bot> botMeleeParams(<yaw>, <dist>);", 2);
    if (num < 0) return;
    g_bots[num].melee_yaw = g.Float(0);
    const int dist = int(g.Float(1));
    g_bots[num].melee_dist = uint8_t(dist < 0 ? 0 : dist > 255 ? 255 : dist);
    g_bots[num].active = true;
}

void PlayerCmd_BotRemoteAngles(PPCContext& ctx, uint8_t* base)
{
    Guest g(ctx, base);
    const int num = BotSlot(g, "Usage: <bot> botRemoteAngles(<pitch>, <yaw>);", 2);
    if (num < 0) return;
    g_bots[num].remote_angles[0] = ClampMove(int(g.Float(0)));
    g_bots[num].remote_angles[1] = ClampMove(int(g.Float(1)));
    g_bots[num].active = g_bots[num].has_remote_angles = true;
}

void PlayerCmd_BotAngles(PPCContext& ctx, uint8_t* base)
{
    Guest g(ctx, base);
    const int num = BotSlot(g, "Usage: <bot> botAngles(<pitch>, <yaw>, <roll>);", 3);
    if (num < 0) return;
    for (int i = 0; i < 3; ++i) g_bots[num].angles[i] = g.Float(uint32_t(i));
    g_bots[num].active = g_bots[num].has_angles = true;
}

// addtestclient([name]): the stock test client, renamed when a name is given.
void GScr_AddTestClient(PPCContext& ctx, uint8_t* base)
{
    Guest g(ctx, base);
    const uint32_t params = g.NumParam();
    if (params > 1) return g.Error("Usage: addtestclient(<name>);");
    char name[32] = {};
    if (params == 1)
    {
        int j = 0;
        for (const char* s = g.String(0); *s && j < int(sizeof(name)) - 1; ++s)
        {
            if (uint8_t(*s) >= 0x20) name[j++] = *s;
        }
        if (!j) return g.Error("addtestclient: name must be at least 1 character long");
    }
    const uint32_t ent = g.Call32(SV_AddTestClient);
    if (!ent) return;
    if (name[0])
    {
        const uint32_t cl = Load32(base, kSvs + 0xC) + (ent - kGEntities) / kGEntitySize * kClientSize;
        g.Call(Info_SetValueForKey, cl + kClientUserinfo, g.Str("name"), g.Str(name));
        g.Call(SV_UserinfoChanged, cl);
    }
    g.Call(Scr_AddEntity, ent);
}

// --- Registration ---------------------------------------------------------------------------------------------------

struct Builtin
{
    const char* name;
    PPCFunc* fn;
};
constexpr Builtin kFunctions[] = {
    {"printconsole", GScr_PrintConsole}, {"filewrite", GScr_FileWrite}, {"fileread", GScr_FileRead},
    {"fileexists", GScr_FileExists},     {"openfile", GScr_OpenFile},   {"readstream", GScr_ReadStream},
    {"closefile", GScr_CloseFile},       {"addtestclient", GScr_AddTestClient},
};
constexpr Builtin kMethods[] = {
    {"botaction", PlayerCmd_BotAction},     {"botstop", PlayerCmd_BotStop},
    {"botmovement", PlayerCmd_BotMovement}, {"botmeleeparams", PlayerCmd_BotMeleeParams},
    {"botremoteangles", PlayerCmd_BotRemoteAngles}, {"botangles", PlayerCmd_BotAngles},
};
constexpr size_t kBuiltins = std::size(kFunctions) + std::size(kMethods);

// A guest address the script VM can call: a free slot of the function table, taken from the end of the code range.
// (A slot that is not a function's entry is never read, so it is empty.)
uint32_t AllocThunk(uint8_t* base, PPCFunc* fn)
{
    static uint32_t next = PPC_CODE_BASE + PPC_CODE_SIZE - 4;
    while (next > PPC_CODE_BASE && PPC_LOOKUP_FUNC(base, next) != nullptr) next -= 4;
    if (next <= PPC_CODE_BASE) return 0;
    PPC_LOOKUP_FUNC(base, next) = fn;
    const uint32_t address = next;
    next -= 4;
    return address;
}

// Guest addresses for the builtins, kFunctions then kMethods.
const uint32_t* Thunks(uint8_t* base)
{
    static const auto thunks = [base]
    {
        std::array<uint32_t, kBuiltins> t{};
        size_t i = 0;
        for (const auto& b : kFunctions) t[i++] = AllocThunk(base, b.fn);
        for (const auto& b : kMethods) t[i++] = AllocThunk(base, b.fn);
        Log("%zu script functions registered", kBuiltins);
        return t;
    }();
    return thunks.data();
}

// Scr_GetFunction/Scr_GetMethod: name null = register every builtin (the game's debugger table); else our builtin of
// that name, if any, into r3 (and its type, any, into *type).
template <size_t N>
bool AnswerBuiltin(PPCContext& ctx, uint8_t* base, const Builtin (&table)[N], const uint32_t* thunks)
{
    const uint32_t pname = ctx.r3.u32, ptype = ctx.r4.u32;
    if (!pname)
    {
        Guest g(ctx, base);
        for (size_t i = 0; i < N; ++i)
        {
            if (thunks[i]) g.Call(Scr_RegisterFunction, thunks[i], g.Str(table[i].name));
        }
        return false;
    }
    const char* name = GuestStr(base, Load32(base, pname));
    for (size_t i = 0; i < N; ++i)
    {
        if (thunks[i] && !strcasecmp(name, table[i].name))
        {
            if (ptype) Store<uint32_t>(base, ptype, 0);
            ctx.r3.u64 = thunks[i];
            return true;
        }
    }
    return false;
}

}  // namespace

// --- The hooks ------------------------------------------------------------------------------------------------------

// Scripts from the active mod first.
PPC_FUNC(sub_8229F2C8)
{
    char path[512];
    const char* script = GuestStr(base, ctx.r4.u32);
    if (FILE* f = ModPath(script, path) ? std::fopen(path, "rb") : nullptr)
    {
        std::fseek(f, 0, SEEK_END);
        const long size = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        Guest g(ctx, base);
        const uint32_t buffer = size > 0 ? g.Call32(Hunk_AllocateTempMemoryHighInternal, uint32_t(size) + 1) : 0;
        const bool ok = buffer && std::fread(base + buffer, 1, size_t(size), f) == size_t(size);
        std::fclose(f);
        if (ok)
        {
            base[buffer + uint32_t(size)] = 0;
            Log("%s from the mod", script);
            ctx.r3.u64 = buffer;
            return;
        }
        Log("%s: could not read %s", script, path);
    }
    __imp__sub_8229F2C8(ctx, base);
}

PPC_FUNC(sub_82254C38)
{
    if (!AnswerBuiltin(ctx, base, kFunctions, Thunks(base))) __imp__sub_82254C38(ctx, base);
}

PPC_FUNC(sub_82254D88)
{
    if (!AnswerBuiltin(ctx, base, kMethods, Thunks(base) + std::size(kFunctions))) __imp__sub_82254D88(ctx, base);
}

// A test client's input: what the bot methods set. Clients the scripts never drove keep the stock path.
PPC_FUNC(sub_822C3208)
{
    const uint32_t cl = ctx.r3.u32;
    const uint32_t clients = Load32(base, kSvs + 0xC);
    const int num = cl >= clients ? int((cl - clients) / kClientSize) : -1;
    if (!cl || !Load32(base, cl + kClientGentity) || num < 0 || num >= kMaxClients ||
        num >= int(Load32(base, kSvs + 8)))
    {
        return;
    }
    Guest g(ctx, base);
    const BotInput& b = g_bots[num];
    if (!b.active && !(g.Call32(__imp__sub_822C3B10, uint32_t(num)) & 0xFF))
    {
        return __imp__sub_822C3208(ctx, base);
    }
    const uint32_t ps = PlayerState(base, num);
    const uint32_t cmd = g.Alloc(0x28);  // usercmd_s
    Store<uint32_t>(base, cmd + 0x00, Load32(base, kSvs));
    Store<uint32_t>(base, cmd + 0x04, b.buttons);
    if (b.has_angles)
    {
        for (int i = 0; i < 3; ++i)
        {
            const float delta = b.angles[i] - LoadF(base, ps + kPsDeltaAngles + 4 * i);
            Store<int32_t>(base, cmd + 0x08 + 4 * i, int32_t(delta * (65536.0f / 360.0f)) & 65535);
        }
    }
    Store<uint16_t>(base, cmd + 0x14, b.weapon ? b.weapon : uint16_t(Load32(base, ps + kPsWeapon)));
    Store<uint16_t>(base, cmd + 0x16, uint16_t(Load32(base, ps + kPsPrimaryForAlt)));
    Store<uint16_t>(base, cmd + 0x18, uint16_t(Load32(base, ps + kPsOffHand)));
    if (b.has_move)
    {
        base[cmd + 0x1A] = uint8_t(b.forward);
        base[cmd + 0x1B] = uint8_t(b.right);
    }
    Store<float>(base, cmd + 0x1C, b.melee_yaw);
    base[cmd + 0x20] = b.melee_dist;
    if (b.has_remote_angles)
    {
        base[cmd + 0x24] = uint8_t(b.remote_angles[0]);
        base[cmd + 0x25] = uint8_t(b.remote_angles[1]);
    }
    Store<uint32_t>(base, cl + kClientDeltaMessage, Load32(base, cl + kClientOutgoingSequence) - 1);
    g.Call(SV_ClientThink, cl, cmd);
}

// Player_ActivateHoldCmd skips Use for bots; here its SV_IsClientBot says no.
PPC_FUNC(sub_822C3B10)
{
    if (ctx.lr == kSvBotUserMoveActivateCheck)
    {
        ctx.r3.u64 = 0;
        return;
    }
    __imp__sub_822C3B10(ctx, base);
}

// The weapon a script switches a client to: test clients ask for it in their input.
PPC_FUNC(sub_82261410)
{
    const int num = ctx.r3.s32;
    if (num >= 0 && num < kMaxClients) g_bots[num].weapon = uint16_t(ctx.r4.u32);
    __imp__sub_82261410(ctx, base);
}

// Script VM shutdown (map end): bot input and the open script file go with it.
PPC_FUNC(sub_822A36E8)
{
    std::memset(g_bots, 0, sizeof(g_bots));
    CloseStream();
    __imp__sub_822A36E8(ctx, base);
}

#endif  // MW2_TITLE_MP && !MW2_VERSION_TU0
