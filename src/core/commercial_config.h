#pragma once

namespace vakit::commercial {

inline constexpr const wchar_t* kAppVersionW = L"2.0.0.0";
inline constexpr int kTrialDays = 14;

// Yalnızca Microsoft Store dağıtım ve uygulama içi satın alma (IAP) yapılandırması
// Partner Center'da uygulama ve iki eklenti (pro_annual, pro_lifetime) oluşturulduğunda
// Store ID değerlerini aşağıya yapıştırın.
inline constexpr bool kStoreOnlyMode = true;
inline constexpr const wchar_t* kStoreAppProductId = L"9NBLGGH00000";
inline constexpr const wchar_t* kStoreAddonAnnualId = L"";
inline constexpr const wchar_t* kStoreAddonLifetimeId = L"";
inline constexpr const wchar_t* kStoreOfferTokenAnnual = L"pro_annual";
inline constexpr const wchar_t* kStoreOfferTokenLifetime = L"pro_lifetime";

inline constexpr const wchar_t* kMicrosoftStorePdpUri = L"ms-windows-store://pdp/?productid=9NBLGGH00000";
inline constexpr const wchar_t* kMicrosoftStoreReviewUri = L"ms-windows-store://review/?productid=9NBLGGH00000";
inline constexpr const wchar_t* kMicrosoftAccountSubsUri = L"https://account.microsoft.com/services";

// Eski web lisans doğrulama sabitleri (birim test ve geriye dönük uyumluluk için korunur)
inline constexpr const wchar_t* kWebsiteUrl = L"https://vakitcubugu.com";
inline constexpr const wchar_t* kCheckoutAnnualUrl = L"ms-windows-store://pdp/?productid=9NBLGGH00000";
inline constexpr const wchar_t* kCheckoutLifetimeUrl = L"ms-windows-store://pdp/?productid=9NBLGGH00000";
inline constexpr const wchar_t* kSupportTipUrl = L"ms-windows-store://review/?productid=9NBLGGH00000";
inline constexpr const wchar_t* kLemonSqueezyHost = L"api.lemonsqueezy.com";
inline constexpr const wchar_t* kLemonSqueezyActivatePath = L"/v1/licenses/activate";
inline constexpr const wchar_t* kGumroadHost = L"api.gumroad.com";
inline constexpr const wchar_t* kGumroadVerifyPath = L"/v2/licenses/verify";
inline constexpr const char* kGumroadProductPermalink = "";

// scripts/generate-license-key.ps1 ile birebir aynı olmalıdır
inline constexpr const char* kLicenseSignatureSalt = "VakitCubugu::Pro::Global::2026::v2";

} // namespace vakit::commercial
