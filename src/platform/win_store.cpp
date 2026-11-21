#include "win_store.h"
#include "../core/commercial_config.h"
#include "../core/utf_util.h"

#include <unknwn.h>

#include <atomic>
#include <cstdio>
#include <cstring>

namespace vakit {

namespace {

using WinRtHString = void*;

constexpr LONG kAppModelErrorNoPackage = 15700L;

const IID kIidUnknown = {0x00000000u, 0x0000u, 0x0000u, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const IID kIidAgileObject = {0x94EA2B94u, 0xE9CCu, 0x49E0u, {0xC0, 0xFF, 0xEE, 0x64, 0xCA, 0x8F, 0x5B, 0x90}};
const IID kIidIAsyncInfo = {0x00000036u, 0x0000u, 0x0000u, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const IID kIidInitializeWithWindow = {0x3E68D4BDu, 0x7135u, 0x4D10u, {0x80, 0x18, 0x9F, 0xB6, 0xD9, 0xF3, 0x3F, 0xA1}};

const IID kIidStoreContextStatics = {0x9C06EE5Fu, 0x15C0u, 0x4E72u, {0x93, 0x30, 0xD6, 0x19, 0x1C, 0xEB, 0xD1, 0x9C}};
const IID kIidCompletedPurchaseHandler = {0x1D9F89EEu, 0x2FCEu, 0x54E6u, {0xA0, 0xA9, 0x52, 0xD0, 0x0C, 0x52, 0xCC, 0x3A}};
const IID kIidIterableKvpLicense = {0xCA8BA445u, 0x6F4Du, 0x5DA9u, {0x95, 0xEE, 0x42, 0xCF, 0x11, 0x8D, 0xEF, 0x63}};

struct ComBaseApi {
    HMODULE mod = nullptr;
    HRESULT (WINAPI* roInitialize)(int initType) = nullptr;
    void (WINAPI* roUninitialize)() = nullptr;
    HRESULT (WINAPI* roGetActivationFactory)(WinRtHString classId, REFIID iid, void** factory) = nullptr;
    HRESULT (WINAPI* createString)(const wchar_t* source, UINT32 length, WinRtHString* out) = nullptr;
    HRESULT (WINAPI* deleteString)(WinRtHString str) = nullptr;
    const wchar_t* (WINAPI* getRawBuffer)(WinRtHString str, UINT32* length) = nullptr;

    bool Load() {
        mod = GetModuleHandleW(L"combase.dll");
        if (!mod) mod = LoadLibraryW(L"combase.dll");
        if (!mod) return false;
        roInitialize = reinterpret_cast<HRESULT (WINAPI*)(int)>(GetProcAddress(mod, "RoInitialize"));
        roUninitialize = reinterpret_cast<void (WINAPI*)()>(GetProcAddress(mod, "RoUninitialize"));
        roGetActivationFactory = reinterpret_cast<HRESULT (WINAPI*)(WinRtHString, REFIID, void**)>(GetProcAddress(mod, "RoGetActivationFactory"));
        createString = reinterpret_cast<HRESULT (WINAPI*)(const wchar_t*, UINT32, WinRtHString*)>(GetProcAddress(mod, "WindowsCreateString"));
        deleteString = reinterpret_cast<HRESULT (WINAPI*)(WinRtHString)>(GetProcAddress(mod, "WindowsDeleteString"));
        getRawBuffer = reinterpret_cast<const wchar_t* (WINAPI*)(WinRtHString, UINT32*)>(GetProcAddress(mod, "WindowsGetStringRawBuffer"));
        return roInitialize && roUninitialize && roGetActivationFactory && createString && deleteString && getRawBuffer;
    }

    std::wstring ConsumeHString(WinRtHString hs) const {
        if (!hs) return {};
        UINT32 len = 0;
        const wchar_t* buf = getRawBuffer(hs, &len);
        std::wstring out = (buf && len > 0) ? std::wstring(buf, len) : std::wstring();
        deleteString(hs);
        return out;
    }
};

struct RawComUnknown {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
    };
    const Vtbl* lpVtbl;
};

struct AbiInitWithWindow {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
        HRESULT (STDMETHODCALLTYPE* Initialize)(void* self, HWND hwnd);
    };
    const Vtbl* lpVtbl;
};

struct AbiAsyncInfo {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
        void* getIids;
        void* getRuntimeClassName;
        void* getTrustLevel;
        HRESULT (STDMETHODCALLTYPE* get_Id)(void* self, uint32_t* id);
        HRESULT (STDMETHODCALLTYPE* get_Status)(void* self, int32_t* status);
        HRESULT (STDMETHODCALLTYPE* get_ErrorCode)(void* self, HRESULT* errorCode);
        HRESULT (STDMETHODCALLTYPE* Cancel)(void* self);
        HRESULT (STDMETHODCALLTYPE* Close)(void* self);
    };
    const Vtbl* lpVtbl;
};

struct AbiAsyncOperation {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
        void* getIids;
        void* getRuntimeClassName;
        void* getTrustLevel;
        HRESULT (STDMETHODCALLTYPE* put_Completed)(void* self, void* handler);
        HRESULT (STDMETHODCALLTYPE* get_Completed)(void* self, void** handler);
        HRESULT (STDMETHODCALLTYPE* GetResults)(void* self, void** results);
    };
    const Vtbl* lpVtbl;
};

struct AbiStoreContextStatics {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
        void* getIids;
        void* getRuntimeClassName;
        void* getTrustLevel;
        HRESULT (STDMETHODCALLTYPE* GetDefault)(void* self, void** storeContext);
        HRESULT (STDMETHODCALLTYPE* GetForUser)(void* self, void* user, void** storeContext);
    };
    const Vtbl* lpVtbl;
};

struct AbiStoreContext {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
        void* getIids;
        void* getRuntimeClassName;
        void* getTrustLevel;
        void* get_User;
        void* add_OfflineLicensesChanged;
        void* remove_OfflineLicensesChanged;
        void* GetCustomerPurchaseIdAsync;
        void* GetCustomerCollectionsIdAsync;
        HRESULT (STDMETHODCALLTYPE* GetAppLicenseAsync)(void* self, AbiAsyncOperation** op);
        void* GetStoreProductForCurrentAppAsync;
        void* GetStoreProductsAsync;
        void* GetAssociatedStoreProductsAsync;
        void* GetAssociatedStoreProductsWithPagingAsync;
        void* GetUserCollectionAsync;
        void* GetUserCollectionWithPagingAsync;
        void* ReportConsumableFulfillmentAsync;
        void* GetConsumableBalanceRemainingAsync;
        void* AcquireStoreLicenseForOptionalPackageAsync;
        HRESULT (STDMETHODCALLTYPE* RequestPurchaseAsync)(void* self, WinRtHString storeId, AbiAsyncOperation** op);
    };
    const Vtbl* lpVtbl;
};

struct AbiStoreAppLicense {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
        void* getIids;
        void* getRuntimeClassName;
        void* getTrustLevel;
        HRESULT (STDMETHODCALLTYPE* get_SkuStoreId)(void* self, WinRtHString* val);
        HRESULT (STDMETHODCALLTYPE* get_IsActive)(void* self, unsigned char* val);
        HRESULT (STDMETHODCALLTYPE* get_IsTrial)(void* self, unsigned char* val);
        HRESULT (STDMETHODCALLTYPE* get_ExpirationDate)(void* self, int64_t* val);
        HRESULT (STDMETHODCALLTYPE* get_ExtendedJsonData)(void* self, WinRtHString* val);
        HRESULT (STDMETHODCALLTYPE* get_AddOnLicenses)(void* self, void** val);
    };
    const Vtbl* lpVtbl;
};

struct AbiStoreLicense {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
        void* getIids;
        void* getRuntimeClassName;
        void* getTrustLevel;
        HRESULT (STDMETHODCALLTYPE* get_SkuStoreId)(void* self, WinRtHString* val);
        HRESULT (STDMETHODCALLTYPE* get_IsActive)(void* self, unsigned char* val);
        HRESULT (STDMETHODCALLTYPE* get_ExpirationDate)(void* self, int64_t* val);
        HRESULT (STDMETHODCALLTYPE* get_ExtendedJsonData)(void* self, WinRtHString* val);
        HRESULT (STDMETHODCALLTYPE* get_InAppOfferToken)(void* self, WinRtHString* val);
    };
    const Vtbl* lpVtbl;
};

struct AbiStorePurchaseResult {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
        void* getIids;
        void* getRuntimeClassName;
        void* getTrustLevel;
        HRESULT (STDMETHODCALLTYPE* get_Status)(void* self, int32_t* val);
        HRESULT (STDMETHODCALLTYPE* get_ExtendedError)(void* self, HRESULT* val);
    };
    const Vtbl* lpVtbl;
};

struct AbiIterableKvp {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
        void* getIids;
        void* getRuntimeClassName;
        void* getTrustLevel;
        HRESULT (STDMETHODCALLTYPE* First)(void* self, void** iterator);
    };
    const Vtbl* lpVtbl;
};

struct AbiIteratorKvp {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
        void* getIids;
        void* getRuntimeClassName;
        void* getTrustLevel;
        HRESULT (STDMETHODCALLTYPE* get_Current)(void* self, void** kvp);
        HRESULT (STDMETHODCALLTYPE* get_HasCurrent)(void* self, unsigned char* val);
        HRESULT (STDMETHODCALLTYPE* MoveNext)(void* self, unsigned char* val);
    };
    const Vtbl* lpVtbl;
};

struct AbiKvpLicense {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
        void* getIids;
        void* getRuntimeClassName;
        void* getTrustLevel;
        HRESULT (STDMETHODCALLTYPE* get_Key)(void* self, WinRtHString* key);
        HRESULT (STDMETHODCALLTYPE* get_Value)(void* self, AbiStoreLicense** val);
    };
    const Vtbl* lpVtbl;
};

template <typename T>
void SafeComRelease(T*& ptr) {
    if (ptr) {
        reinterpret_cast<RawComUnknown*>(ptr)->lpVtbl->Release(ptr);
        ptr = nullptr;
    }
}

bool IsRunningPackaged() {
    using GetPkgFn = LONG(WINAPI*)(UINT32*, PWSTR);
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    if (!k32) return false;
    auto fn = reinterpret_cast<GetPkgFn>(GetProcAddress(k32, "GetCurrentPackageFullName"));
    if (!fn) return false;
    UINT32 len = 0;
    return fn(&len, nullptr) != kAppModelErrorNoPackage;
}

bool EqualsIgnoreCase(std::wstring_view a, std::wstring_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        wchar_t ca = a[i];
        wchar_t cb = b[i];
        if (ca >= L'A' && ca <= L'Z') ca = static_cast<wchar_t>(ca - L'A' + L'a');
        if (cb >= L'A' && cb <= L'Z') cb = static_cast<wchar_t>(cb - L'A' + L'a');
        if (ca != cb) return false;
    }
    return true;
}

bool StartsWithIgnoreCase(std::wstring_view hay, std::wstring_view prefix) {
    if (prefix.empty() || hay.size() < prefix.size()) return false;
    return EqualsIgnoreCase(hay.substr(0, prefix.size()), prefix);
}

bool ContainsAsciiInsensitiveW(std::wstring_view hay, std::wstring_view needle) {
    if (needle.empty() || hay.size() < needle.size()) return false;
    for (size_t i = 0; i + needle.size() <= hay.size(); ++i) {
        if (EqualsIgnoreCase(hay.substr(i, needle.size()), needle)) {
            return true;
        }
    }
    return false;
}

std::wstring FormatIsoDateW(DateYMD d) {
    wchar_t buf[16];
    swprintf_s(buf, L"%04d-%02d-%02d", d.year, d.month, d.day);
    return std::wstring(buf);
}

DateYMD TodayLocalDate() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    return {st.wYear, st.wMonth, st.wDay};
}

StorePurchaseOutcome MapPurchaseStatus(int32_t rawStatus) {
    switch (rawStatus) {
        case 0: return StorePurchaseOutcome::Succeeded;
        case 1: return StorePurchaseOutcome::AlreadyPurchased;
        case 2: return StorePurchaseOutcome::NotPurchased;
        case 3: return StorePurchaseOutcome::NetworkError;
        case 4: return StorePurchaseOutcome::ServerError;
        default: return StorePurchaseOutcome::ServerError;
    }
}

struct PurchaseCompletedHandler {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(PurchaseCompletedHandler* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(PurchaseCompletedHandler* self);
        ULONG (STDMETHODCALLTYPE* Release)(PurchaseCompletedHandler* self);
        HRESULT (STDMETHODCALLTYPE* Invoke)(PurchaseCompletedHandler* self, void* asyncOp, int32_t asyncStatus);
    };

    static HRESULT STDMETHODCALLTYPE QI(PurchaseCompletedHandler* self, REFIID riid, void** ppv) {
        if (!ppv) return E_POINTER;
        if (memcmp(&riid, &kIidUnknown, sizeof(IID)) == 0 ||
            memcmp(&riid, &kIidAgileObject, sizeof(IID)) == 0 ||
            memcmp(&riid, &kIidCompletedPurchaseHandler, sizeof(IID)) == 0) {
            *ppv = self;
            self->refCount.fetch_add(1, std::memory_order_relaxed);
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    static ULONG STDMETHODCALLTYPE AddRefFn(PurchaseCompletedHandler* self) {
        return self->refCount.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    static ULONG STDMETHODCALLTYPE ReleaseFn(PurchaseCompletedHandler* self) {
        ULONG remaining = self->refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (remaining == 0) {
            delete self;
        }
        return remaining;
    }

    static HRESULT STDMETHODCALLTYPE InvokeFn(PurchaseCompletedHandler* self, void* rawAsyncOp, int32_t asyncStatus) {
        StorePurchaseResponse resp{};
        if (asyncStatus == 2) {
            resp.outcome = StorePurchaseOutcome::NotPurchased;
            resp.hr = E_ABORT;
        } else if (asyncStatus != 1 || !rawAsyncOp) {
            resp.outcome = StorePurchaseOutcome::ServerError;
            resp.hr = E_FAIL;
        } else {
            auto* op = static_cast<AbiAsyncOperation*>(rawAsyncOp);
            void* rawRes = nullptr;
            HRESULT hr = op->lpVtbl->GetResults(op, &rawRes);
            resp.hr = hr;
            if (SUCCEEDED(hr) && rawRes) {
                auto* res = static_cast<AbiStorePurchaseResult*>(rawRes);
                int32_t statusVal = 2;
                HRESULT extErr = S_OK;
                res->lpVtbl->get_Status(res, &statusVal);
                res->lpVtbl->get_ExtendedError(res, &extErr);
                resp.outcome = MapPurchaseStatus(statusVal);
                resp.extendedError = extErr;
                SafeComRelease(res);
            } else {
                resp.outcome = StorePurchaseOutcome::NetworkError;
            }
        }

        if (self->callback) {
            auto cb = std::move(self->callback);
            cb(resp);
        }
        return S_OK;
    }

    static const Vtbl kVtbl;

    const Vtbl* lpVtbl = &kVtbl;
    std::atomic<ULONG> refCount{1};
    std::function<void(StorePurchaseResponse)> callback;
};

const PurchaseCompletedHandler::Vtbl PurchaseCompletedHandler::kVtbl = {
    &PurchaseCompletedHandler::QI,
    &PurchaseCompletedHandler::AddRefFn,
    &PurchaseCompletedHandler::ReleaseFn,
    &PurchaseCompletedHandler::InvokeFn
};

} // namespace

bool ConvertWinRtDateTimeToYmd(int64_t universalTime100ns, DateYMD& outDate) {
    if (universalTime100ns <= 0) return false;
    if (universalTime100ns >= 2650467743990000000LL) {
        outDate = {9999, 12, 31};
        return true;
    }

    uint64_t ticks = static_cast<uint64_t>(universalTime100ns);
    FILETIME ftUtc{};
    ftUtc.dwLowDateTime = static_cast<DWORD>(ticks & 0xFFFFFFFFULL);
    ftUtc.dwHighDateTime = static_cast<DWORD>(ticks >> 32);

    FILETIME ftLocal{};
    if (!FileTimeToLocalFileTime(&ftUtc, &ftLocal)) {
        ftLocal = ftUtc;
    }

    SYSTEMTIME st{};
    if (!FileTimeToSystemTime(&ftLocal, &st)) return false;
    outDate = {st.wYear, st.wMonth, st.wDay};
    return true;
}

LicenseTier ClassifyStoreAddOn(
    const std::wstring& skuStoreId,
    const std::wstring& offerToken,
    const std::wstring& configuredAnnualId,
    const std::wstring& configuredLifetimeId) {

    std::wstring sku = TrimWide(skuStoreId);
    std::wstring token = TrimWide(offerToken);

    if (!configuredLifetimeId.empty() && StartsWithIgnoreCase(sku, configuredLifetimeId)) {
        return LicenseTier::ProLifetime;
    }
    if (!configuredAnnualId.empty() && StartsWithIgnoreCase(sku, configuredAnnualId)) {
        return LicenseTier::ProAnnual;
    }

    if (EqualsIgnoreCase(token, commercial::kStoreOfferTokenLifetime) ||
        ContainsAsciiInsensitiveW(token, L"lifetime") ||
        ContainsAsciiInsensitiveW(token, L"omur")) {
        return LicenseTier::ProLifetime;
    }
    if (EqualsIgnoreCase(token, commercial::kStoreOfferTokenAnnual) ||
        ContainsAsciiInsensitiveW(token, L"annual") ||
        ContainsAsciiInsensitiveW(token, L"year")) {
        return LicenseTier::ProAnnual;
    }

    return LicenseTier::Free;
}

StoreLicenseSnapshot QueryMicrosoftStoreLicenseSync() {
    StoreLicenseSnapshot snap;
    if (!IsRunningPackaged()) {
        snap.hr = HRESULT_FROM_WIN32(kAppModelErrorNoPackage);
        return snap;
    }

    ComBaseApi api;
    if (!api.Load()) {
        snap.hr = E_NOINTERFACE;
        return snap;
    }

    HRESULT roHr = api.roInitialize(1 /* RO_INIT_MULTITHREADED */);
    bool shouldUninit = (roHr == S_OK || roHr == S_FALSE);

    WinRtHString hsClass = nullptr;
    AbiStoreContextStatics* statics = nullptr;
    AbiStoreContext* ctx = nullptr;
    AbiAsyncOperation* op = nullptr;
    AbiAsyncInfo* asyncInfo = nullptr;
    AbiStoreAppLicense* appLic = nullptr;
    void* rawMap = nullptr;
    AbiIterableKvp* iterable = nullptr;
    AbiIteratorKvp* iter = nullptr;

    do {
        constexpr wchar_t kClassName[] = L"Windows.Services.Store.StoreContext";
        snap.hr = api.createString(kClassName, static_cast<UINT32>(wcslen(kClassName)), &hsClass);
        if (FAILED(snap.hr) || !hsClass) break;

        snap.hr = api.roGetActivationFactory(hsClass, kIidStoreContextStatics, reinterpret_cast<void**>(&statics));
        if (FAILED(snap.hr) || !statics) break;

        void* rawCtx = nullptr;
        snap.hr = statics->lpVtbl->GetDefault(statics, &rawCtx);
        if (FAILED(snap.hr) || !rawCtx) break;
        ctx = static_cast<AbiStoreContext*>(rawCtx);

        snap.hr = ctx->lpVtbl->GetAppLicenseAsync(ctx, &op);
        if (FAILED(snap.hr) || !op) break;

        snap.hr = op->lpVtbl->QueryInterface(op, kIidIAsyncInfo, reinterpret_cast<void**>(&asyncInfo));
        if (FAILED(snap.hr) || !asyncInfo) break;

        int32_t asyncStatus = 0;
        for (int waitMs = 0; waitMs < 10000; waitMs += 20) {
            if (FAILED(asyncInfo->lpVtbl->get_Status(asyncInfo, &asyncStatus)) || asyncStatus != 0) {
                break;
            }
            Sleep(20);
        }

        if (asyncStatus != 1) {
            HRESULT errCode = E_FAIL;
            asyncInfo->lpVtbl->get_ErrorCode(asyncInfo, &errCode);
            snap.hr = FAILED(errCode) ? errCode : E_FAIL;
            break;
        }

        void* rawAppLic = nullptr;
        snap.hr = op->lpVtbl->GetResults(op, &rawAppLic);
        if (FAILED(snap.hr) || !rawAppLic) break;
        appLic = static_cast<AbiStoreAppLicense*>(rawAppLic);

        snap.storeAvailable = true;

        unsigned char isActive = 0;
        unsigned char isTrial = 0;
        appLic->lpVtbl->get_IsActive(appLic, &isActive);
        appLic->lpVtbl->get_IsTrial(appLic, &isTrial);
        snap.appLicenseActive = (isActive != 0);
        snap.appLicenseIsTrial = (isTrial != 0);

        if (FAILED(appLic->lpVtbl->get_AddOnLicenses(appLic, &rawMap)) || !rawMap) break;
        auto* rawMapUnk = static_cast<RawComUnknown*>(rawMap);
        if (FAILED(rawMapUnk->lpVtbl->QueryInterface(rawMapUnk, kIidIterableKvpLicense, reinterpret_cast<void**>(&iterable))) || !iterable) {
            break;
        }

        void* rawIter = nullptr;
        if (FAILED(iterable->lpVtbl->First(iterable, &rawIter)) || !rawIter) break;
        iter = static_cast<AbiIteratorKvp*>(rawIter);

        DateYMD today = TodayLocalDate();
        DateYMD bestAnnualExp{0, 0, 0};

        unsigned char hasCurr = 0;
        while (SUCCEEDED(iter->lpVtbl->get_HasCurrent(iter, &hasCurr)) && hasCurr != 0) {
            void* rawKvp = nullptr;
            if (SUCCEEDED(iter->lpVtbl->get_Current(iter, &rawKvp)) && rawKvp) {
                auto* kvp = static_cast<AbiKvpLicense*>(rawKvp);
                AbiStoreLicense* addLic = nullptr;
                if (SUCCEEDED(kvp->lpVtbl->get_Value(kvp, &addLic)) && addLic) {
                    unsigned char addActive = 0;
                    addLic->lpVtbl->get_IsActive(addLic, &addActive);
                    if (addActive != 0) {
                        WinRtHString hsSku = nullptr;
                        WinRtHString hsTok = nullptr;
                        addLic->lpVtbl->get_SkuStoreId(addLic, &hsSku);
                        addLic->lpVtbl->get_InAppOfferToken(addLic, &hsTok);
                        std::wstring sku = api.ConsumeHString(hsSku);
                        std::wstring tok = api.ConsumeHString(hsTok);

                        LicenseTier kind = ClassifyStoreAddOn(
                            sku, tok, commercial::kStoreAddonAnnualId, commercial::kStoreAddonLifetimeId);
                        if (kind == LicenseTier::ProLifetime) {
                            snap.hasProLifetime = true;
                        } else if (kind == LicenseTier::ProAnnual) {
                            int64_t expTicks = 0;
                            DateYMD expYmd{};
                            bool hasExp = SUCCEEDED(addLic->lpVtbl->get_ExpirationDate(addLic, &expTicks)) &&
                                          ConvertWinRtDateTimeToYmd(expTicks, expYmd);
                            bool expired = hasExp && (expYmd.year < 9999) && (expYmd < today);
                            if (!expired) {
                                snap.hasProAnnual = true;
                                if (hasExp && expYmd.year < 9999) {
                                    if (bestAnnualExp.year == 0 || bestAnnualExp < expYmd) {
                                        bestAnnualExp = expYmd;
                                        snap.annualExpiryIso = FormatIsoDateW(expYmd);
                                    }
                                }
                            }
                        }
                    }
                    SafeComRelease(addLic);
                }
                SafeComRelease(kvp);
            }
            unsigned char moved = 0;
            if (FAILED(iter->lpVtbl->MoveNext(iter, &moved)) || moved == 0) {
                break;
            }
        }
    } while (false);

    SafeComRelease(iter);
    SafeComRelease(iterable);
    SafeComRelease(rawMap);
    SafeComRelease(appLic);
    SafeComRelease(asyncInfo);
    SafeComRelease(op);
    SafeComRelease(ctx);
    SafeComRelease(statics);
    if (hsClass) api.deleteString(hsClass);
    if (shouldUninit) api.roUninitialize();

    return snap;
}

bool BeginMicrosoftStorePurchaseAsync(
    HWND ownerHwnd,
    const std::wstring& addOnStoreId,
    std::function<void(StorePurchaseResponse)> onDone) {

    if (!IsRunningPackaged()) {
        if (onDone) onDone({StorePurchaseOutcome::StoreUnavailable, HRESULT_FROM_WIN32(kAppModelErrorNoPackage), S_OK});
        return false;
    }

    std::wstring trimmedId = TrimWide(addOnStoreId);
    if (trimmedId.empty()) {
        if (onDone) onDone({StorePurchaseOutcome::NotConfigured, E_INVALIDARG, S_OK});
        return false;
    }

    ComBaseApi api;
    if (!api.Load()) {
        if (onDone) onDone({StorePurchaseOutcome::StoreUnavailable, E_NOINTERFACE, S_OK});
        return false;
    }

    api.roInitialize(0 /* RO_INIT_SINGLETHREADED */);

    WinRtHString hsClass = nullptr;
    WinRtHString hsAddon = nullptr;
    AbiStoreContextStatics* statics = nullptr;
    AbiStoreContext* ctx = nullptr;
    AbiInitWithWindow* initWin = nullptr;
    AbiAsyncOperation* op = nullptr;
    HRESULT failHr = E_FAIL;

    do {
        constexpr wchar_t kClassName[] = L"Windows.Services.Store.StoreContext";
        failHr = api.createString(kClassName, static_cast<UINT32>(wcslen(kClassName)), &hsClass);
        if (FAILED(failHr) || !hsClass) break;

        failHr = api.createString(trimmedId.c_str(), static_cast<UINT32>(trimmedId.size()), &hsAddon);
        if (FAILED(failHr) || !hsAddon) break;

        failHr = api.roGetActivationFactory(hsClass, kIidStoreContextStatics, reinterpret_cast<void**>(&statics));
        if (FAILED(failHr) || !statics) break;

        void* rawCtx = nullptr;
        failHr = statics->lpVtbl->GetDefault(statics, &rawCtx);
        if (FAILED(failHr) || !rawCtx) break;
        ctx = static_cast<AbiStoreContext*>(rawCtx);

        if (ownerHwnd) {
            failHr = ctx->lpVtbl->QueryInterface(ctx, kIidInitializeWithWindow, reinterpret_cast<void**>(&initWin));
            if (FAILED(failHr) || !initWin) break;
            failHr = initWin->lpVtbl->Initialize(initWin, ownerHwnd);
            if (FAILED(failHr)) break;
        }

        failHr = ctx->lpVtbl->RequestPurchaseAsync(ctx, hsAddon, &op);
        if (FAILED(failHr) || !op) break;

        auto* handler = new PurchaseCompletedHandler();
        handler->callback = std::move(onDone);
        failHr = op->lpVtbl->put_Completed(op, handler);
        PurchaseCompletedHandler::ReleaseFn(handler);
        if (FAILED(failHr)) break;

        SafeComRelease(op);
        SafeComRelease(initWin);
        SafeComRelease(ctx);
        SafeComRelease(statics);
        if (hsAddon) api.deleteString(hsAddon);
        if (hsClass) api.deleteString(hsClass);
        return true;
    } while (false);

    SafeComRelease(op);
    SafeComRelease(initWin);
    SafeComRelease(ctx);
    SafeComRelease(statics);
    if (hsAddon) api.deleteString(hsAddon);
    if (hsClass) api.deleteString(hsClass);

    if (onDone) onDone({StorePurchaseOutcome::StoreUnavailable, failHr, S_OK});
    return false;
}

} // namespace vakit
