#include "city_catalog.h"
#include "utf_util.h"

#include <algorithm>
#include <cmath>

namespace vakit {

namespace {

constexpr wchar_t kTrTz[] = L"Turkey Standard Time";

double HaversineKm(double lat1, double lon1, double lat2, double lon2) {
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kR = 6371.0;
    double dLat = (lat2 - lat1) * kPi / 180.0;
    double dLon = (lon2 - lon1) * kPi / 180.0;
    double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
               std::cos(lat1 * kPi / 180.0) * std::cos(lat2 * kPi / 180.0) *
               std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    return kR * 2.0 * std::atan2(std::sqrt(a), std::sqrt(std::max(0.0, 1.0 - a)));
}

} // namespace

const std::vector<CityLocation>& GetAllCities() {
    static const std::vector<CityLocation> kCities = {
        // Türkiye — 81 İl
        {L"istanbul", L"İstanbul", L"Türkiye", L"TR", 41.0082, 28.9784, kTrTz, L"Istanbul"},
        {L"ankara", L"Ankara", L"Türkiye", L"TR", 39.9334, 32.8597, kTrTz, L""},
        {L"izmir", L"İzmir", L"Türkiye", L"TR", 38.4237, 27.1428, kTrTz, L"Izmir"},
        {L"bursa", L"Bursa", L"Türkiye", L"TR", 40.1885, 29.0610, kTrTz, L""},
        {L"antalya", L"Antalya", L"Türkiye", L"TR", 36.8969, 30.7133, kTrTz, L""},
        {L"adana", L"Adana", L"Türkiye", L"TR", 37.0000, 35.3213, kTrTz, L""},
        {L"konya", L"Konya", L"Türkiye", L"TR", 37.8746, 32.4932, kTrTz, L""},
        {L"gaziantep", L"Gaziantep", L"Türkiye", L"TR", 37.0662, 37.3833, kTrTz, L""},
        {L"sanliurfa", L"Şanlıurfa", L"Türkiye", L"TR", 37.1674, 38.7955, kTrTz, L"Urfa Sanliurfa"},
        {L"kocaeli", L"Kocaeli", L"Türkiye", L"TR", 40.8533, 29.8815, kTrTz, L"Izmit"},
        {L"mersin", L"Mersin", L"Türkiye", L"TR", 36.8121, 34.6415, kTrTz, L"Icel"},
        {L"diyarbakir", L"Diyarbakır", L"Türkiye", L"TR", 37.9144, 40.2306, kTrTz, L"Diyarbakir"},
        {L"hatay", L"Hatay", L"Türkiye", L"TR", 36.2026, 36.1604, kTrTz, L"Antakya"},
        {L"manisa", L"Manisa", L"Türkiye", L"TR", 38.6191, 27.4289, kTrTz, L""},
        {L"kayseri", L"Kayseri", L"Türkiye", L"TR", 38.7312, 35.4787, kTrTz, L""},
        {L"samsun", L"Samsun", L"Türkiye", L"TR", 41.2867, 36.3300, kTrTz, L""},
        {L"balikesir", L"Balıkesir", L"Türkiye", L"TR", 39.6484, 27.8826, kTrTz, L"Balikesir"},
        {L"kahramanmaras", L"Kahramanmaraş", L"Türkiye", L"TR", 37.5858, 36.9371, kTrTz, L"Maras Kahramanmaras"},
        {L"van", L"Van", L"Türkiye", L"TR", 38.5012, 43.3730, kTrTz, L""},
        {L"aydin", L"Aydın", L"Türkiye", L"TR", 37.8560, 27.8416, kTrTz, L"Aydin"},
        {L"tekirdag", L"Tekirdağ", L"Türkiye", L"TR", 40.9833, 27.5167, kTrTz, L"Tekirdag"},
        {L"sakarya", L"Sakarya", L"Türkiye", L"TR", 40.7569, 30.3783, kTrTz, L"Adapazari"},
        {L"denizli", L"Denizli", L"Türkiye", L"TR", 37.7765, 29.0864, kTrTz, L""},
        {L"mugla", L"Muğla", L"Türkiye", L"TR", 37.2153, 28.3636, kTrTz, L"Mugla"},
        {L"eskisehir", L"Eskişehir", L"Türkiye", L"TR", 39.7767, 30.5206, kTrTz, L"Eskisehir"},
        {L"mardin", L"Mardin", L"Türkiye", L"TR", 37.3212, 40.7245, kTrTz, L""},
        {L"trabzon", L"Trabzon", L"Türkiye", L"TR", 41.0027, 39.7168, kTrTz, L""},
        {L"malatya", L"Malatya", L"Türkiye", L"TR", 38.3552, 38.3095, kTrTz, L""},
        {L"ordu", L"Ordu", L"Türkiye", L"TR", 40.9862, 37.8797, kTrTz, L""},
        {L"erzurum", L"Erzurum", L"Türkiye", L"TR", 39.9000, 41.2700, kTrTz, L""},
        {L"afyonkarahisar", L"Afyonkarahisar", L"Türkiye", L"TR", 38.7507, 30.5567, kTrTz, L"Afyon"},
        {L"sivas", L"Sivas", L"Türkiye", L"TR", 39.7477, 37.0179, kTrTz, L""},
        {L"adiyaman", L"Adıyaman", L"Türkiye", L"TR", 37.7648, 38.2786, kTrTz, L"Adiyaman"},
        {L"tokat", L"Tokat", L"Türkiye", L"TR", 40.3167, 36.5500, kTrTz, L""},
        {L"zonguldak", L"Zonguldak", L"Türkiye", L"TR", 41.4564, 31.7987, kTrTz, L""},
        {L"elazig", L"Elazığ", L"Türkiye", L"TR", 38.6810, 39.2264, kTrTz, L"Elazig"},
        {L"batman", L"Batman", L"Türkiye", L"TR", 37.8812, 41.1351, kTrTz, L""},
        {L"corum", L"Çorum", L"Türkiye", L"TR", 40.5506, 34.9556, kTrTz, L"Corum"},
        {L"canakkale", L"Çanakkale", L"Türkiye", L"TR", 40.1553, 26.4142, kTrTz, L"Canakkale"},
        {L"kutahya", L"Kütahya", L"Türkiye", L"TR", 39.4167, 29.9833, kTrTz, L"Kutahya"},
        {L"sirnak", L"Şırnak", L"Türkiye", L"TR", 37.5164, 42.4611, kTrTz, L"Sirnak"},
        {L"osmaniye", L"Osmaniye", L"Türkiye", L"TR", 37.0742, 36.2478, kTrTz, L""},
        {L"giresun", L"Giresun", L"Türkiye", L"TR", 40.9128, 38.3895, kTrTz, L""},
        {L"agri", L"Ağrı", L"Türkiye", L"TR", 39.7191, 43.0503, kTrTz, L"Agri"},
        {L"isparta", L"Isparta", L"Türkiye", L"TR", 37.7648, 30.5566, kTrTz, L""},
        {L"aksaray", L"Aksaray", L"Türkiye", L"TR", 38.3687, 34.0370, kTrTz, L""},
        {L"yozgat", L"Yozgat", L"Türkiye", L"TR", 39.8181, 34.8147, kTrTz, L""},
        {L"edirne", L"Edirne", L"Türkiye", L"TR", 41.6771, 26.5557, kTrTz, L""},
        {L"duzce", L"Düzce", L"Türkiye", L"TR", 40.8438, 31.1565, kTrTz, L"Duzce"},
        {L"mus", L"Muş", L"Türkiye", L"TR", 38.7433, 41.5065, kTrTz, L"Mus"},
        {L"kastamonu", L"Kastamonu", L"Türkiye", L"TR", 41.3887, 33.7827, kTrTz, L""},
        {L"usak", L"Uşak", L"Türkiye", L"TR", 38.6823, 29.4082, kTrTz, L"Usak"},
        {L"kirklareli", L"Kırklareli", L"Türkiye", L"TR", 41.7333, 27.2167, kTrTz, L"Kirklareli"},
        {L"nigde", L"Niğde", L"Türkiye", L"TR", 37.9667, 34.6833, kTrTz, L"Nigde"},
        {L"bitlis", L"Bitlis", L"Türkiye", L"TR", 38.4006, 42.1095, kTrTz, L""},
        {L"rize", L"Rize", L"Türkiye", L"TR", 41.0201, 40.5234, kTrTz, L""},
        {L"amasya", L"Amasya", L"Türkiye", L"TR", 40.6499, 35.8353, kTrTz, L""},
        {L"siirt", L"Siirt", L"Türkiye", L"TR", 37.9333, 41.9500, kTrTz, L""},
        {L"bolu", L"Bolu", L"Türkiye", L"TR", 40.7392, 31.6089, kTrTz, L""},
        {L"nevsehir", L"Nevşehir", L"Türkiye", L"TR", 38.6244, 34.7239, kTrTz, L"Nevsehir"},
        {L"yalova", L"Yalova", L"Türkiye", L"TR", 40.6500, 29.2667, kTrTz, L""},
        {L"bingol", L"Bingöl", L"Türkiye", L"TR", 38.8855, 40.4966, kTrTz, L"Bingol"},
        {L"kars", L"Kars", L"Türkiye", L"TR", 40.6167, 43.1000, kTrTz, L""},
        {L"kirikkale", L"Kırıkkale", L"Türkiye", L"TR", 39.8468, 33.5153, kTrTz, L"Kirikkale"},
        {L"hakkari", L"Hakkari", L"Türkiye", L"TR", 37.5744, 43.7408, kTrTz, L""},
        {L"burdur", L"Burdur", L"Türkiye", L"TR", 37.7203, 30.2908, kTrTz, L""},
        {L"karaman", L"Karaman", L"Türkiye", L"TR", 37.1759, 33.2287, kTrTz, L""},
        {L"karabuk", L"Karabük", L"Türkiye", L"TR", 41.2061, 32.6204, kTrTz, L"Karabuk"},
        {L"kirsehir", L"Kırşehir", L"Türkiye", L"TR", 39.1425, 34.1709, kTrTz, L"Kirsehir"},
        {L"erzincan", L"Erzincan", L"Türkiye", L"TR", 39.7500, 39.5000, kTrTz, L""},
        {L"bilecik", L"Bilecik", L"Türkiye", L"TR", 40.1451, 29.9799, kTrTz, L""},
        {L"sinop", L"Sinop", L"Türkiye", L"TR", 42.0231, 35.1531, kTrTz, L""},
        {L"igdir", L"Iğdır", L"Türkiye", L"TR", 39.9167, 44.0333, kTrTz, L"Igdir"},
        {L"bartin", L"Bartın", L"Türkiye", L"TR", 41.6344, 32.3375, kTrTz, L"Bartin"},
        {L"cankiri", L"Çankırı", L"Türkiye", L"TR", 40.6013, 33.6134, kTrTz, L"Cankiri"},
        {L"artvin", L"Artvin", L"Türkiye", L"TR", 41.1828, 41.8183, kTrTz, L""},
        {L"gumushane", L"Gümüşhane", L"Türkiye", L"TR", 40.4386, 39.5086, kTrTz, L"Gumushane"},
        {L"kilis", L"Kilis", L"Türkiye", L"TR", 36.7184, 37.1212, kTrTz, L""},
        {L"ardahan", L"Ardahan", L"Türkiye", L"TR", 41.1105, 42.7022, kTrTz, L""},
        {L"tunceli", L"Tunceli", L"Türkiye", L"TR", 39.1079, 39.5401, kTrTz, L""},
        {L"bayburt", L"Bayburt", L"Türkiye", L"TR", 40.2552, 40.2249, kTrTz, L""},

        // Popüler İlçeler
        {L"kadikoy", L"Kadıköy (İstanbul)", L"Türkiye", L"TR", 40.9903, 29.0297, kTrTz, L"Kadikoy Istanbul"},
        {L"cankaya", L"Çankaya (Ankara)", L"Türkiye", L"TR", 39.9030, 32.8597, kTrTz, L"Cankaya Ankara"},
        {L"konak", L"Konak (İzmir)", L"Türkiye", L"TR", 38.4189, 27.1287, kTrTz, L"Konak Izmir"},
        {L"alanya", L"Alanya (Antalya)", L"Türkiye", L"TR", 36.5444, 31.9954, kTrTz, L"Alanya Antalya"},
        {L"gebze", L"Gebze (Kocaeli)", L"Türkiye", L"TR", 40.8028, 29.4307, kTrTz, L"Gebze Kocaeli"},
        {L"bandirma", L"Bandırma (Balıkesir)", L"Türkiye", L"TR", 40.3522, 27.9767, kTrTz, L"Bandirma Balikesir"},
        {L"inegol", L"İnegöl (Bursa)", L"Türkiye", L"TR", 40.0781, 29.5133, kTrTz, L"Inegol Bursa"},
        {L"bodrum", L"Bodrum (Muğla)", L"Türkiye", L"TR", 37.0344, 27.4305, kTrTz, L"Bodrum Mugla"},
        {L"fethiye", L"Fethiye (Muğla)", L"Türkiye", L"TR", 36.6512, 29.1231, kTrTz, L"Fethiye Mugla"},
        {L"tarsus", L"Tarsus (Mersin)", L"Türkiye", L"TR", 36.9166, 34.8952, kTrTz, L"Tarsus Mersin"},

        // Dünya Şehirleri
        {L"lefkosa", L"Lefkoşa", L"KKTC", L"CY", 35.1856, 33.3823, L"GTB Standard Time", L"Nicosia Lefkosa"},
        {L"baku", L"Bakü", L"Azerbaycan", L"AZ", 40.4093, 49.8671, L"Azerbaijan Standard Time", L"Baku Azerbaijan"},
        {L"sarajevo", L"Saraybosna", L"Bosna Hersek", L"BA", 43.8563, 18.4131, L"Central European Standard Time", L"Sarajevo"},
        {L"makkah", L"Mekke", L"Suudi Arabistan", L"SA", 21.4225, 39.8262, L"Arab Standard Time", L"Makkah Mecca"},
        {L"madinah", L"Medine", L"Suudi Arabistan", L"SA", 24.4672, 39.6111, L"Arab Standard Time", L"Madinah Medina"},
        {L"riyadh", L"Riyad", L"Suudi Arabistan", L"SA", 24.7136, 46.6753, L"Arab Standard Time", L"Riyadh Saudi Arabia"},
        {L"berlin", L"Berlin", L"Almanya", L"DE", 52.5200, 13.4050, L"W. Europe Standard Time", L"Germany Almanya"},
        {L"munich", L"Münih", L"Almanya", L"DE", 48.1351, 11.5820, L"W. Europe Standard Time", L"Munich Munchen Almanya"},
        {L"cologne", L"Köln", L"Almanya", L"DE", 50.9375, 6.9603, L"W. Europe Standard Time", L"Koln Cologne Almanya"},
        {L"frankfurt", L"Frankfurt", L"Almanya", L"DE", 50.1109, 8.6821, L"W. Europe Standard Time", L"Germany Almanya"},
        {L"london", L"Londra", L"Birleşik Krallık", L"GB", 51.5072, -0.1276, L"GMT Standard Time", L"London UK"},
        {L"paris", L"Paris", L"Fransa", L"FR", 48.8566, 2.3522, L"Romance Standard Time", L"France Fransa"},
        {L"amsterdam", L"Amsterdam", L"Hollanda", L"NL", 52.3676, 4.9041, L"W. Europe Standard Time", L"Netherlands Hollanda"},
        {L"brussels", L"Brüksel", L"Belçika", L"BE", 50.8503, 4.3517, L"Romance Standard Time", L"Brussels Belcika"},
        {L"vienna", L"Viyana", L"Avusturya", L"AT", 48.2082, 16.3738, L"W. Europe Standard Time", L"Vienna Wien Austria"},
        {L"newyork", L"New York", L"ABD", L"US", 40.7128, -74.0060, L"Eastern Standard Time", L"USA Amerika"},
        {L"chicago", L"Chicago", L"ABD", L"US", 41.8781, -87.6298, L"Central Standard Time", L"USA Amerika"},
        {L"toronto", L"Toronto", L"Kanada", L"CA", 43.6532, -79.3832, L"Eastern Standard Time", L"Canada Kanada"},
        {L"cairo", L"Kahire", L"Mısır", L"EG", 30.0444, 31.2357, L"Egypt Standard Time", L"Cairo Misir Egypt"},
        {L"doha", L"Doha", L"Katar", L"QA", 25.2854, 51.5310, L"Arab Standard Time", L"Qatar Katar"},
        {L"dubai", L"Dubai", L"BAE", L"AE", 25.2048, 55.2708, L"Arabian Standard Time", L"UAE BAE"},
        {L"kuwait", L"Kuveyt", L"Kuveyt", L"KW", 29.3759, 47.9774, L"Arab Standard Time", L"Kuwait"},
        {L"amman", L"Amman", L"Ürdün", L"JO", 31.9454, 35.9284, L"Jordan Standard Time", L"Jordan Urdun"},
        {L"tehran", L"Tahran", L"İran", L"IR", 35.6892, 51.3890, L"Iran Standard Time", L"Tehran Iran"},
        {L"karachi", L"Karaçi", L"Pakistan", L"PK", 24.8607, 67.0011, L"Pakistan Standard Time", L"Karachi Pakistan"},
        {L"islamabad", L"İslamabad", L"Pakistan", L"PK", 33.6844, 73.0479, L"Pakistan Standard Time", L"Islamabad Pakistan"},
        {L"kualalumpur", L"Kuala Lumpur", L"Malezya", L"MY", 3.1390, 101.6869, L"Singapore Standard Time", L"Malaysia Malezya"},
        {L"jakarta", L"Cakarta", L"Endonezya", L"ID", -6.2088, 106.8456, L"SE Asia Standard Time", L"Jakarta Indonesia Endonezya"},
        {L"singapore", L"Singapur", L"Singapur", L"SG", 1.3521, 103.8198, L"Singapore Standard Time", L"Singapore"},
        {L"tunis", L"Tunus", L"Tunus", L"TN", 36.8065, 10.1815, L"W. Central Africa Standard Time", L"Tunisia Tunis"},
        {L"algiers", L"Cezayir", L"Cezayir", L"DZ", 36.7538, 3.0588, L"W. Central Africa Standard Time", L"Algeria Algiers"},
        {L"casablanca", L"Kazablanka", L"Fas", L"MA", 33.5731, -7.5898, L"Morocco Standard Time", L"Casablanca Morocco Fas"},
        {L"lisbon", L"Lizbon", L"Portekiz", L"PT", 38.7223, -9.1393, L"GMT Standard Time", L"Lisbon Portugal Portekiz"},
        {L"moscow", L"Moskova", L"Rusya", L"RU", 55.7558, 37.6173, L"Russian Standard Time", L"Moscow Russia Rusya"},
        {L"tokyo", L"Tokyo", L"Japonya", L"JP", 35.6762, 139.6503, L"Tokyo Standard Time", L"Japan"},
        {L"sydney", L"Sidney", L"Avustralya", L"AU", -33.8688, 151.2093, L"AUS Eastern Standard Time", L"Sydney Australia"},
        {L"adelaide", L"Adelaide", L"Avustralya", L"AU", -34.9285, 138.6007, L"Cen. Australia Standard Time", L"Adelaide Australia"},
        {L"melbourne", L"Melbourne", L"Avustralya", L"AU", -37.8136, 144.9631, L"AUS Eastern Standard Time", L"Melbourne Australia"},
        {L"brisbane", L"Brisbane", L"Avustralya", L"AU", -27.4698, 153.0251, L"E. Australia Standard Time", L"Brisbane Australia"},
        {L"perth", L"Perth", L"Avustralya", L"AU", -31.9505, 115.8605, L"W. Australia Standard Time", L"Perth Australia"},
        {L"darwin", L"Darwin", L"Avustralya", L"AU", -12.4634, 130.8456, L"AUS Central Standard Time", L"Darwin Australia"},
        {L"canberra", L"Canberra", L"Avustralya", L"AU", -35.2809, 149.1300, L"AUS Eastern Standard Time", L"Canberra Australia"}
    };
    return kCities;
}

const CityLocation& GetDefaultCity() {
    return GetAllCities()[0];
}

std::optional<CityLocation> FindCityById(std::wstring_view id) {
    if (id.empty()) return std::nullopt;
    std::wstring needle = NormalizeTurkishLower(id);
    for (const auto& city : GetAllCities()) {
        if (NormalizeTurkishLower(city.id) == needle) {
            return city;
        }
    }
    return std::nullopt;
}

std::optional<CityLocation> FindNearestCity(double latitude, double longitude, double maxDistanceKm) {
    const CityLocation* best = nullptr;
    double bestDist = 1e9;
    for (const auto& city : GetAllCities()) {
        double dist = HaversineKm(latitude, longitude, city.latitude, city.longitude);
        if (dist < bestDist) {
            bestDist = dist;
            best = &city;
        }
    }
    if (best && bestDist <= maxDistanceKm) {
        return *best;
    }
    return std::nullopt;
}

std::vector<CityLocation> SearchCities(std::wstring_view query, size_t maxResults) {
    const auto& all = GetAllCities();
    std::wstring trimmed = TrimWide(query);
    if (trimmed.empty()) {
        size_t n = std::min(maxResults, all.size());
        return std::vector<CityLocation>(all.begin(), all.begin() + static_cast<ptrdiff_t>(n));
    }

    std::wstring needle = NormalizeTurkishLower(trimmed);
    std::vector<CityLocation> prefix;
    std::vector<CityLocation> contains;

    for (const auto& city : all) {
        std::wstring normName = NormalizeTurkishLower(city.name);
        if (normName.rfind(needle, 0) == 0) {
            prefix.push_back(city);
        } else {
            std::wstring hay = normName + L" " + NormalizeTurkishLower(city.country) + L" " + NormalizeTurkishLower(city.searchAliases);
            if (hay.find(needle) != std::wstring::npos) {
                contains.push_back(city);
            }
        }
    }

    prefix.insert(prefix.end(), contains.begin(), contains.end());
    if (prefix.size() > maxResults) {
        prefix.resize(maxResults);
    }
    return prefix;
}

std::wstring ResolveCountryCodeFromName(std::wstring_view countryName) {
    std::wstring norm = NormalizeTurkishLower(TrimWide(countryName));
    if (norm == L"turkiye" || norm == L"turkey" || norm == L"kktc") return L"TR";
    if (norm == L"suudi arabistan" || norm == L"saudi arabia") return L"SA";
    if (norm == L"misir" || norm == L"egypt") return L"EG";
    if (norm == L"pakistan") return L"PK";
    if (norm == L"iran") return L"IR";
    if (norm == L"katar" || norm == L"qatar") return L"QA";
    if (norm == L"kuveyt" || norm == L"kuwait") return L"KW";
    if (norm == L"bae" || norm == L"uae") return L"AE";
    if (norm == L"singapur" || norm == L"singapore") return L"SG";
    if (norm == L"malezya" || norm == L"malaysia") return L"MY";
    if (norm == L"endonezya" || norm == L"indonesia") return L"ID";
    if (norm == L"tunus" || norm == L"tunisia") return L"TN";
    if (norm == L"cezayir" || norm == L"algeria") return L"DZ";
    if (norm == L"fas" || norm == L"morocco") return L"MA";
    if (norm == L"urdun" || norm == L"jordan") return L"JO";
    if (norm == L"portekiz" || norm == L"portugal") return L"PT";
    if (norm == L"rusya" || norm == L"russia") return L"RU";
    if (norm == L"fransa" || norm == L"france") return L"FR";
    if (norm == L"abd" || norm == L"usa" || norm == L"kanada" || norm == L"canada") return L"US";
    return L"";
}

} // namespace vakit
