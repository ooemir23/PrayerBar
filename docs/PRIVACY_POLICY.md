# Gizlilik Politikası — PrayerBar (Privacy Policy)

Son güncelleme: 2026-11-21

## Türkçe

**PrayerBar**, kullanıcı gizliliğine tam saygı gösterir:

1. **Takip ve Konum Geçmişi Yok:** Uygulama reklam, davranış analitiği veya kullanım telemetrisi kullanmaz ve konum geçmişi tutmaz. Konum tespiti ve şehir araması sırasında kullanılan dış servisler IP adresinizi işler; ayrıntılar aşağıdadır.
2. **Yerel Depolama:** Seçtiğiniz şehir, bildirim kuralları, görünüm tercihleri ve aylık namaz vakitleri önbelleği yalnızca kendi bilgisayarınızda (`%LocalAppData%\PrayerBar`) saklanır.
3. **İsteğe Bağlı Ağ İstekleri:**
   - "Diyanet/Aladhan aylık eşitlemesi" seçeneği açık olduğunda uygulama, seçtiğiniz şehir koordinatları için yalnızca ayda bir kez `api.aladhan.com` adresinden aylık vakit takvimini indirir. Özelliği kapattığınızda uygulama tamamen çevrimdışı astronomik hesaplama ile çalışır.
   - "Uygulama açıldığında konumu otomatik bul" seçeneği varsayılan olarak açıktır. Açıkken uygulama her başlatıldığında önce Windows Konum Hizmeti'ni (izin verilmişse) kullanır; kullanılamıyorsa IP tabanlı genel konum tespiti (`ipwho.is` / `get.geojs.io`) yapar. Seçeneği kapatarak bunu durdurabilir veya konumu elle seçebilirsiniz. "Konumumu Otomatik Bul" düğmesi tek seferlik tespit yapar.
   - Dünya genelinde şehir araması yaptığınızda arama metni ve IP adresiniz, koordinatları bulmak için Open-Meteo Geocoding API'ye (`geocoding-api.open-meteo.com`) gönderilir. Konum geçmişi tutulmaz.
4. **Arka Plan ve Kaynaklar:** Uygulama yalnızca kullanıcı açıkça etkinleştirdiğinde Windows oturum açılışında başlatılır.

---

## English

**PrayerBar** respects your privacy:

1. **No Tracking or Location History:** The app does not use advertising, behavioral analytics, or usage telemetry and does not retain location history. External providers used for location detection and city search process your IP address; see details below.
2. **Local Storage Only:** Your selected city, notification rules, display preferences, and monthly cached prayer schedules are stored exclusively on your local device under `%LocalAppData%\PrayerBar`.
3. **Optional Network Requests:**
   - When monthly schedule synchronization is enabled, the app performs an anonymous HTTPS request to `api.aladhan.com` using only the chosen city's coordinates and calculation method. Disabling online sync switches the app to 100% offline astronomical calculations.
   - **Automatically detect location at startup** is enabled by default. When enabled, the app first uses Windows Location (if permission is available) each time it starts and falls back to approximate IP geolocation (`ipwho.is` / `get.geojs.io`) when needed. Turn it off to stop this or choose a city manually. The **"Auto-detect location"** button performs a one-time lookup.
   - When you search worldwide for a city, the search text and IP address are sent to the Open-Meteo Geocoding API (`geocoding-api.open-meteo.com`) to find its coordinates. No location history is retained.
