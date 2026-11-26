# Microsoft Partner Center — Adım Adım Yayınlama Başvuru Formu (PrayerBar v2.1.0.0)

Partner Center'da (`partner.microsoft.com` ➔ **PrayerBar: Prayer Times & Adhan** ➔ **Start submission**) soldaki menüyü **yukarıdan aşağıya** şu değerlerle doldurup tek seferde **Submit to the Store** diyebilirsiniz.

---

## 1. Pricing and availability (Fiyatlandırma ve Kullanılabilirlik)
- **Base price (Ana uygulama fiyatı):** `Free` *(Uygulama içi satın alma destekli ücretsiz model)*
- **Markets (Pazarlar):** `All markets` *(Dünya geneli — 240+ ülke/bölge)*
- **Visibility:** `Make this product available and discoverable in the Store`
- **Schedule / Release date:** `Release as soon as it passes certification` *(Onaylanınca hemen yayınla)*
- **Add-ons (Eklenti Kontrolü):** İki eklentinin de *Submitted / In the Store* durumda olduğundan emin olun:
  - `pro_annual` (`9NTMQDF6DLKM`) — Yıllık Abonelik (`100 TL` / `$4.99` baz katman)
  - `pro_lifetime` (`9P043N2ZZ5XN`) — Ömür Boyu Lisans (`250 TL` / `$9.99` baz katman)

---

## 2. Properties (Özellikler & Kategoriler)
- **Primary category (Ana kategori):** `Productivity`
- **Subcategory (Alt kategori):** `Utilities & tools` *(Yedek: `Personalization`)*
- **Support info / Website URL:** `https://prayerbar.ooemir.com/`
- **Support contact (email):** Partner Center hesabınızdaki destek e-postası
- **Privacy policy URL (Zorunlu):**
  ```text
  https://prayerbar.ooemir.com/privacy
  ```
- **System requirements / Hardware:**
  - Minimum OS: Windows 10 version 19041.0 (2004) veya Windows 11 (`x64`)
  - RAM: ~3 MB çalışma belleği (her modern/eski Windows PC'de sorunsuz)
- **Product declarations:**
  - Uygulama reklam içeriyor mu? → `No`
  - Kullanıcı girişi gerektiriyor mu? → `No` *(Çekirdek tüm özellikler hesap açmadan çalışır)*

---

## 3. Age ratings (IARC Yaş Derecelendirmesi)
Ankette şu şekilde seçin *(tüm dünyada `3+` / `Everyone` derecesi verir)*:
- **Category:** `Reference, News, or Educational` veya `Utility` *(Eğitim / Yardımcı Araç)*
- Şiddet, korku, cinsellik, küfür, kumar, uyuşturucu: Tüm sorular **`No`**
- Kullanıcılar arası sohbet / kullanıcı içeriği paylaşımı: **`No`**
- Kullanıcının kişisel bilgilerini üçüncü taraflarla paylaşma: **`No`** *(Zero Telemetry)*
- İsteğe bağlı konum bilgisi: Şehir koordinatı / IP tabanlı tek seferlik şehir eşleştirme (sürekli GPS takibi yoktur; **`Yes`** veya şehir seçimi kapsamında beyan edilir)
- Dijital satın alma (In-app purchases): **`Yes`** *(Pro Yıllık & Ömür Boyu eklentileri)*

---

## 4. Packages (Uygulama Paketi Yükleme)
Yüklenecek hazır ve doğrulanmış MSIX dosyası:
- **Dosya Yolu:** [PrayerBar_2.1.0.0_x64_Store.msix](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/dist/release/PrayerBar_2.1.0.0_x64_Store.msix)
- **Sürüm:** `2.1.0.0` (`x64`)
- **Identity / Publisher:** `BomeraGdaLimitedirketi.PrayerBar` / `CN=8C3658B6-BFA1-4074-AF50-3C28C1865B0C` (`Bomera Gıda Limited Şirketi`)
- **SHA256:** `4DC2432F98BC100AEB00587C4CB4AB4D4613B3ED8B62D58AEA240C822128320B`
- **Device families:** `Windows.Desktop` *(Otomatik seçili gelir)*

---

## 5. Store listings (Mağaza Vitrinleri, Metinler & Görseller)
Başlık, kısa açıklama, 7 anahtar kelime ve tam açıklamalar **12 dil** için eksiksiz olarak [docs/STORE_LISTINGS_GLOBAL.md](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/docs/STORE_LISTINGS_GLOBAL.md) dosyasındadır. En azından **`English (en-US)`** ve **`Turkish (tr-TR)`** dillerini mutlaka doldurun (kalan 10 dili de tek tek *Add language* ile ekleyebilirsiniz).

### Yüklenecek Store Logo ve Tanıtım Görselleri
Klasör: `dist\store-assets\logos\`
- **Store Logo (300×300):** [StoreLogo_300x300.png](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/dist/store-assets/logos/StoreLogo_300x300.png)
- **Square / Box Art (1080×1080):** [BoxArt_1080x1080.png](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/dist/store-assets/logos/BoxArt_1080x1080.png)
- **2:3 Poster Art (720×1080):** [PosterArt_720x1080.png](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/dist/store-assets/logos/PosterArt_720x1080.png)
- **16:9 Hero Art / Promotional Artwork (1920×1080):** [HeroArt_1920x1080.png](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/dist/store-assets/logos/HeroArt_1920x1080.png)

### Masaüstü Ekran Görüntüleri (Desktop Screenshots — 1920×1080)
- **English (`en-US` ve diğer küresel diller için):** `dist\store-assets\screenshots-en\`
  1. [01_taskbar_countdown.png](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/dist/store-assets/screenshots-en/01_taskbar_countdown.png)
  2. [02_fluent_flyout_qibla.png](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/dist/store-assets/screenshots-en/02_fluent_flyout_qibla.png)
  3. [03_ramadan_imsakiye.png](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/dist/store-assets/screenshots-en/03_ramadan_imsakiye.png)
  4. [04_settings_features.png](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/dist/store-assets/screenshots-en/04_settings_features.png)
- **Türkçe (`tr-TR` vitrini için):** `dist\store-assets\screenshots-tr\`
  1. [01_gorev_cubugu_sayac.png](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/dist/store-assets/screenshots-tr/01_gorev_cubugu_sayac.png)
  2. [02_acilir_kart_kible.png](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/dist/store-assets/screenshots-tr/02_acilir_kart_kible.png)
  3. [03_ramazan_imsakiyesi.png](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/dist/store-assets/screenshots-tr/03_ramazan_imsakiyesi.png)
  4. [04_ayarlar_ozellikler.png](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/dist/store-assets/screenshots-tr/04_ayarlar_ozellikler.png)

*(Gerekirse aynı klasörlerden görseller yeniden üretilebilir: `powershell -ExecutionPolicy Bypass -File scripts\generate-store-kit.ps1`)*

---

## 6. Submission options & Notes for certification (Sertifikasyon Notu)
Paket `runFullTrust` kullandığı için Microsoft inceleme ekibinin sorusuz onaylaması adına **Notes for certification** kutusuna şunu aynen yapıştırın:

```text
This pure native C++20 Win32 desktop utility displays a lightweight prayer-time countdown band anchored adjacent to the Windows taskbar (Shell_TrayWnd / Shell_NotifyIconW) alongside an on-demand Fluent flyout, monthly/Ramadan calendar (Imsakiye), and settings window using ~3 MB RAM and 0% idle CPU. The runFullTrust capability is required solely for taskbar indicator window positioning (SetWindowPos / SetWinEventHook), WinHTTP monthly prayer-schedule caching (optional, full offline fallback supported), and optional Windows Startup registration. All core prayer features operate offline without accounts or telemetry. No special test credentials are required to test the application or its Microsoft Store IAP add-ons (pro_annual / pro_lifetime).
```

Tüm yeşil tikler tamamlandığında sağ üstten **"Submit to the Store"** butonuna basın!
