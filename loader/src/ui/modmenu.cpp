// TitaniumLoader - Mod Library button + popup UI (Geode-style)
//
// Hooks MenuLayer::init and injects a "Mod Library" button at the bottom
// of the main menu. Clicking it opens a popup listing the loaded mods.

#ifdef _WIN32

#include <Titanium/HookEngine.hpp>
#include <Titanium/PatternScan.hpp>
#include <Titanium/ModLoader.hpp>
#include <Log.hpp>
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

namespace titanium {

// ─── Cocos2d forward declarations (ABI-only, not full layout) ───────────────

struct CCPoint { float x, y; };
struct CCSize  { float width, height; };
struct CCRect  { CCPoint origin; CCSize size; };
struct ccColor4B { uint8_t r, g, b, a; };

// Every cocos2d node starts with a single vtable pointer, so we can talk to
// them through `CocosNode*` regardless of the actual class — we just call
// the right mangled name.
struct CocosNode {
    void* vtable;
};

using FnCCDirectorShared      = void* (*)();
using FnCCDirectorGetWinSize  = CCSize (*)(void*);
using FnCCSpriteCreate        = void* (*)(const char*);
using FnCCMenuCreate          = void* (*)();
// CCMenuItemSprite::create(normal, selected, disabled, target, selector): five
// parameters (see the mangled name below). The selector is a member-function
// pointer, which on x64 is a plain code pointer called as selector(target, sender).
using FnCCMenuItemSpriteCreate = void* (*)(
    void* normalSprite, void* selectedSprite, void* disabledSprite,
    void* target, void(*selector)(void*, void*));
using FnCCLabelBMFontCreate   = void* (*)(const char* str, const char* fntFile);
using FnCCLayerColorCreate    = void* (*)(const ccColor4B& color, float w, float h);
using FnCCNodeAddChild        = void (*)(void* node, void* child);
using FnCCNodeAddChildZ       = void (*)(void* node, void* child, int z);
// The real parameter is `const CCPoint&`, i.e. a pointer, not the struct by value.
using FnCCNodeSetPosition     = void (*)(void* node, const CCPoint& pos);
using FnCCNodeSetScale        = void (*)(void* node, float scale);
using FnCCNodeSetZOrder       = void (*)(void* node, int z);
using FnCCNodeSetVisible      = void (*)(void* node, bool visible);
using FnCCNodeRemoveFromParent = void (*)(void* node);
using FnCCMenuSetPosition     = void (*)(void* menu, const CCPoint& pos);

// ─── Resolver helpers ────────────────────────────────────────────────────────

static HMODULE g_cocos = nullptr;

static HMODULE getCocos() {
    if (!g_cocos) g_cocos = GetModuleHandleA("libcocos2d.dll");
    return g_cocos;
}

template<typename T>
static T sym(const char* name) {
    HMODULE h = getCocos();
    if (!h) return nullptr;
    return reinterpret_cast<T>(GetProcAddress(h, name));
}

struct CocosSymbols {
    FnCCDirectorShared       sharedDirector     = nullptr;
    FnCCDirectorGetWinSize   getWinSize         = nullptr;
    FnCCSpriteCreate         spriteCreate       = nullptr;
    FnCCMenuCreate           menuCreate         = nullptr;
    FnCCMenuItemSpriteCreate menuItemCreate     = nullptr;
    FnCCLabelBMFontCreate    labelCreate        = nullptr;
    FnCCLayerColorCreate     layerColorCreate   = nullptr;
    FnCCNodeAddChild         addChild           = nullptr;
    FnCCNodeAddChildZ        addChildZ          = nullptr;
    FnCCNodeSetPosition      setPosition        = nullptr;
    FnCCNodeSetScale         setScale           = nullptr;
    FnCCNodeSetZOrder        setZOrder          = nullptr;
    FnCCNodeSetVisible       setVisible         = nullptr;
    FnCCNodeRemoveFromParent removeFromParent   = nullptr;
    FnCCMenuSetPosition      menuSetPosition    = nullptr;

    bool load() {
        sharedDirector     = sym<FnCCDirectorShared>(
            "?sharedDirector@CCDirector@cocos2d@@SAPEAV12@XZ");
        getWinSize         = sym<FnCCDirectorGetWinSize>(
            "?getWinSize@CCDirector@cocos2d@@QEAA?AVCCSize@2@XZ");
        spriteCreate       = sym<FnCCSpriteCreate>(
            "?create@CCSprite@cocos2d@@SAPEAV12@PEBD@Z");
        menuCreate         = sym<FnCCMenuCreate>(
            "?create@CCMenu@cocos2d@@SAPEAV12@XZ");
        menuItemCreate     = sym<FnCCMenuItemSpriteCreate>(
            "?create@CCMenuItemSprite@cocos2d@@SAPEAV12@PEAVCCNode@2@00PEAVCCObject@2@P842@EAAX1@Z@Z");
        labelCreate        = sym<FnCCLabelBMFontCreate>(
            "?create@CCLabelBMFont@cocos2d@@SAPEAV12@PEBD0@Z");
        layerColorCreate   = sym<FnCCLayerColorCreate>(
            "?create@CCLayerColor@cocos2d@@SAPEAV12@AEBU_ccColor4B@2@MM@Z");
        addChild           = sym<FnCCNodeAddChild>(
            "?addChild@CCNode@cocos2d@@UEAAXPEAV12@@Z");
        addChildZ          = sym<FnCCNodeAddChildZ>(
            "?addChild@CCNode@cocos2d@@UEAAXPEAV12@H@Z");
        setPosition        = sym<FnCCNodeSetPosition>(
            "?setPosition@CCNode@cocos2d@@UEAAXAEBVCCPoint@2@@Z");
        setScale           = sym<FnCCNodeSetScale>(
            "?setScale@CCNode@cocos2d@@UEAAXM@Z");
        setZOrder          = sym<FnCCNodeSetZOrder>(
            "?setZOrder@CCNode@cocos2d@@UEAAXH@Z");
        setVisible         = sym<FnCCNodeSetVisible>(
            "?setVisible@CCNode@cocos2d@@UEAAX_N@Z");
        removeFromParent   = sym<FnCCNodeRemoveFromParent>(
            "?removeFromParent@CCNode@cocos2d@@UEAAXXZ");
        menuSetPosition    = setPosition;  // CCMenu inherits CCNode, same impl
        return sharedDirector && getWinSize && spriteCreate && menuCreate
            && menuItemCreate && labelCreate && layerColorCreate
            && addChild && addChildZ && setPosition && setScale
            && setZOrder && setVisible && removeFromParent;
    }
};

static CocosSymbols g_cocosSyms;

// ─── Popup ──────────────────────────────────────────────────────────────────

static void* g_popupLayer = nullptr;

static void onCloseButtonClicked(void*, void*) {
    if (g_popupLayer && g_cocosSyms.removeFromParent) {
        g_cocosSyms.removeFromParent(g_popupLayer);
        g_popupLayer = nullptr;
        logMsg("[TitaniumUI] Mod Library popup closed");
    }
}

static void showModLibraryPopup() {
    if (g_popupLayer) {
        // Already open, just bring to front.
        if (g_cocosSyms.setZOrder) g_cocosSyms.setZOrder(g_popupLayer, 9999);
        return;
    }
    if (!g_cocosSyms.load()) {
        logMsg("[TitaniumUI] Cannot show popup: cocos2d symbols missing");
        return;
    }

    void* director = g_cocosSyms.sharedDirector();
    if (!director) {
        logMsg("[TitaniumUI] sharedDirector() returned null");
        return;
    }
    CCSize win = g_cocosSyms.getWinSize(director);

    // Dark semi-transparent panel covering ~60% of the screen, centered.
    constexpr float w = 0.6f, h = 0.7f;
    ccColor4B bg{0, 0, 0, 200};
    void* panel = g_cocosSyms.layerColorCreate(bg, win.width * w, win.height * h);
    if (!panel) {
        logMsg("[TitaniumUI] layerColorCreate returned null");
        return;
    }
    g_cocosSyms.setPosition(panel, { win.width * (1.0f - w) * 0.5f,
                                      win.height * (1.0f - h) * 0.5f });

    // Title at top.
    void* title = g_cocosSyms.labelCreate("TitaniumLoader - Mod Library", "bigFont.fnt");
    if (title) {
        g_cocosSyms.setPosition(title, { win.width * 0.5f, win.height - 50.0f });
        g_cocosSyms.setScale(title, 0.6f);
        g_cocosSyms.addChildZ(panel, title, 2);
    }

    // List loaded mods.
    const auto& mods = ModLoader::get().getMods();
    if (mods.empty()) {
        void* empty = g_cocosSyms.labelCreate("No mods loaded.", "bigFont.fnt");
        if (empty) {
            g_cocosSyms.setPosition(empty, { win.width * 0.5f, win.height * 0.5f });
            g_cocosSyms.setScale(empty, 0.45f);
            g_cocosSyms.addChildZ(panel, empty, 2);
        }
    } else {
        float y = win.height - 100.0f;
        const float x = win.width * 0.15f;
        for (size_t i = 0; i < mods.size() && i < 12; ++i) {
            const auto& m = mods[i];
            char buf[256];
            std::string info = std::string(m.enabled ? "[ON]  " : "[OFF] ") + m.id;
            if (m.instance) {
                auto mi = m.instance->getInfo();
                info += "  -  ";
                info.append(mi.name.data(), mi.name.size());
                info += " v";
                info.append(mi.version.data(), mi.version.size());
            }
            snprintf(buf, sizeof(buf), "%s", info.c_str());
            void* lbl = g_cocosSyms.labelCreate(buf, "bigFont.fnt");
            if (lbl) {
                g_cocosSyms.setPosition(lbl, { x, y });
                g_cocosSyms.setScale(lbl, 0.4f);
                g_cocosSyms.addChildZ(panel, lbl, 2);
                y -= 28.0f;
            }
        }
    }

    // Close button (top-right of panel) using GJ_button_01.png as a stand-in
    // until we add a proper X sprite. Scaling it down to a small square.
    void* closeSpriteN = g_cocosSyms.spriteCreate("GJ_closeBtn_001.png");
    void* closeSpriteS = g_cocosSyms.spriteCreate("GJ_closeBtn_001.png");
    void* closeMenuItem = nullptr;
    if (closeSpriteN && closeSpriteS && g_cocosSyms.setScale) {
        g_cocosSyms.setScale(closeSpriteS, 0.9f);
        closeMenuItem = g_cocosSyms.menuItemCreate(
            closeSpriteN, closeSpriteS, nullptr, nullptr, onCloseButtonClicked);
    }
    void* closeMenu = g_cocosSyms.menuCreate();
    if (closeMenu && closeMenuItem) {
        g_cocosSyms.addChild(closeMenu, closeMenuItem);
        // Top-right corner of the panel.
        g_cocosSyms.setPosition(closeMenu, { win.width * w - 25.0f, win.height * h - 25.0f });
        g_cocosSyms.setPosition(closeMenuItem, { 0.0f, 0.0f });
        g_cocosSyms.setScale(closeMenu, 0.7f);
        g_cocosSyms.addChildZ(panel, closeMenu, 3);
    }

    // Attach to the running scene at a very high z-order so it sits on top of
    // everything including Geode's UI if it happens to be loaded.
    void* scene = reinterpret_cast<void*(*)(void*)>(GetProcAddress(
        getCocos(), "?getRunningScene@CCDirector@cocos2d@@QEAAPEAVCCScene@2@XZ"))(director);
    if (scene) {
        g_cocosSyms.addChildZ(scene, panel, 9999);
        g_popupLayer = panel;
        logMsg("[TitaniumUI] Mod Library popup shown (%zu mod(s))", mods.size());
    } else {
        logMsg("[TitaniumUI] getRunningScene returned null, popup not shown");
    }
}

// ─── MenuLayer::init hook ──────────────────────────────────────────────────

static bool __fastcall menuLayerInitDetour(void* self, void* a2, void* a3, void* a4);

// Known GeometryDash.exe builds, identified by the PE header TimeDateStamp (the
// same way Geode identifies Windows builds), with the RVA of MenuLayer::init.
// To add a build: run the game once, read the "PE timestamp" line in the log,
// find MenuLayer::init in that build and add a row.
struct KnownBuild {
    uint32_t    timestamp;
    uintptr_t   menuLayerInitRva;
    const char* name;
    bool        verified;  // false = offset taken from third-party data, untested
};
static constexpr KnownBuild k_KnownBuilds[] = {
    { 0x6ACE3633, 0x46eee0, "GD 2.209",  true  },
    { 0x6AD2D987, 0x4720c0, "GD 2.2091", false },
};

static uint32_t readPeTimestamp(HMODULE mod) {
    auto* base = reinterpret_cast<const uint8_t*>(mod);
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    return nt->FileHeader.TimeDateStamp;
}

static uint32_t readImageSize(HMODULE mod) {
    auto* base = reinterpret_cast<const uint8_t*>(mod);
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    return nt->OptionalHeader.SizeOfImage;
}

// Sanity-checks a candidate function address so a wrong offset is refused
// instead of corrupting unrelated code. Returns true if it looks like
// MenuLayer::init. `strict` failures are logged; with TITANIUM_FORCE_HOOK=1 the
// hook is installed anyway.
static bool verifyMenuLayerInit(HMODULE mod, uintptr_t rva) {
    auto* base = reinterpret_cast<const uint8_t*>(mod);
    uint32_t imageSize = readImageSize(mod);
    if (rva < 0x1000 || rva + 0x400 > imageSize) {
        logMsg("[TitaniumUI] Verify: RVA 0x%zx is outside the image (size 0x%x)", (size_t)rva, imageSize);
        return false;
    }
    const uint8_t* fn = base + rva;

    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(fn, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
        !(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) {
        logMsg("[TitaniumUI] Verify: target is not in executable memory");
        return false;
    }
    if (rva & 0xF) {
        logMsg("[TitaniumUI] Verify: target is not 16-byte aligned");
        return false;
    }
    if (fn[-1] != 0xCC) {
        logMsg("[TitaniumUI] Verify: target is not preceded by padding (0xCC)");
        return false;
    }

    // MenuLayer::init loads many GJ_* sprites through RIP-relative `lea`
    // instructions. Count how many of the first 0x600 bytes' lea operands point
    // at strings starting with "GJ_".
    size_t scan = 0x600;
    if (fn + scan > base + imageSize) scan = (base + imageSize) - fn;
    int hits = 0;
    for (size_t i = 0; i + 7 <= scan; ++i) {
        // REX.W (48/4C) 8D /r with mod=00 rm=101 (RIP-relative)
        if ((fn[i] == 0x48 || fn[i] == 0x4C) && fn[i + 1] == 0x8D && (fn[i + 2] & 0xC7) == 0x05) {
            int32_t disp;
            memcpy(&disp, fn + i + 3, 4);
            const uint8_t* str = fn + i + 7 + disp;
            if (str > base && str + 4 < base + imageSize && str[0] == 'G' && str[1] == 'J' && str[2] == '_')
                ++hits;
        }
    }
    logMsg("[TitaniumUI] Verify: %d references to GJ_* sprite names near the target", hits);
    if (hits < 3) {
        logMsg("[TitaniumUI] Verify: too few sprite references; this does not look like MenuLayer::init");
        return false;
    }
    return true;
}

using MenuLayerInitFn = bool(__thiscall*)(void* self, void* a2, void* a3, void* a4);
static MenuLayerInitFn g_originalMenuLayerInit = nullptr;

static void onTitaniumButtonClicked(void*, void*) {
    logMsg("[TitaniumUI] Mod Library button clicked");
    showModLibraryPopup();
}

// Forwards all four argument registers untouched so the hook is transparent even
// if the real function takes more than `self`.
static bool __fastcall menuLayerInitDetour(void* self, void* a2, void* a3, void* a4) {
    // installHook() returns the trampoline only after the detour is live, so in
    // the tiny window between the two the pointer is still null. Wait briefly
    // instead of failing MenuLayer::init (which would leave the game stuck).
    for (int i = 0; i < 200 && !g_originalMenuLayerInit; ++i)
        Sleep(1);
    if (!g_originalMenuLayerInit) {
        logMsg("[TitaniumUI] detour called but the original function pointer is missing");
        return false;
    }

    logMsg("[TitaniumUI] MenuLayer::init detour entered, calling original");
    bool ok = g_originalMenuLayerInit(self, a2, a3, a4);
    logMsg("[TitaniumUI] original MenuLayer::init returned %d", ok ? 1 : 0);
    if (!ok) return false;

    char ptBuf[8];
    if (GetEnvironmentVariableA("TITANIUM_MENU_PASSTHROUGH", ptBuf, sizeof(ptBuf)) > 0 && ptBuf[0] == '1') {
        logMsg("[TitaniumUI] TITANIUM_MENU_PASSTHROUGH=1: not adding the Mods button");
        return true;
    }
    logMsg("[TitaniumUI] resolving cocos2d symbols");

    if (!g_cocosSyms.load()) {
        logMsg("[TitaniumUI] Missing cocos2d symbols, skipping button injection");
        return true;
    }

    logMsg("[TitaniumUI] cocos2d symbols resolved; getting director");
    void* director = g_cocosSyms.sharedDirector();
    if (!director) return true;
    CCSize win = g_cocosSyms.getWinSize(director);

    logMsg("[TitaniumUI] window size %.0fx%.0f; creating sprites", win.width, win.height);
    // Mod Library button sprite (using standard GD button asset).
    void* normalSprite   = g_cocosSyms.spriteCreate("GJ_button_01.png");
    void* selectedSprite = g_cocosSyms.spriteCreate("GJ_button_01.png");
    if (!normalSprite || !selectedSprite) {
        logMsg("[TitaniumUI] Failed to load GJ_button_01.png sprite");
        return true;
    }
    g_cocosSyms.setScale(selectedSprite, 0.9f);

    logMsg("[TitaniumUI] creating menu item");
    void* menuItem = g_cocosSyms.menuItemCreate(
        normalSprite, selectedSprite, nullptr, nullptr, onTitaniumButtonClicked);
    if (!menuItem) {
        logMsg("[TitaniumUI] menuItemCreate returned null");
        return true;
    }

    void* menu = g_cocosSyms.menuCreate();
    if (!menu) return true;
    g_cocosSyms.addChild(menu, menuItem);

    // Bottom-right of the screen, similar to where Geode places its button.
    logMsg("[TitaniumUI] positioning menu");
    g_cocosSyms.setPosition(menu, { win.width - 50.0f, 50.0f });
    g_cocosSyms.setPosition(menuItem, { 0.0f, 0.0f });
    g_cocosSyms.setScale(menu, 0.7f);
    g_cocosSyms.setZOrder(menu, 100);

    // MenuLayer is a CCNode, so we can addChild directly to self.
    logMsg("[TitaniumUI] adding menu to MenuLayer");
    g_cocosSyms.addChild(self, menu);

    // Label inside the button: "Mods".
    void* label = g_cocosSyms.labelCreate("Mods", "bigFont.fnt");
    if (label) {
        g_cocosSyms.setPosition(label, { 0.0f, 0.0f });
        g_cocosSyms.setScale(label, 0.5f);
        g_cocosSyms.addChild(menuItem, label);
    }

    logMsg("[TitaniumUI] Mod Library button injected at (%.0f, 50)", win.width - 50.0f);
    return true;
}

// ─── Installation ─────────────────────────────────────────────────────────

void installModMenuHook() {
    HMODULE gdExe = GetModuleHandleA(nullptr);
    if (!gdExe) gdExe = GetModuleHandleA("GeometryDash.exe");
    if (!gdExe) {
        logMsg("[TitaniumUI] Could not get GeometryDash.exe handle");
        return;
    }

    char offBuf[8];
    if (GetEnvironmentVariableA("TITANIUM_NO_MENU_HOOK", offBuf, sizeof(offBuf)) > 0 && offBuf[0] == '1') {
        logMsg("[TitaniumUI] TITANIUM_NO_MENU_HOOK=1 set, MenuLayer::init hook disabled");
        return;
    }

    uint32_t stamp = readPeTimestamp(gdExe);
    logMsg("[TitaniumUI] GeometryDash.exe PE timestamp: 0x%08X", stamp);

    uintptr_t rva = 0;
    const KnownBuild* build = nullptr;
    for (const auto& b : k_KnownBuilds)
        if (b.timestamp == stamp) build = &b;

    // Manual override for builds not in the table: TITANIUM_MENULAYER_INIT_RVA=0x...
    char envBuf[32];
    if (GetEnvironmentVariableA("TITANIUM_MENULAYER_INIT_RVA", envBuf, sizeof(envBuf)) > 0) {
        rva = static_cast<uintptr_t>(strtoull(envBuf, nullptr, 0));
        logMsg("[TitaniumUI] Using RVA 0x%zx from TITANIUM_MENULAYER_INIT_RVA", (size_t)rva);
    } else if (build) {
        rva = build->menuLayerInitRva;
        logMsg("[TitaniumUI] Recognized %s (%s offset), MenuLayer::init RVA 0x%zx",
               build->name, build->verified ? "verified" : "UNVERIFIED", (size_t)rva);
    } else {
        // Unknown build: fall back to the GD 2.206 offset, but only if the
        // verification below confirms it really is MenuLayer::init.
        rva = 0x333a90;
        logMsg("[TitaniumUI] Unknown Geometry Dash build; trying the GD 2.206 offset 0x%zx "
               "(verified before use). Send the timestamp line above to the Titanium "
               "developer to get your version supported, or set TITANIUM_MENULAYER_INIT_RVA.",
               (size_t)rva);
    }

    if (!verifyMenuLayerInit(gdExe, rva)) {
        char forceBuf[8];
        if (GetEnvironmentVariableA("TITANIUM_FORCE_HOOK", forceBuf, sizeof(forceBuf)) > 0 && forceBuf[0] == '1') {
            logMsg("[TitaniumUI] TITANIUM_FORCE_HOOK=1 set, hooking despite failed verification");
        } else {
            logMsg("[TitaniumUI] Verification failed, NOT hooking MenuLayer::init (mod button disabled)");
            return;
        }
    }

    auto target = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(gdExe) + rva);

    Hook hook;
    if (!HookEngine::get().installHook(target,
            reinterpret_cast<void*>(menuLayerInitDetour), &hook)) {
        logMsg("[TitaniumUI] Failed to install MenuLayer::init hook at %p", target);
        return;
    }
    g_originalMenuLayerInit = reinterpret_cast<MenuLayerInitFn>(hook.trampoline);
    logMsg("[TitaniumUI] MenuLayer::init hook installed at %p", target);
}

} // namespace titanium

#endif // _WIN32
