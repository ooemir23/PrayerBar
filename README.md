# PrayerBar (Saf Native C++20 + Win32)

Windows 10/11 görev çubuğunun istediğiniz bölümünde (sistem tepsisi yanı, en sağ / saatin sağ köşesi, orta veya sol) ve sistem tepsisinde çalışan, **ultra düşük RAM (~3–5 MB) ve sıfır boşta CPU (%0.000)** tüketimli modern namaz vakitleri uygulaması.

## Mimari Avantajlar
- **Saf Native C++20 + Win32 + GDI+ + WinHTTP + WinMM:** .NET / WPF / Direct3D yükü yoktur; açılışta birkaç milisaniyede açılır ve eski laptoplarda dahi takılmadan, pil ömrünü koruyarak çalışır.
- **Sıfır Boşta CPU & Uyku/Uyanma Uyumu:** Dakika değişimine hizalı tek Win32 `SetTimer` ve olay güdümlü `SetWinEventHook` + `WM_POWERBROADCAST` desteği.
- **Sağ Kenara Yaslı Kompakt Açılır Panel:** Kapatıldığında pencere tutamacı yok edilir ve bellek `SetProcessWorkingSetSize` ile sisteme iade edilir.
- **4 Görev Çubuğu Konumu & 9 Stil:** Sağ (tepsi yanı), En Sağ (opak arka planlı), Orta ve En Sol + `-500..+500 px` ince kaydırma.
- **Her Vakit İçin Ayrı Bildirim & Ücretsiz Ezan Sesleri:** 6 vaktin her biri için ayrı *Vaktinde bildir*, *Önce hatırlat (5–60 dk)* ve *Ses/Ezan* seçimi (Public Domain kısa/uzun ezan MP3, yumuşak çan, huzurlu zil veya özel `.mp3`/`.wav`).
- **23 Dünya Hesaplama Metodu + Otomatik Konum:** Diyanet, MWL, ISNA, Ümmü'l-Kura, Mısır, Karaçi, UOIF Fransa vb. tüm Aladhan metotları, Standart/Hanefi İkindi mezhebi ve IP/ülke bazlı otomatik konum ve metot seçimi.
- **Microsoft Store Hazır:** ASLR / DEP / High-Entropy VA uyumlu statik tek `.exe` ve ~3–4 MB kompakt Store `.msix` paketi.

## v2.0 Global & Ticari Özellikler
- **12 Dilli Global Arayüz (`Auto` OS Dili Algılama):** English, Türkçe, العربية, Deutsch, Français, Bahasa Indonesia, اردو, বাংলা, Bahasa Melayu, فارسی, Русский, Español.
- **23 Dünya Hesaplama Yöntemi:** Diyanet, ISNA, MWL, Umm al-Qura, Egyptian, Karachi, Singapore, France UOIF, Russia, vb.
- **Ramazan İftar & Sahur Geri Sayımı + Aylık İmsakiye:** İmsakiye penceresi, CSV/HTML dışa aktarım ve otomatik Ramazan Iftar/Sahur modu.
- **Etik Hibrit Freemium & 14 Gün Ücretsiz Pro Deneme:** Çekirdek vakitler, günlük bildirimler, dahili ezan sesleri ve İmsakiye görüntüleme daima ücretsizdir. Pro (`$4.99/yıl` veya `$9.99 ömür boyu`) 14 gün tam deneme sunar; satın alma yalnızca Microsoft Store eklentileriyle yapılır ve lisans her açılışta Store'dan doğrulanır. Pro tüm görev çubuğu stillerini, açılır kart temalarını, özel sesleri ve yedeklemeyi açar.

- **Aç/Kapa Özellikler Sekmesi:** Cuma ve dini gün hatırlatmaları, seyahat modu (saat dilimi değişince uyarı), Ertele düğmeli Windows bildirimleri, Kuşluk / Gece yarısı / Teheccüd vakitleri, haftalık özetli namaz takibi ve Win+Alt+P kısayolu. Hepsi tek tek açılıp kapatılabilir; hiçbiri ek zamanlayıcı veya arka plan işi eklemez.
- **Sabah Ezanı Seçimi:** İmsak vakti için ayrı ses seçilebilir.

## Derleme, Test & Paketleme Komutları

```powershell
# Derle + C++20 birim testlerini çalıştır + PE güvenlik kontrolü yap
powershell -ExecutionPolicy Bypass -File scripts\build.ps1

# Çalışma zamanı, bellek ve lisans doğrulama testi
.\build\VakitCubugu.exe --diag-smoke

# Açılır pencereyi istenen dilde BMP olarak kaydet (mağaza görselleri için; --lang 1-12)
.\build\VakitCubugu.exe --diag-smoke --lang 2 --flyout-preview C:\Temp\en

# Çalışan uygulamada ayarları belirli sekmede aç (0-4)
.\build\VakitCubugu.exe --settings 3

# Global Store (.msix) + Portable (.zip) + SHA256SUMS.txt yayın paketlerini dist\release\ altına üret
powershell -ExecutionPolicy Bypass -File scripts\build-release.ps1
```

## Dokümantasyon ve Web Varlıkları
- **Mağaza Yayınlama Rehberi:** [docs/MICROSOFT_STORE_GUIDE.md](docs/MICROSOFT_STORE_GUIDE.md)
- **6 Dilli Global Mağaza Metinleri (ASO):** [docs/STORE_LISTINGS_GLOBAL.md](docs/STORE_LISTINGS_GLOBAL.md)
- **Global Lansman Adımları:** [docs/GLOBAL_LAUNCH_CHECKLIST.md](docs/GLOBAL_LAUNCH_CHECKLIST.md)
- **Global Ürün Web Sitesi & Gizlilik Politikası:** [website/index.html](website/index.html), [website/privacy.html](website/privacy.html), [website/terms.html](website/terms.html)
