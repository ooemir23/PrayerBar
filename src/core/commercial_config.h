#pragma once

namespace vakit::commercial {

inline constexpr const wchar_t* kAppVersionW = L"2.4.1.0";
inline constexpr int kTrialDays = 14;

// Yalnızca Microsoft Store dağıtım ve uygulama içi satın alma (IAP) yapılandırması
// Partner Center'da uygulama ve iki eklenti (pro_annual, pro_lifetime) oluşturulduğunda
// Store ID değerlerini aşağıya yapıştırın.
inline constexpr const wchar_t* kStoreAppProductId = L"9MSQBZTPGWX6";
inline constexpr const wchar_t* kStoreAddonAnnualId = L"9NTMQDF6DLKM";
inline constexpr const wchar_t* kStoreAddonLifetimeId = L"9P043N2ZZ5XN";
inline constexpr const wchar_t* kStoreOfferTokenAnnual = L"pro_annual";
inline constexpr const wchar_t* kStoreOfferTokenLifetime = L"pro_lifetime";

inline constexpr const wchar_t* kMicrosoftStorePdpUri = L"ms-windows-store://pdp/?productid=9MSQBZTPGWX6";
inline constexpr const wchar_t* kMicrosoftStoreReviewUri = L"ms-windows-store://review/?productid=9MSQBZTPGWX6";
inline constexpr const wchar_t* kMicrosoftAccountSubsUri = L"https://account.microsoft.com/services";

} // namespace vakit::commercial
