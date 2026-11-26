#include "toast.h"
#include "theme_utils.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <propsys.h>
#include <roapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <winstring.h>
#include <windows.data.xml.dom.h>
#include <windows.ui.notifications.h>

#include <atomic>
#include <mutex>
#include <utility>

namespace vakit {

namespace {

namespace xml = ABI::Windows::Data::Xml::Dom;
namespace notif = ABI::Windows::UI::Notifications;

constexpr wchar_t kDesktopAppUserModelId[] = L"PrayerBar.Desktop";
constexpr GUID kIidAgileObject = {0x94EA2B94u, 0xE9CCu, 0x49E0u, {0xC0u, 0xFFu, 0xEEu, 0x64u, 0xCAu, 0x8Fu, 0x5Bu, 0x90u}};
constexpr PROPERTYKEY kPkeyAppUserModelId = {
    {0x9F4C2855u, 0x9F79u, 0x4B39u, {0xA8u, 0xD0u, 0xE1u, 0xD4u, 0x2Du, 0xE1u, 0xD5u, 0xF3u}}, 5
};

struct HStr {
    HSTRING h = nullptr;
    explicit HStr(const std::wstring& s) { WindowsCreateString(s.c_str(), static_cast<UINT32>(s.size()), &h); }
    ~HStr() { if (h) WindowsDeleteString(h); }
};

template <typename T>
struct Com {
    T* p = nullptr;
    Com() = default;
    Com(const Com&) = delete;
    Com& operator=(const Com&) = delete;
    ~Com() { if (p) p->Release(); }
    T** Put() { return &p; }
    T* Detach() { T* r = p; p = nullptr; return r; }
};

std::wstring XmlEscape(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() + 16);
    for (wchar_t c : s) {
        switch (c) {
            case L'&': out += L"&amp;"; break;
            case L'<': out += L"&lt;"; break;
            case L'>': out += L"&gt;"; break;
            case L'"': out += L"&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

bool EnsureDesktopToastShortcut() {
    static bool initialized = false;
    static bool ok = false;
    if (initialized) return ok;
    initialized = true;

    SetCurrentProcessExplicitAppUserModelID(kDesktopAppUserModelId);

    wchar_t exePath[MAX_PATH]{};
    DWORD exeLen = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (exeLen == 0 || exeLen >= MAX_PATH) return false;

    PWSTR programsDir = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr, &programsDir)) || !programsDir) {
        return false;
    }
    std::wstring lnkPath = std::wstring(programsDir) + L"\\PrayerBar.lnk";
    CoTaskMemFree(programsDir);

    Com<IShellLinkW> link;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IShellLinkW, reinterpret_cast<void**>(link.Put())))) {
        return false;
    }
    link.p->SetPath(exePath);
    link.p->SetIconLocation(exePath, 0);

    Com<IPropertyStore> propStore;
    if (FAILED(link.p->QueryInterface(IID_IPropertyStore, reinterpret_cast<void**>(propStore.Put())))) {
        return false;
    }
    PROPVARIANT pv{};
    pv.vt = VT_LPWSTR;
    pv.pwszVal = const_cast<LPWSTR>(kDesktopAppUserModelId);
    if (FAILED(propStore.p->SetValue(kPkeyAppUserModelId, pv))) return false;
    if (FAILED(propStore.p->Commit())) return false;

    Com<IPersistFile> pf;
    if (FAILED(link.p->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(pf.Put())))) {
        return false;
    }
    ok = SUCCEEDED(pf.p->Save(lnkPath.c_str(), TRUE));
    return ok;
}

using ActivatedHandlerIface = __FITypedEventHandler_2_Windows__CUI__CNotifications__CToastNotification_IInspectable;
using DismissedHandlerIface = __FITypedEventHandler_2_Windows__CUI__CNotifications__CToastNotification_Windows__CUI__CNotifications__CToastDismissedEventArgs;

class ToastActivatedHandler final : public ActivatedHandlerIface {
public:
    explicit ToastActivatedHandler(std::function<void()> cb) : cb_(std::move(cb)) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == __uuidof(ActivatedHandlerIface) || riid == kIidAgileObject) {
            *ppv = static_cast<ActivatedHandlerIface*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(++refCount_); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = static_cast<ULONG>(--refCount_);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE Invoke(notif::IToastNotification*, IInspectable*) override {
        if (cb_) cb_();
        return S_OK;
    }

private:
    std::atomic<long> refCount_{1};
    std::function<void()> cb_;
};

class ToastDismissedHandler final : public DismissedHandlerIface {
public:
    explicit ToastDismissedHandler(std::function<void()> cb) : cb_(std::move(cb)) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == __uuidof(DismissedHandlerIface) || riid == kIidAgileObject) {
            *ppv = static_cast<DismissedHandlerIface*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(++refCount_); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = static_cast<ULONG>(--refCount_);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE Invoke(notif::IToastNotification*, notif::IToastDismissedEventArgs* args) override {
        if (args && cb_) {
            notif::ToastDismissalReason reason = notif::ToastDismissalReason_TimedOut;
            if (SUCCEEDED(args->get_Reason(&reason)) &&
                (reason == notif::ToastDismissalReason_UserCanceled ||
                 reason == notif::ToastDismissalReason_ApplicationHidden)) {
                cb_();
            }
        }
        return S_OK;
    }

private:
    std::atomic<long> refCount_{1};
    std::function<void()> cb_;
};

struct ActiveToastRef {
    notif::IToastNotification* toast = nullptr;
    EventRegistrationToken actTok{};
    EventRegistrationToken disTok{};
};

std::mutex g_toastMu;
ActiveToastRef g_activeToast;

void ReplaceActiveToast(notif::IToastNotification* newToast, EventRegistrationToken actTok, EventRegistrationToken disTok) {
    std::lock_guard<std::mutex> lk(g_toastMu);
    if (g_activeToast.toast) {
        g_activeToast.toast->remove_Activated(g_activeToast.actTok);
        g_activeToast.toast->remove_Dismissed(g_activeToast.disTok);
        g_activeToast.toast->Release();
        g_activeToast = {};
    }
    if (newToast) {
        newToast->AddRef();
        g_activeToast.toast = newToast;
        g_activeToast.actTok = actTok;
        g_activeToast.disTok = disTok;
    }
}

} // namespace

bool ShowModernToast(
    const std::wstring& title,
    const std::wstring& body,
    bool silent,
    const std::wstring& stopButtonLabel,
    std::function<void()> onStopAudio) {

    std::wstring actionsXml;
    if (!stopButtonLabel.empty()) {
        actionsXml =
            L"<actions>"
            L"<action content=\"" + XmlEscape(stopButtonLabel) + L"\" arguments=\"stop_audio\" activationType=\"foreground\"/>"
            L"<action activationType=\"system\" arguments=\"dismiss\" content=\"\"/>"
            L"</actions>";
    } else {
        actionsXml =
            L"<actions>"
            L"<action activationType=\"system\" arguments=\"snooze\" content=\"\"/>"
            L"<action activationType=\"system\" arguments=\"dismiss\" content=\"\"/>"
            L"</actions>";
    }

    std::wstring payload =
        L"<toast><visual><binding template=\"ToastGeneric\"><text>" + XmlEscape(title) +
        L"</text><text>" + XmlEscape(body) + L"</text></binding></visual>" +
        actionsXml +
        std::wstring(silent ? L"<audio silent=\"true\"/>" : L"") + L"</toast>";

    Com<IInspectable> docInsp;
    HStr docClass(L"Windows.Data.Xml.Dom.XmlDocument");
    if (FAILED(RoActivateInstance(docClass.h, docInsp.Put()))) return false;

    Com<xml::IXmlDocumentIO> docIo;
    if (FAILED(docInsp.p->QueryInterface(__uuidof(xml::IXmlDocumentIO), reinterpret_cast<void**>(docIo.Put())))) return false;
    HStr payloadH(payload);
    if (FAILED(docIo.p->LoadXml(payloadH.h))) return false;

    Com<xml::IXmlDocument> doc;
    if (FAILED(docInsp.p->QueryInterface(__uuidof(xml::IXmlDocument), reinterpret_cast<void**>(doc.Put())))) return false;

    Com<notif::IToastNotificationFactory> factory;
    HStr toastClass(L"Windows.UI.Notifications.ToastNotification");
    if (FAILED(RoGetActivationFactory(toastClass.h, __uuidof(notif::IToastNotificationFactory),
                                      reinterpret_cast<void**>(factory.Put())))) return false;
    Com<notif::IToastNotification> toast;
    if (FAILED(factory.p->CreateToastNotification(doc.p, toast.Put()))) return false;

    EventRegistrationToken actTok{};
    EventRegistrationToken disTok{};
    if (onStopAudio) {
        auto* actHandler = new ToastActivatedHandler(onStopAudio);
        auto* disHandler = new ToastDismissedHandler(onStopAudio);
        toast.p->add_Activated(actHandler, &actTok);
        toast.p->add_Dismissed(disHandler, &disTok);
        actHandler->Release();
        disHandler->Release();
    }

    Com<notif::IToastNotificationManagerStatics> mgr;
    HStr mgrClass(L"Windows.UI.Notifications.ToastNotificationManager");
    if (FAILED(RoGetActivationFactory(mgrClass.h, __uuidof(notif::IToastNotificationManagerStatics),
                                      reinterpret_cast<void**>(mgr.Put())))) return false;

    Com<notif::IToastNotifier> notifier;
    if (!IsPackagedApp() || FAILED(mgr.p->CreateToastNotifier(notifier.Put()))) {
        if (!EnsureDesktopToastShortcut()) return false;
        HStr appId(kDesktopAppUserModelId);
        if (FAILED(mgr.p->CreateToastNotifierWithId(appId.h, notifier.Put()))) return false;
    }

    if (FAILED(notifier.p->Show(toast.p))) return false;
    ReplaceActiveToast(onStopAudio ? toast.p : nullptr, actTok, disTok);
    return true;
}

} // namespace vakit
