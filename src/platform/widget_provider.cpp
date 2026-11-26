#include "widget_provider.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <inspectable.h>
#include <roapi.h>
#include <winstring.h>

#include <set>

namespace vakit {

namespace {

// ABI of Microsoft.Windows.Widgets.Providers (Windows App SDK, Microsoft.Windows.Widgets.winmd).
// Hand-declared because this project builds without C++/WinRT; method order = vtable order.
constexpr IID IID_IWidgetProvider = {0x5C5774CC, 0x72A0, 0x452D, {0xB9, 0xED, 0x07, 0x5C, 0x0D, 0xD2, 0x5E, 0xED}};
constexpr IID IID_IWidgetManagerStatics = {0x7F233B06, 0x28E5, 0x5E2B, {0x8C, 0x04, 0xA4, 0xFA, 0x74, 0x7C, 0x28, 0xC7}};
constexpr IID IID_IWidgetUpdateRequestOptionsFactory = {0xE0E00AF8, 0x1D10, 0x57A8, {0x94, 0x19, 0x3F, 0x56, 0x8E, 0x85, 0x4D, 0xAA}};
// Must match <com:Class Id> and <CreateInstance ClassId> in packaging/msix/AppxManifest.template.xml.
constexpr CLSID CLSID_PrayerBarWidgetProvider = {0x34DBA178, 0x5C11, 0x46B6, {0x86, 0x9C, 0x2A, 0x61, 0xF6, 0x3F, 0x35, 0x3D}};

struct IWidgetContext : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Id(HSTRING* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_DefinitionId(HSTRING* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Size(INT32* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsActive(boolean* value) = 0;
};

struct IWidgetActionInvokedArgs : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_WidgetContext(IWidgetContext** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Verb(HSTRING* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Data(HSTRING* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_CustomState(HSTRING* value) = 0;
};

struct IWidgetUpdateRequestOptions : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_WidgetId(HSTRING* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Template(HSTRING* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Template(HSTRING value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Data(HSTRING* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Data(HSTRING value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_CustomState(HSTRING* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_CustomState(HSTRING value) = 0;
};

struct IWidgetUpdateRequestOptionsFactory : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE CreateInstance(HSTRING widgetId, IWidgetUpdateRequestOptions** value) = 0;
};

struct IWidgetManager : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE UpdateWidget(IWidgetUpdateRequestOptions* options) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetWidgetIds(UINT32* length, HSTRING** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetWidgetInfo(HSTRING widgetId, IInspectable** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetWidgetInfos(UINT32* length, IInspectable*** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE DeleteWidget(HSTRING widgetId) = 0;
};

struct IWidgetManagerStatics : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE GetDefault(IWidgetManager** value) = 0;
};

struct IWidgetProvider : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE CreateWidget(IWidgetContext* context) = 0;
    virtual HRESULT STDMETHODCALLTYPE DeleteWidget(HSTRING widgetId, HSTRING customState) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnActionInvoked(IWidgetActionInvokedArgs* args) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnWidgetContextChanged(IInspectable* args) = 0;
    virtual HRESULT STDMETHODCALLTYPE Activate(IWidgetContext* context) = 0;
    virtual HRESULT STDMETHODCALLTYPE Deactivate(HSTRING widgetId) = 0;
};

// Adaptive Card 1.5; ${...} fields come from FormatWidgetDataJson (core/formatter.cpp).
// One layout per board size via $host.widgetSize. The next prayer is marked with an emphasis band + bold,
// not accent-colored text (accent was hard to read on the dark board).
constexpr wchar_t kTemplate[] = LR"({
"type":"AdaptiveCard","$schema":"http://adaptivecards.io/schemas/adaptive-card.json","version":"1.5",
"body":[
 {"type":"Container","$when":"${$host.widgetSize==\"small\"}","height":"stretch","verticalContentAlignment":"center","items":[
  {"type":"TextBlock","text":"${heading}","horizontalAlignment":"center","isSubtle":true,"wrap":true},
  {"type":"TextBlock","text":"${remain}","size":"extraLarge","weight":"bolder","horizontalAlignment":"center","spacing":"none","wrap":true},
  {"type":"TextBlock","text":"${next} · ${time}","size":"medium","weight":"bolder","horizontalAlignment":"center","spacing":"small"}
 ]},
 {"type":"Container","$when":"${$host.widgetSize==\"medium\"}","items":[
  {"type":"ColumnSet","columns":[
   {"type":"Column","width":"stretch","items":[
    {"type":"TextBlock","text":"${heading}","isSubtle":true,"wrap":true},
    {"type":"TextBlock","text":"${remain}","size":"extraLarge","weight":"bolder","spacing":"none"}]},
   {"type":"Column","width":"auto","verticalContentAlignment":"bottom","items":[
    {"type":"TextBlock","text":"${next}","horizontalAlignment":"right","isSubtle":true},
    {"type":"TextBlock","text":"${time}","size":"large","weight":"bolder","horizontalAlignment":"right","spacing":"none"}]}]},
  {"type":"ColumnSet","spacing":"medium","columns":[
   {"type":"Column","$data":"${times}","$when":"${$index<3}","width":"stretch","style":"${if(cur,'emphasis','default')}","items":[
    {"type":"TextBlock","text":"${n}","horizontalAlignment":"center","isSubtle":"${!cur}"},
    {"type":"TextBlock","text":"${t}","size":"medium","weight":"bolder","horizontalAlignment":"center","spacing":"none"}]}]},
  {"type":"ColumnSet","spacing":"small","columns":[
   {"type":"Column","$data":"${times}","$when":"${$index>=3}","width":"stretch","style":"${if(cur,'emphasis','default')}","items":[
    {"type":"TextBlock","text":"${n}","horizontalAlignment":"center","isSubtle":"${!cur}"},
    {"type":"TextBlock","text":"${t}","size":"medium","weight":"bolder","horizontalAlignment":"center","spacing":"none"}]}]}
 ]},
 {"type":"Container","$when":"${$host.widgetSize==\"large\"}","items":[
  {"type":"TextBlock","text":"${city}","size":"medium","weight":"bolder"},
  {"type":"TextBlock","text":"${hijri}","isSubtle":true,"spacing":"none","wrap":true},
  {"type":"Container","style":"emphasis","spacing":"medium","items":[
   {"type":"TextBlock","text":"${heading}","isSubtle":true,"wrap":true},
   {"type":"TextBlock","text":"${remain}","size":"extraLarge","weight":"bolder","spacing":"none"},
   {"type":"TextBlock","text":"${next} · ${time}","size":"medium","spacing":"small"}]},
  {"type":"ColumnSet","$data":"${times}","spacing":"small","style":"${if(cur,'emphasis','default')}","columns":[
   {"type":"Column","width":"stretch","items":[
    {"type":"TextBlock","text":"${n}","size":"medium","weight":"${if(cur,'bolder','default')}","isSubtle":"${past}"}]},
   {"type":"Column","width":"auto","verticalContentAlignment":"center","items":[
    {"type":"TextBlock","text":"${d}","isSubtle":true,"horizontalAlignment":"right"}]},
   {"type":"Column","width":"auto","items":[
    {"type":"TextBlock","text":"${t}","size":"medium","weight":"bolder","horizontalAlignment":"right","isSubtle":"${past}"}]}]}
 ]}
],
"selectAction":{"type":"Action.Execute","verb":"open"}
})";

struct HStr {
    HSTRING h = nullptr;
    explicit HStr(const wchar_t* s) { WindowsCreateString(s, static_cast<UINT32>(wcslen(s)), &h); }
    explicit HStr(const std::wstring& s) { WindowsCreateString(s.c_str(), static_cast<UINT32>(s.size()), &h); }
    ~HStr() { if (h) WindowsDeleteString(h); }
};

std::wstring FromHString(HSTRING h) {
    UINT32 len = 0;
    const wchar_t* p = WindowsGetStringRawBuffer(h, &len);
    return std::wstring(p ? p : L"", len);
}

// UI-thread state; the provider is a process-lifetime singleton, so no refcounting is needed.
std::function<std::wstring()> g_dataSource;
std::function<void()> g_onOpen;
std::set<std::wstring> g_widgetIds;
std::wstring g_lastData;
DWORD g_cookie = 0;

IWidgetManager* GetManager() {
    HStr cls(L"Microsoft.Windows.Widgets.Providers.WidgetManager");
    IWidgetManagerStatics* statics = nullptr;
    if (FAILED(RoGetActivationFactory(cls.h, IID_IWidgetManagerStatics, reinterpret_cast<void**>(&statics)))) return nullptr;
    IWidgetManager* mgr = nullptr;
    statics->GetDefault(&mgr);
    statics->Release();
    return mgr;
}

void UpdateOne(IWidgetManager* mgr, const std::wstring& id, const std::wstring& data) {
    HStr cls(L"Microsoft.Windows.Widgets.Providers.WidgetUpdateRequestOptions");
    IWidgetUpdateRequestOptionsFactory* factory = nullptr;
    if (FAILED(RoGetActivationFactory(cls.h, IID_IWidgetUpdateRequestOptionsFactory, reinterpret_cast<void**>(&factory)))) return;
    HStr idH(id), tplH(kTemplate), dataH(data);
    IWidgetUpdateRequestOptions* opts = nullptr;
    if (SUCCEEDED(factory->CreateInstance(idH.h, &opts))) {
        opts->put_Template(tplH.h);
        opts->put_Data(dataH.h);
        mgr->UpdateWidget(opts);
        opts->Release();
    }
    factory->Release();
}

void PushTo(const std::wstring& id) {
    if (!g_dataSource) return;
    if (IWidgetManager* mgr = GetManager()) {
        UpdateOne(mgr, id, g_dataSource());
        mgr->Release();
    }
}

void TrackAndPush(IWidgetContext* ctx) {
    HSTRING h = nullptr;
    if (!ctx || FAILED(ctx->get_Id(&h))) return;
    std::wstring id = FromHString(h);
    WindowsDeleteString(h);
    g_widgetIds.insert(id);
    PushTo(id);
}

class Provider final : public IWidgetProvider {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, __uuidof(IInspectable)) || IsEqualIID(riid, IID_IWidgetProvider)) {
            *ppv = static_cast<IWidgetProvider*>(this);
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 2; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE GetIids(ULONG* count, IID** iids) override {
        *iids = static_cast<IID*>(CoTaskMemAlloc(sizeof(IID)));
        if (!*iids) return E_OUTOFMEMORY;
        **iids = IID_IWidgetProvider;
        *count = 1;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING* name) override {
        // Same as C++/WinRT: the first WinRT interface name. The metadata marshaler resolves types through it; empty stalls the board.
        static constexpr wchar_t kName[] = L"Microsoft.Windows.Widgets.Providers.IWidgetProvider";
        return WindowsCreateString(kName, static_cast<UINT32>(wcslen(kName)), name);
    }
    HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel* level) override { *level = BaseTrust; return S_OK; }

    HRESULT STDMETHODCALLTYPE CreateWidget(IWidgetContext* ctx) override { TrackAndPush(ctx); return S_OK; }
    HRESULT STDMETHODCALLTYPE Activate(IWidgetContext* ctx) override { TrackAndPush(ctx); return S_OK; }
    HRESULT STDMETHODCALLTYPE DeleteWidget(HSTRING id, HSTRING) override { g_widgetIds.erase(FromHString(id)); return S_OK; }
    HRESULT STDMETHODCALLTYPE Deactivate(HSTRING) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnWidgetContextChanged(IInspectable*) override { return S_OK; }  // template adapts via $host.widgetSize
    HRESULT STDMETHODCALLTYPE OnActionInvoked(IWidgetActionInvokedArgs* args) override {
        HSTRING verb = nullptr;
        if (args && SUCCEEDED(args->get_Verb(&verb))) {
            if (FromHString(verb) == L"open" && g_onOpen) g_onOpen();
            WindowsDeleteString(verb);
        }
        return S_OK;
    }
};

class Factory final : public IClassFactory {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppv = static_cast<IClassFactory*>(this);
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 2; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (outer) return CLASS_E_NOAGGREGATION;
        return provider_.QueryInterface(riid, ppv);
    }
    HRESULT STDMETHODCALLTYPE LockServer(BOOL) override { return S_OK; }

private:
    Provider provider_;
};

Factory g_factory;

} // namespace

bool RegisterWidgetProvider(std::function<std::wstring()> dataSource, std::function<void()> onOpen) {
    if (g_cookie) return true;
    g_dataSource = std::move(dataSource);
    g_onOpen = std::move(onOpen);
    HRESULT regHr = CoRegisterClassObject(CLSID_PrayerBarWidgetProvider, &g_factory, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &g_cookie);
    if (FAILED(regHr)) {
        g_cookie = 0;
        return false;
    }
    // Widgets pinned in an earlier session: the board won't re-send CreateWidget, so pick them up here.
    if (IWidgetManager* mgr = GetManager()) {
        UINT32 n = 0;
        HSTRING* ids = nullptr;
        if (SUCCEEDED(mgr->GetWidgetIds(&n, &ids)) && ids) {
            for (UINT32 i = 0; i < n; ++i) {
                g_widgetIds.insert(FromHString(ids[i]));
                WindowsDeleteString(ids[i]);
            }
            CoTaskMemFree(ids);
        }
        mgr->Release();
    }
    PushWidgetUpdate();
    return true;
}

void PushWidgetUpdate() {
    if (!g_cookie || g_widgetIds.empty() || !g_dataSource) return;
    std::wstring data = g_dataSource();
    if (data == g_lastData) return;
    IWidgetManager* mgr = GetManager();
    if (!mgr) return;
    for (const auto& id : g_widgetIds) UpdateOne(mgr, id, data);
    mgr->Release();
    g_lastData = std::move(data);
}

void UnregisterWidgetProvider() {
    if (g_cookie) CoRevokeClassObject(g_cookie);
    g_cookie = 0;
}

} // namespace vakit
