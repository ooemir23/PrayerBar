# Microsoft Store Lansman Kontrol Listesi (PrayerBar v2.1)

Bu liste, uygulamayı **yalnızca Microsoft Store** üzerinden yayınlayıp tüm dünya gelirlerini tek **Partner Center** panelinden yöneterek doğrudan **TR IBAN** hesabınıza almak için gereken son adımları özetler.

---

## Adım 1 — Partner Center Kurulumu ve Banka / IBAN Tanımlama
1. [partner.microsoft.com](https://partner.microsoft.com/dashboard) hesabınızı açın.
2. **Account settings → Payout and tax profile** menüsünden **TR IBAN + SWIFT** banka bilginizi ve vergi formunuzu (W-8BEN bireysel) tamamlayın.
3. Yeni uygulama oluşturun (`PrayerBar - Prayer Times`) ve **Product Identity** değerlerini [packaging/store.config.json](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/packaging/store.config.json) içine yazın.

## Adım 2 — 2 Store Eklentisi (Pro Annual & Pro Lifetime)
Partner Center içinde uygulamanızın **Add-ons** bölümünden iki eklenti açıp `9N...` Store ID'lerini [src/core/commercial_config.h](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/src/core/commercial_config.h) dosyasına girin:
1. `pro_annual` (Subscription, 1 Year, `$4.99` baz fiyat) → `kStoreAddonAnnualId`
2. `pro_lifetime` (Durable, Forever, `$9.99` baz fiyat) → `kStoreAddonLifetimeId`
3. Ana uygulama Store ID (`9N...`) → `kStoreAppProductId`, `kMicrosoftStorePdpUri`, `kMicrosoftStoreReviewUri`

## Adım 3 — Gizlilik Politikası Linki (TAMAMLANDI & YAYINDA!)
`https://prayerbar.ooemir.com` üzerinden HTTPS gizlilik politikası ve destek siteniz aktif edildi:
- **Privacy Policy URL (Partner Center için zorunlu):** `https://prayerbar.ooemir.com/privacy`
- **Support / Website URL:** `https://prayerbar.ooemir.com/`
- Bu bağlantıyı Partner Center'da her dilin **Privacy policy URL** alanına ekleyin.

## Adım 4 — Paketleme ve Mağazaya Yükleme
Store ID'leri kaydettikten sonra tek komutla Store paketini üretin:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-release.ps1
```

Çıktı dosyası: `dist\release\PrayerBar_2.1.0.0_x64_Store.msix`
- 12 dildeki mağaza başlık ve açıklamalarını [docs/STORE_LISTINGS_GLOBAL.md](file:///c:/Users/ooemi/Desktop/Projeler/EzanNamazVak/docs/STORE_LISTINGS_GLOBAL.md) dosyasından kopyalayın.
- 1920×1080 ekran görüntülerini (Görev Çubuğu + Açılır Kart + Ayarlar + İmsakiye) ekleyip **Submit to the Store** deyin.

## Adım 5 — Yayın Sonrası Yönetim
- **Admin Paneli:** `partner.microsoft.com` → **Analyze** (indirmeler, aktif aboneler, ülke kırılımı, iadeler, aylık IBAN ödemeleri).
- Kullanıcı satın aldıktan veya iade ettikten sonra uygulama içindeki native Windows 11 `StoreContext` API'si lisansı başlangıçta ve **"Satın Almaları Geri Yükle"** butonuyla otomatik senkronize eder.
