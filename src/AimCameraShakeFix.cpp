#include <windows.h>
#include <cstdint>

#include "MinHook.h"

namespace {

constexpr uintptr_t kImageBase            = 0x00400000;
constexpr uintptr_t kTheCamera            = 0x00B6F028;
constexpr uintptr_t kCameraProcess        = 0x0052B730;
constexpr uintptr_t kProcessAimWeapon     = 0x00521500;
constexpr uintptr_t kTimerTimeStep        = 0x00B7CB5C;
constexpr uintptr_t kTimerTimeStepNonClip = 0x00B7CB58;

constexpr uint32_t kCameraActiveCam  = 0x59;
constexpr uint32_t kCameraCams       = 0x174;
constexpr uint32_t kCameraWeaponMode = 0x830;
constexpr uint32_t kCamSize          = 0x238;
constexpr uint32_t kCamMode          = 0x0C;
constexpr uint32_t kQueuedModeMode   = 0x00;
constexpr uint32_t kPedMatrix        = 0x14;
// pedFlags bit 0 = bInVehicle; CPed::m_pVehicle can stay set after exit.
constexpr uint32_t kPedFlagsInVehicle = 0x46D;

constexpr float kMinAimTimeStep = 1.0f; // 50 FPS in GTA SA timestep units.

enum CamMode {
    MODE_SNIPER = 7,
    MODE_ROCKETLAUNCHER = 8,
    MODE_M16_1STPERSON = 34,
    MODE_SNIPER_RUNABOUT = 39,
    MODE_ROCKETLAUNCHER_RUNABOUT = 40,
    MODE_1STPERSON_RUNABOUT = 41,
    MODE_M16_1STPERSON_RUNABOUT = 42,
    MODE_FIGHT_CAM_RUNABOUT = 43,
    MODE_HELICANNON_1STPERSON = 45,
    MODE_CAMERA = 46,
    MODE_ROCKETLAUNCHER_HS = 51,
    MODE_ROCKETLAUNCHER_RUNABOUT_HS = 52,
    MODE_AIMWEAPON = 53,
    MODE_AIMWEAPON_ATTACHED = 65
};

struct Vec3 { float x, y, z; };

using CameraProcess_t = void(__thiscall*)(void*);
using AimWeapon_t = void(__thiscall*)(void*, const Vec3&, float, float, float);

CameraProcess_t g_origCameraProcess = nullptr;
AimWeapon_t g_origAimWeapon = nullptr;
bool g_installed = false;

int g_guardDepth = 0;
float g_savedTimeStep = 0.0f;
float g_savedTimeStepNonClip = 0.0f;
bool g_haveTimeStep = false;
bool g_haveTimeStepNonClip = false;

template <typename T>
bool SafeRead(uintptr_t addr, T& out) {
    __try {
        out = *reinterpret_cast<const T*>(addr);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template <typename T>
bool SafeWrite(uintptr_t addr, const T& val) {
    __try {
        *reinterpret_cast<T*>(addr) = val;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool LooksLikeGameCode(uintptr_t addr) {
    MEMORY_BASIC_INFORMATION mbi = {};
    if (!addr || VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi)) != sizeof(mbi))
        return false;
    const DWORD exec = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                       PAGE_EXECUTE_WRITECOPY;
    return mbi.State == MEM_COMMIT &&
           (mbi.Protect & exec) != 0 &&
           mbi.AllocationBase == GetModuleHandleA(nullptr);
}

uintptr_t PlayerPed() {
    uintptr_t ped = 0;
    SafeRead<uintptr_t>(0x00B6F5F0, ped);
    return ped;
}

bool IsPlayerOnFoot() {
    const uintptr_t ped = PlayerPed();
    if (!ped) return false;
    uint8_t flags = 0;
    if (!SafeRead<uint8_t>(ped + kPedFlagsInVehicle, flags))
        return false;
    return (flags & 1) == 0;
}

bool IsAimMode(int mode) {
    switch (mode) {
    case MODE_SNIPER:
    case MODE_ROCKETLAUNCHER:
    case MODE_M16_1STPERSON:
    case MODE_SNIPER_RUNABOUT:
    case MODE_ROCKETLAUNCHER_RUNABOUT:
    case MODE_1STPERSON_RUNABOUT:
    case MODE_M16_1STPERSON_RUNABOUT:
    case MODE_FIGHT_CAM_RUNABOUT:
    case MODE_HELICANNON_1STPERSON:
    case MODE_CAMERA:
    case MODE_ROCKETLAUNCHER_HS:
    case MODE_ROCKETLAUNCHER_RUNABOUT_HS:
    case MODE_AIMWEAPON:
    case MODE_AIMWEAPON_ATTACHED:
        return true;
    default:
        return false;
    }
}

bool IsAimCameraActive(void* cameraPtr) {
    const uintptr_t camera = reinterpret_cast<uintptr_t>(cameraPtr);
    if (camera != kTheCamera || !IsPlayerOnFoot())
        return false;

    int16_t weaponMode = 0;
    SafeRead<int16_t>(camera + kCameraWeaponMode + kQueuedModeMode, weaponMode);
    if (IsAimMode(weaponMode) || weaponMode != 0)
        return true;

    uint8_t active = 0;
    if (!SafeRead<uint8_t>(camera + kCameraActiveCam, active) || active > 2)
        return false;

    for (uint8_t i = 0; i < 3; ++i) {
        int16_t mode = 0;
        const uintptr_t cam = camera + kCameraCams + static_cast<uintptr_t>(i) * kCamSize;
        if (SafeRead<int16_t>(cam + kCamMode, mode) && IsAimMode(mode))
            return true;
    }

    return false;
}

void BeginGuard() {
    if (g_guardDepth++ != 0)
        return;

    g_haveTimeStep = SafeRead<float>(kTimerTimeStep, g_savedTimeStep);
    g_haveTimeStepNonClip = SafeRead<float>(kTimerTimeStepNonClip, g_savedTimeStepNonClip);

    if (g_haveTimeStep && g_savedTimeStep > 0.00001f && g_savedTimeStep < kMinAimTimeStep)
        SafeWrite<float>(kTimerTimeStep, kMinAimTimeStep);
    if (g_haveTimeStepNonClip && g_savedTimeStepNonClip > 0.00001f &&
        g_savedTimeStepNonClip < kMinAimTimeStep)
        SafeWrite<float>(kTimerTimeStepNonClip, kMinAimTimeStep);
}

void EndGuard() {
    if (g_guardDepth <= 0) {
        g_guardDepth = 0;
        return;
    }
    if (--g_guardDepth != 0)
        return;

    if (g_haveTimeStep)
        SafeWrite<float>(kTimerTimeStep, g_savedTimeStep);
    if (g_haveTimeStepNonClip)
        SafeWrite<float>(kTimerTimeStepNonClip, g_savedTimeStepNonClip);
}

void __fastcall HkCameraProcess(void* self, void*) {
    const bool guard = IsAimCameraActive(self);
    if (guard) BeginGuard();
    g_origCameraProcess(self);
    if (guard) EndGuard();
}

void __fastcall HkProcessAimWeapon(void* self, void*, const Vec3* target,
                                   float orientation, float speedVar, float speedVarWanted) {
    BeginGuard();
    if (target) {
        g_origAimWeapon(self, *target, orientation, speedVar, speedVarWanted);
    } else {
        Vec3 zero = {};
        g_origAimWeapon(self, zero, orientation, speedVar, speedVarWanted);
    }
    EndGuard();
}

DWORD WINAPI InitThread(void*) {
    if (reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr)) != kImageBase)
        return 0;
    if (!LooksLikeGameCode(kCameraProcess) || !LooksLikeGameCode(kProcessAimWeapon))
        return 0;

    if (MH_Initialize() != MH_OK)
        return 0;
    if (MH_CreateHook(reinterpret_cast<void*>(kCameraProcess), &HkCameraProcess,
                      reinterpret_cast<void**>(&g_origCameraProcess)) != MH_OK ||
        MH_CreateHook(reinterpret_cast<void*>(kProcessAimWeapon), &HkProcessAimWeapon,
                      reinterpret_cast<void**>(&g_origAimWeapon)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(kCameraProcess)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(kProcessAimWeapon)) != MH_OK)
        return 0;

    g_installed = true;
    return 0;
}

void Shutdown() {
    if (!g_installed)
        return;
    MH_DisableHook(reinterpret_cast<void*>(kCameraProcess));
    MH_DisableHook(reinterpret_cast<void*>(kProcessAimWeapon));
    MH_RemoveHook(reinterpret_cast<void*>(kCameraProcess));
    MH_RemoveHook(reinterpret_cast<void*>(kProcessAimWeapon));
    g_installed = false;
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInst);
        if (HANDLE thread = CreateThread(nullptr, 0, InitThread, hInst, 0, nullptr))
            CloseHandle(thread);
    } else if (reason == DLL_PROCESS_DETACH) {
        Shutdown();
    }
    return TRUE;
}
