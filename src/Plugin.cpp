#include <windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include "Badge.h"

namespace game
{
// Fixed ABI for the MO 3.3.6 executable accepted in SyringeHandshake.
constexpr std::uintptr_t Map = 0x87F7E8, View = 0x886FA0;
constexpr std::uintptr_t Tactical = 0x887324, Composite = 0x88731C;
constexpr std::uintptr_t OverlayTypes = 0xA83D84, OverlayCount = 0xA83D90;
constexpr int MapSide = 512;
struct Point { int x, y; };
struct Coord { int x, y, z; };
struct Cell { short x, y; };
struct Viewport { int x, y, w, h; };
struct Rect { int lowX = MapSide, lowY = MapSide, highX = -1, highY = -1; };

template<class T> T at(std::uintptr_t address)
{
    return *reinterpret_cast<const T*>(address);
}

template<class T> T field(const void* object, std::size_t offset)
{
    return at<T>(reinterpret_cast<std::uintptr_t>(object) + offset);
}

template<class Fn> Fn method(const void* object, std::size_t slot)
{
    const auto table = field<std::uintptr_t>(object, 0);
    return table ? at<Fn>(table + slot) : nullptr;
}

struct MapCells
{
    const void* const* list = at<const void* const*>(Map + 0x13C);
    int count = at<int>(Map + 0x140);

    const void* get(int x, int y) const
    {
        const unsigned index = static_cast<unsigned>(y) * MapSide + x;
        return index < static_cast<unsigned>(count) ? list[index] : nullptr;
    }
};

bool visibleCells(const Viewport& view, Rect& area)
{
    using Pick = bool (__thiscall *)(void*, Point*, Cell*, Coord*, void**, BYTE*, BYTE*);
    const auto pick = reinterpret_cast<Pick>(0x692300);
    // Sampling the middle and edges also covers views whose corners fall off the map.
    for (int dy : {0, view.h / 2, view.h - 1})
        for (int dx : {0, view.w / 2, view.w - 1})
        {
            Point p{view.x + dx, view.y + dy};
            Cell square{};
            Coord world{};
            void* object = nullptr;
            BYTE a = 0, b = 0;
            if (!pick(reinterpret_cast<void*>(Map), &p, &square, &world, &object, &a, &b))
                continue;
            if (square.x < 0 || square.x >= MapSide || square.y < 0 || square.y >= MapSide) continue;
            area.lowX = std::min(area.lowX, static_cast<int>(square.x));
            area.lowY = std::min(area.lowY, static_cast<int>(square.y));
            area.highX = std::max(area.highX, static_cast<int>(square.x));
            area.highY = std::max(area.highY, static_cast<int>(square.y));
        }
    if (area.lowX > area.highX) return false;
    area.lowX = std::max(0, area.lowX - 3);
    area.lowY = std::max(0, area.lowY - 3);
    area.highX = std::min(MapSide - 1, area.highX + 3);
    area.highY = std::min(MapSide - 1, area.highY + 3);
    return true;
}

Point project(void* tactical, Cell cell, const void* raw)
{
    using Height = int (__thiscall *)(void*, const Coord*);
    using Project = bool (__thiscall *)(void*, const Coord*, Point*);
    Coord world{cell.x * 256 + 128, cell.y * 256 + 128, 0};
    const int floor = reinterpret_cast<Height>(0x578080)(reinterpret_cast<void*>(Map), &world);
    if (floor >= -8192 && floor <= 8192) world.z = floor;
    if (field<unsigned>(raw, 0x140) & 0x500) world.z += 416;
    Point pixel{};
    reinterpret_cast<Project>(0x6D2140)(tactical, &world, &pixel);
    return pixel;
}
}

namespace
{
struct OptionPaths
{
    char shroud[MAX_PATH]{};
    char ini[MAX_PATH]{};

    OptionPaths()
    {
        char file[MAX_PATH]{};
        const DWORD n = GetModuleFileNameA(nullptr, file, MAX_PATH);
        if (!n || n >= MAX_PATH) return;
        char* divider = std::strrchr(file, '\\');
        if (!divider || (divider - file) + sizeof("\\SA_CrateIcon.ini") > MAX_PATH) return;
        divider[1] = 0;
        lstrcpyA(shroud, file);
        lstrcatA(shroud, "shroud.shp");
        lstrcpyA(ini, file);
        lstrcatA(ini, "SA_CrateIcon.ini");
    }

    bool translucent() const
    {
        const auto attr = GetFileAttributesA(shroud);
        return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
    }
};

struct CrateScan
{
    std::array<game::Cell, 256> results{};
    std::array<bool, 512> crateOverlays{};
    game::Rect lastArea{};
    const void* const* lastCells = nullptr;
    DWORD lastTick = 0;
    unsigned used = 0;

    int type(const void* cell) const
    {
        if (!cell) return -1;
        const unsigned overlay = game::field<unsigned>(cell, 0x44);
        const unsigned value = game::field<BYTE>(cell, 0x11E);
        return overlay < crateOverlays.size() && crateOverlays[overlay] &&
               value < std::size(badge::labels) ? static_cast<int>(value) : -1;
    }

    void update(const game::MapCells& map, const game::Rect& area)
    {
        const auto now = GetTickCount();
        if (lastCells == map.list && now - lastTick < 200 &&
            area.lowX == lastArea.lowX && area.lowY == lastArea.lowY &&
            area.highX == lastArea.highX && area.highY == lastArea.highY)
            return;
        lastTick = now;
        lastCells = map.list;
        lastArea = area;
        used = 0;
        crateOverlays.fill(false);
        const auto* types = game::at<const void* const*>(game::OverlayTypes);
        const unsigned count = game::at<unsigned>(game::OverlayCount);
        if (!types || count > crateOverlays.size() ||
            (area.highX - area.lowX + 1) * (area.highY - area.lowY + 1) > 20000)
            return;
        for (unsigned i = 0; i < count; ++i)
            crateOverlays[i] = types[i] && game::field<BYTE>(types[i], 0x2AA);
        for (int y = area.lowY; y <= area.highY; ++y)
            for (int x = area.lowX; x <= area.highX; ++x)
                if (type(map.get(x, y)) >= 0)
                {
                    results[used++] = {static_cast<short>(x), static_cast<short>(y)};
                    if (used == results.size()) return;
                }
    }
};

void DrawCrates()
{
    using Metric = int (__thiscall *)(void*);
    using Lock = unsigned char* (__thiscall *)(void*, int, int);
    using Unlock = void (__thiscall *)(void*);
    const auto* started = reinterpret_cast<volatile const BYTE*>(0xA8ED5C);
    if (!*started) return;
    static const OptionPaths paths;
    static const bool enabled = paths.ini[0] == 0 ||
        GetPrivateProfileIntA("CrateIcon", "Enable", 1, paths.ini) != 0;
    static const bool translucent = paths.translucent();
    if (!enabled) return;

    void* surface = game::at<void*>(game::Composite);
    void* tactical = game::at<void*>(game::Tactical);
    if (!surface || !tactical) return;
    const auto bpp = game::method<Metric>(surface, 0x70);
    const auto pitch = game::method<Metric>(surface, 0x74);
    const auto width = game::method<Metric>(surface, 0x7C);
    const auto height = game::method<Metric>(surface, 0x80);
    const auto lock = game::method<Lock>(surface, 0x5C);
    const auto unlock = game::method<Unlock>(surface, 0x60);
    if (!bpp || !pitch || !width || !height || !lock || !unlock || bpp(surface) != 2) return;
    const auto view = game::at<game::Viewport>(game::View);
    const int surfaceWidth = width(surface), stride = pitch(surface);
    const int w = std::min(surfaceWidth, view.w), h = std::min(height(surface), view.h);
    if (w < 32 || h < 32 || surfaceWidth > 16384 || stride < surfaceWidth * 2) return;

    const game::MapCells map;
    if (!map.list || map.count <= 0 || map.count > game::MapSide * game::MapSide) return;
    game::Rect area;
    if (!game::visibleCells(view, area)) return;
    static CrateScan scan;
    scan.update(map, area);
    if (scan.used == 0) return;

    unsigned char* pixels = lock(surface, 0, 0);
    if (!pixels) return;
    for (unsigned i = 0; i < scan.used; ++i)
    {
        const auto location = scan.results[i];
        const auto* native = map.get(location.x, location.y);
        const int type = scan.type(native);
        if (type < 0) continue;
        const bool shadow = !(game::field<unsigned>(native, 0x12C) & 8);
        if (shadow && !translucent) continue;
        const auto pixel = game::project(tactical, location, native);
        badge::draw({pixels, w, h, stride}, pixel.x - view.x, pixel.y - view.y - 20,
                    static_cast<unsigned>(type), shadow);
    }
    unlock(surface);
}

struct HookRecord { unsigned address, length; const char* name; unsigned reserved; };
struct LoaderInfo
{
    int size, hookCount;
    unsigned checksum, exeSize, exeTimestamp, exeCRC;
    int messageCapacity;
    char* message;
};
static_assert(sizeof(HookRecord) == 16 && sizeof(LoaderInfo) == 32);
HookRecord hooks[] __attribute__((section(".syhks00"), used, aligned(16))) =
{
    {0x005D4A9B, 5, "Crate_DrawHook", 0},
};
}

extern "C" __declspec(dllexport) DWORD __cdecl Crate_DrawHook(void*)
{
    DrawCrates();
    return 0;
}

extern "C" __declspec(dllexport) HRESULT __cdecl SyringeHandshake(LoaderInfo* info)
{
    if (!info || info->size < static_cast<int>(sizeof(*info))) return E_FAIL;
    const bool supported = info->exeSize == 0x00497FE0 && info->exeTimestamp == 0x3BDF544E &&
                           info->exeCRC == 0x1B499086;
    if (info->message && info->messageCapacity > 0)
        lstrcpynA(info->message, supported ? "SA_CrateIcon (MO 3.3.6)"
                                         : "SA_CrateIcon: unsupported executable", info->messageCapacity);
    return supported ? S_OK : E_FAIL;
}
