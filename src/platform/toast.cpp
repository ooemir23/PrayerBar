#include "toast.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <windows.data.xml.dom.h>
#include <windows.ui.notifications.h>

namespace vakit {

namespace {

namespace xml = ABI::Windows::Data::Xml::Dom;
namespace notif = ABI::Windows::UI::Notifications;

struct HStr {
    HSTRING h = nullptr;
    explicit HStr(const std::wstring& s) { WindowsCreateString(s.c_str(), static_cast<UINT32>(s.size()), &h); }
    ~HStr() { if (h) WindowsDeleteString(h); }
};

template <typename T>
struct Com {
    T* p = nullptr;
    ~Com() { if (p) p->Release(); }
    T** Put() { return &p; }
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

} // namespace

bool ShowModernToast(const std::wstring& title, const std::wstring& body, bool silent) {
    // Empty content on system actions = Windows shows its own localized "Snooze" / "Dismiss".
    std::wstring payload =
        L"<toast><visual><binding template=\"ToastGeneric\"><text>" + XmlEscape(title) +
        L"</text><text>" + XmlEscape(body) + L"</text></binding></visual>"
        L"<actions><action activationType=\"system\" arguments=\"snooze\" content=\"\"/>"
        L"<action activationType=\"system\" arguments=\"dismiss\" content=\"\"/></actions>" +
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

    Com<notif::IToastNotificationManagerStatics> mgr;
    HStr mgrClass(L"Windows.UI.Notifications.ToastNotificationManager");
    if (FAILED(RoGetActivationFactory(mgrClass.h, __uuidof(notif::IToastNotificationManagerStatics),
                                      reinterpret_cast<void**>(mgr.Put())))) return false;
    Com<notif::IToastNotifier> notifier;
    // Without arguments this uses the package identity; fails (-> balloon fallback) when unpackaged.
    if (FAILED(mgr.p->CreateToastNotifier(notifier.Put()))) return false;
    return SUCCEEDED(notifier.p->Show(toast.p));
}

} // namespace vakit
