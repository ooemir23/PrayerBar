# Microsoft Store Yayınlama ve Uygulama İçi Satın Alma Rehberi (PrayerBar v2.0)

Bu proje, harici web sitesi veya ödeme sağlayıcısı olmadan **%100 Microsoft Store üzerinden** satış, abonelik yönetimi, iade ve doğrudan **TR IBAN'a ödeme** alacak şekilde hazırlanmıştır. Tek yönetim (admin) paneliniz **Microsoft Partner Center** (`partner.microsoft.com`) hesabıdır.

---

## 1. Partner Center Hesabı, IBAN ve Uygulama Kaydı
1. [Microsoft Partner Center](https://partner.microsoft.com/dashboard) üzerinden bireysel geliştirici hesabı açın.
2. **Account settings → Payout and tax profile** bölümünden Türkiye banka hesabınızın **IBAN + SWIFT/BIC** bilgisini ve vergi profilinizi tanımlayın. Global KDV/VAT kesintilerini Microsoft otomatik yapar ve biriken net geliriniz doğrudan IBAN'ınıza yatar.
3. **Apps and games → New product → MSIX or PWA app** üzerinden uygulamanız için benzersiz bir isim rezerve edin (ör. `PrayerBar - Prayer Times`).
4. **Product management → Product Identity** sayfasından şu değerleri kopyalayın:
   - `Package/Identity/Name` → [packaging/store.config.json](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/packaging/store.config.json) (`IdentityName`)
   - `Package/Identity/Publisher` → (`Publisher`, örn. `CN=XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX`)
   - `Package/Properties/PublisherDisplayName` → (`PublisherDisplayName`)
   - **Store Product ID** (`9N...` ile başlayan 12 haneli kod)

---

## 2. İki Microsoft Store Eklentisi (Add-On) Oluşturma
Uygulamanın içindeki **"Yıllık Al"** ve **"Ömür Boyu Al"** butonlarının doğrudan yerel Windows 11 Microsoft Store ödeme penceresini açması için Partner Center'da ürününüzün altında **Add-ons → New add-on** menüsünden iki eklenti tanımlayın:

1. **Yıllık Abonelik (`Subscription`):**
   - **Product ID (kendi vereceğiniz takma ad):** `pro_annual` *(kod otomatik tanır; `InAppOfferToken`)*
   - **Title:** `PrayerBar Pro — Annual`
   - **Billing period:** `1 year` (`$4.99` baz fiyat önerilir — Türkiye vb. pazarlara otomatik yerel fiyat)
   - Oluştuktan sonra Partner Center'ın verdiği `9N...` **Store ID** kodunu kopyalayın.
2. **Ömür Boyu Lisans (`Durable — Product lifetime`):**
   - **Product ID (kendi vereceğiniz takma ad):** `pro_lifetime`
   - **Title:** `PrayerBar Pro — Lifetime`
   - **Product lifetime:** `Forever` (`$9.99` baz fiyat önerilir)
   - Oluştuktan sonra Partner Center'ın verdiği `9N...` **Store ID** kodunu kopyalayın.

---

## 3. Store ID Değerlerini Koda ve Yapılandırmaya Yazma
1. [src/core/commercial_config.h](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/src/core/commercial_config.h) dosyasını açıp gerçek Store ID değerlerinizi girin:
   - `kStoreAppProductId` → Uygulama Store ID (`9N...`)
   - `kStoreAddonAnnualId` → Yıllık abonelik eklentisi Store ID (`9N...`)
   - `kStoreAddonLifetimeId` → Ömür boyu eklenti Store ID (`9N...`)
   - `kMicrosoftStorePdpUri` ve `kMicrosoftStoreReviewUri` → Sonundaki `9NBLGGH00000` yerine uygulamanızın Store ID'si.
2. [packaging/store.config.json](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/packaging/store.config.json) içinde `IdentityName`, `Publisher`, `PublisherDisplayName` ve `Commercial` bölümündeki `StoreProductId` değerlerini güncelleyin (Version: `2.0.0.0`).

---

## 4. Ücretsiz (0 TL) Gizlilik Politikası Bağlantısı (Hazır ve Yayında!)
GitHub Pages üzerinden ücretsiz, SSL sertifikalı (HTTPS) gizlilik politikası ve destek siteniz yayına alınmıştır:
- **Privacy Policy URL (Partner Center için zorunlu):** `https://ooemir23.github.io/VakitCubugu/privacy.html`
- **Support / Website URL:** `https://ooemir23.github.io/VakitCubugu/`
- **Terms of Service URL:** `https://ooemir23.github.io/VakitCubugu/terms.html`

---

## 5. MSIX Store Paketini Üretme ve Yayınlama
PowerShell'de repo kök klasöründen çalıştırın:

```powershell
# Native C++20 ikilisini derler, 11 birim test paketini + smoke testini çalıştırır,
# 12 dil (en-US, tr-TR, ar-SA, de-DE, fr-FR, id-ID, ur-PK, bn-BD, ms-MY, fa-IR, ru-RU, es-ES) bildirimli Store .msix üretir
powershell -ExecutionPolicy Bypass -File scripts\build-release.ps1
```

Sadece MSIX üretmek veya kendi bilgisayarınızda test sertifikasıyla yükleyip denemek için:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-msix.ps1
powershell -ExecutionPolicy Bypass -File scripts\build-msix.ps1 -CreateTestCert
```

Oluşan `dist\release\VakitCubugu_2.0.0.0_x64_Store.msix` paketini Partner Center → **Packages** alanına yükleyin.
- **6 Dilli Mağaza Açıklamaları (TR/EN/AR/DE/FR/ID):** [docs/STORE_LISTINGS_GLOBAL.md](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/docs/STORE_LISTINGS_GLOBAL.md)
- **Notes for certification** alanına eklenecek açıklama *(manifest `runFullTrust` kullandığı için)*:

> This native C++20 desktop utility displays live daily prayer countdowns on the Windows 11 taskbar and system tray using minimal memory (~3 MB private RAM, 0% idle CPU). The `runFullTrust` capability is required solely to anchor the lightweight layered indicator adjacent to the taskbar tray (`Shell_TrayWnd` / `Shell_NotifyIconW`) and show the user-triggered Fluent flyout/settings UI.

---

## 6. Satış, Abone, İade ve IBAN Takibi (Admin Paneliniz)
Uygulama yayınlandıktan sonra tüm verileri **Microsoft Partner Center → Analyze** menüsünden canlı görürsünüz:
- **Acquisitions & Add-ons:** Ücretsiz indirme sayısı, Yıllık ve Ömür Boyu Pro satın alma sayıları
- **Add-on Subscriptions:** Aktif abone sayısı, yenilemeler ve iptaller
- **Orders / Refunds:** Ülke bazlı brüt satış, Microsoft mağaza komisyonu sonrası net tutar ve Microsoft tarafından yapılan iadeler (iade edilen eklentiyi uygulama başlangıçta `StoreContext::GetAppLicenseAsync` ile otomatik kapatır)
- **Payout Summary:** Tanımlı TR IBAN hesabınıza yapılan aylık hak ediş transferleri
