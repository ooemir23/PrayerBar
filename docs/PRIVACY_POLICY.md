# Gizlilik Politikası — PrayerBar (Privacy Policy)

Son güncelleme: 2026-11-21

## Türkçe

**PrayerBar**, kullanıcı gizliliğine tam saygı gösterir:

1. **İzleme Yok:** Uygulama konum geçmişi, cihaz kimliği veya kullanım telemetrisi toplamaz ya da saklamaz. Aşağıdaki isteğe bağlı özellikler için gereken sınırlı veriler ilgili hizmetlere iletilir.
2. **Yerel Depolama:** Seçtiğiniz şehir, bildirim kuralları, görünüm tercihleri ve aylık namaz vakitleri önbelleği yalnızca kendi bilgisayarınızda (`%LocalAppData%\PrayerBar`) saklanır.
3. **İsteğe Bağlı Ağ İstekleri:**
   - "Diyanet/Aladhan aylık eşitlemesi" seçeneği açık olduğunda uygulama, seçtiğiniz şehir koordinatları için yalnızca ayda bir kez `api.aladhan.com` adresinden aylık vakit takvimini indirir. Özelliği kapattığınızda uygulama tamamen çevrimdışı astronomik hesaplama ile çalışır.
   - Ayarlardaki **"Açılışta konumumu otomatik güncelle"** seçeneği varsayılan olarak açıktır. Uygulama açılışında önce Windows Konum Hizmeti kullanılır (çevrimdışı katalog dışındaki koordinatlar için şehir adını çözmek üzere `api.bigdatacloud.net` adresine yalnızca koordinat gönderilebilir); kullanılamazsa IP tabanlı yaklaşık konum (`ipwho.is`, `free.freeipapi.com`, `ipapi.co`, `ipinfo.io`, `get.geojs.io`) denenir. Bu seçeneği kapatabilir veya konumu elle seçebilirsiniz.
   - Şehir araması yazarken arama sorgusu ve bağlantı IP adresi, dünya çapındaki şehir sonuçları için `geocoding-api.open-meteo.com` hizmetine gönderilir. Yerleşik şehir kataloğu çevrimdışı kullanılabilir.
4. **Arka Plan ve Kaynaklar:** Uygulama yalnızca kullanıcı açıkça etkinleştirdiğinde Windows oturum açılışında başlatılır.

---

## English

**PrayerBar** respects your privacy:

1. **No Tracking:** The app does not collect or store location history, device identifiers, or usage telemetry. Limited data required by the optional features below is sent to the relevant service providers.
2. **Local Storage Only:** Your selected city, notification rules, display preferences, and monthly cached prayer schedules are stored exclusively on your local device under `%LocalAppData%\PrayerBar`.
3. **Optional Network Requests:**
   - When monthly schedule synchronization is enabled, the app performs an anonymous HTTPS request to `api.aladhan.com` using only the chosen city's coordinates and calculation method. Disabling online sync switches the app to 100% offline astronomical calculations.
   - **Automatically update my location at startup** is enabled by default. At startup, the app tries Windows Location first (for coordinates outside the offline city catalog, coordinates may be sent to `api.bigdatacloud.net` to resolve the city name); if unavailable, it may use approximate IP geolocation (`ipwho.is`, `free.freeipapi.com`, `ipapi.co`, `ipinfo.io`, `get.geojs.io`). You can turn this off or choose a location manually.
   - While you search for a city, the search query and your connection IP address are sent to `geocoding-api.open-meteo.com` to return worldwide city results. The built-in city catalog remains available offline.
