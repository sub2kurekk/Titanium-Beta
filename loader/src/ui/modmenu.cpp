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
using FnCCMenuItemSpriteCreate = void* (*)(
    void* normalSprite, void* selectedSprite, void* target,
    void(*selector)(void*, void*));
using FnCCLabelBMFontCreate   = void* (*)(const char* str, const char* fntFile);
using FnCCLayerColorCreate    = void* (*)(const ccColor4B& color, float w, float h);
using FnCCNodeAddChild        = void (*)(void* node, void* child);
using FnCCNodeAddChildZ       = void (*)(void* node, void* child, int z);
using FnCCNodeSetPosition     = void (*)(void* node, CCPoint pos);
using FnCCNodeSetScale        = void (*)(void* node, float scale);
using FnCCNodeSetZOrder       = void (*)(void* node, int z);
using FnCCNodeSetVisible      = void (*)(void* node, bool visible);
using FnCCNodeRemoveFromParent = void (*)(void* node);
using FnCCMenuSetPosition     = void (*)(void* menu, CCPoint pos);

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
            closeSpriteN, closeSpriteS, nullptr, onCloseButtonClicked);
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

static bool __fastcall menuLayerInitDetour(void* self);

// Offset of MenuLayer::init in GeometryDash.exe (GD 2.206, x64). This is the
// same offset Geode uses for its MenuLayer hook.
static constexpr uintptr_t k_MenuLayerInitOffset = 0x333a90;

using MenuLayerInitFn = bool(__thiscall*)(void* self);
static MenuLayerInitFn g_originalMenuLayerInit = nullptr;

static void onTitaniumButtonClicked(void*, void*) {
    logMsg("[TitaniumUI] Mod Library button clicked");
    showModLibraryPopup();
}

static bool __fastcall menuLayerInitDetour(void* self) {
    if (!g_originalMenuLayerInit)
        return false;  // hook not installed correctly; bail

    bool ok = g_originalMenuLayerInit(self);
    if (!ok) return false;

    if (!g_cocosSyms.load()) {
        logMsg("[TitaniumUI] Missing cocos2d symbols, skipping button injection");
        return true;
    }

    void* director = g_cocosSyms.sharedDirector();
    if (!director) return true;
    CCSize win = g_cocosSyms.getWinSize(director);

    // Mod Library button sprite (using standard GD button asset).
    void* normalSprite   = g_cocosSyms.spriteCreate("GJ_button_01.png");
    void* selectedSprite = g_cocosSyms.spriteCreate("GJ_button_01.png");
    if (!normalSprite || !selectedSprite) {
        logMsg("[TitaniumUI] Failed to load GJ_button_01.png sprite");
        return true;
    }
    g_cocosSyms.setScale(selectedSprite, 0.9f);

    void* menuItem = g_cocosSyms.menuItemCreate(
        normalSprite, selectedSprite, nullptr, onTitaniumButtonClicked);
    if (!menuItem) {
        logMsg("[TitaniumUI] menuItemCreate returned null");
        return true;
    }

    void* menu = g_cocosSyms.menuCreate();
    if (!menu) return true;
    g_cocosSyms.addChild(menu, menuItem);

    // Bottom-right of the screen, similar to where Geode places its button.
    g_cocosSyms.setPosition(menu, { win.width - 50.0f, 50.0f });
    g_cocosSyms.setPosition(menuItem, { 0.0f, 0.0f });
    g_cocosSyms.setScale(menu, 0.7f);
    g_cocosSyms.setZOrder(menu, 100);

    // MenuLayer is a CCNode, so we can addChild directly to self.
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

    auto target = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(gdExe) + k_MenuLayerInitOffset);

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
