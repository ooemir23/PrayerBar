# PrayerBar — Çok Kanallı Tanıtım ve Pazarlama Kiti

Bu doküman; **PrayerBar v2.1** uygulamasını Reddit, Türkiye teknoloji forumları, global yazılım dizinleri ve kısa video platformlarında **spam filtresine takılmadan**, her topluluğun kültürüne uygun dille tanıtmak için hazırlanmış hazır metinleri içerir.

**Temel Bağlantılar:**
- **Web Sitesi:** https://ooemir23.github.io/PrayerBar/
- **Microsoft Store:** https://apps.microsoft.com/detail/9MSQBZTPGWX6
- **GitHub:** https://github.com/ooemir23/PrayerBar
- **Gizlilik Politikası:** https://ooemir23.github.io/PrayerBar/privacy.html

---

## 1. Reddit Paylaşım Taslakları

> [!IMPORTANT]
> **Reddit Altın Kuralları:**
> - Aynı gün içinde 1 veya en fazla 2 subreddit'e paylaşım yapın; aksi hâlde Reddit anti-spam filtresi hesabınızı shadowbanlayabilir.
> - Gönderilere mutlaka **10–15 saniyelik bir GIF veya temiz ekran görüntüsü** ekleyin (görselsiz link/metin gönderileri çok daha az tıklanır).
> - Başlığa "BUY / DOWNLOAD NOW" gibi reklam ifadeleri koymayın; geliştirici hikâyesi ve teknik/pratik fayda odaklı yazın.

### A) `r/islam` ve `r/MuslimLounge` (İngilizce — Ana Kitle)
**Önerilen Zaman:** Cuma günü (TSİ 14:00–18:00 veya ABD sabahı)
**Başlık:**
`I built a super lightweight Windows 11 taskbar prayer times app (~3 MB RAM, no ads, 12 languages) — PrayerBar`

**Metin:**
```markdown
Assalamu alaikum everyone,

I got tired of desktop prayer apps that either look dated or run Electron in the background eating 200+ MB of RAM just to show a countdown. So I built **PrayerBar** from scratch in pure native C++20 + Win32.

What it does:
- **Sits natively on your Windows 10/11 taskbar** (near the tray, far right, center, or left) with 9 display styles and 4 flyout themes.
- **Ultra-lightweight:** Uses ~3–5 MB RAM and 0% idle CPU (wakes up on a single minute-aligned timer).
- **23 Calculation Methods + Auto-detect:** Diyanet, ISNA, MWL, Umm al-Qura, Egyptian, Karachi, Singapore, France UOIF, etc., plus Standard/Hanafi Asr.
- **12 Languages:** English, Arabic, Turkish, Urdu, Indonesian, Malay, Bengali, Persian, German, French, Russian, Spanish (with RTL support).
- **Ramadan & Daily Tools:** Automatic Ramadan Iftar/Suhoor countdown, monthly Imsakiye (CSV/HTML export), Qibla bearing, Hijri date, optional Duha/Tahajjud times, Friday & Islamic holiday reminders, and custom or built-in Adhan sounds.
- **Privacy-first:** No ads, no telemetry, works 100% offline with local astronomical calculations if you disable monthly sync.

Core prayer times, notifications, Adhan sounds, and Imsakiye are **free forever**. There is a 14-day free Pro trial for extra taskbar/flyout themes and custom sound files ($4.99/yr or $9.99 lifetime via Microsoft Store).

Links:
- Microsoft Store: https://apps.microsoft.com/detail/9MSQBZTPGWX6
- GitHub (source code): https://github.com/ooemir23/PrayerBar
- Website: https://ooemir23.github.io/PrayerBar/

Would love any feedback or feature requests!
```

---

### B) `r/Windows11` ve `r/Windows10` (İngilizce — Taskbar & Performans Odaklı)
**Başlık:**
`I made a native C++20 Win32 taskbar countdown widget (~3 MB RAM, 0% idle CPU) — no .NET or Electron`

**Metin:**
```markdown
I wanted a clean, native-feeling prayer times countdown directly on the Windows 11 taskbar without spinning up an Electron or WebView2 process.

Built **PrayerBar** using pure C++20, Win32, GDI+, and WinHTTP:
- **Footprint:** ~3–5 MB working set, 0.00% idle CPU, trims working set (`SetProcessWorkingSetSize`) when the flyout closes.
- **Taskbar integration:** Dockable near the system tray, far right, center, or left with pixel-level offset (`-500..+500 px`), follows Windows Light/Dark mode and system accent color.
- **Power & fullscreen aware:** Handles `WM_POWERBROADCAST` sleep/wake cleanly and stays quiet during fullscreen apps/games.
- **Global hotkey:** `Win+Alt+P` to toggle the flyout anywhere.

It's open source on GitHub and also packaged as a ~3–4 MB MSIX on the Microsoft Store:
- GitHub: https://github.com/ooemir23/PrayerBar
- Store: https://apps.microsoft.com/detail/9MSQBZTPGWX6

Happy to answer any Win32 / taskbar positioning / GDI+ questions!
```

---

### C) `r/software` ve `r/opensource` (İngilizce — Hafif Yazılım Alternatifi)
**Başlık:**
`PrayerBar — A pure native C++20 Windows taskbar prayer times widget (~3 MB RAM, 0% idle CPU)`

**Metin:**
```markdown
Sharing **PrayerBar**, a lightweight Windows 10/11 taskbar & system tray utility for prayer times, written in pure C++20 + Win32 without any heavy framework dependencies.

Highlights:
- ~3–5 MB RAM, 0% idle CPU, static single-binary build
- 23 calculation methods + offline astronomical fallback + optional Aladhan monthly sync
- 12 UI languages with full RTL support
- Monthly Imsakiye export (CSV / HTML)
- No telemetry, no ads, local JSON storage only

Source & build scripts (LLVM-MinGW + PowerShell): https://github.com/ooemir23/PrayerBar
Website: https://ooemir23.github.io/PrayerBar/
```

---

### D) `r/cpp` (İngilizce — Teknik Geliştirici Topluluğu)
**Başlık:**
`Show /r/cpp: PrayerBar — Pure C++20 + Win32/GDI+ Windows 11 taskbar band & flyout (~3 MB RAM)`

**Metin:**
```markdown
I built a native Windows 10/11 taskbar widget and flyout application in pure C++20 without .NET, WPF, or WebView2, compiled with LLVM-MinGW (`x86_64-w64-mingw32-clang++`).

A few technical details that might interest Win32 devs here:
- **Zero-idle-CPU timer:** Aligns a single Win32 `SetTimer` to minute boundaries and listens to `SetWinEventHook` + `WM_POWERBROADCAST` for taskbar geometry and sleep/wake changes.
- **Memory trimming:** Destroys the flyout HWND on close and calls `SetProcessWorkingSetSize` to keep working set around 3–5 MB.
- **Self-contained:** Hand-rolled lightweight JSON parser, GDI+ flat API wrapper, WinHTTP client, WinMM audio playback, and native `StoreContext` WinRT interop without pulling in heavy SDKs.
- **Security & CI:** Built with ASLR, DEP, High-Entropy VA, and automated C++20 unit tests + `--diag-smoke` headless render checks on GitHub Actions.

Repo: https://github.com/ooemir23/PrayerBar
```

---

### E) `r/Turkey` (Türkçe — Yerel Topluluk)
**Başlık:**
`Windows 11 görev çubuğu için saf C++ ile yazdığım ~3 MB RAM tüketen namaz vakti uygulaması: PrayerBar`

**Metin:**
```markdown
Merhaba,

Windows'ta görev çubuğunun sağında (veya orta/solunda) sade bir geri sayım gösteren, arka planda yüzlerce MB RAM yemeyen bir namaz vakti uygulaması bulamadığım için **PrayerBar**'ı sıfırdan saf C++20 + Win32 ile yazdım.

Öne çıkan özellikler:
- **~3–5 MB RAM, %0 boşta CPU:** .NET/Electron yok, sistem tepsisi ve görev çubuğuyla yerel (native) bütünleşiyor.
- **Diyanet uyumlu & 23 metot:** Diyanet dahil 23 hesaplama metodu, aylık imsakiye (CSV/HTML dışa aktarım), Ramazan'da otomatik İftar/Sahur modu.
- **Özelleştirilebilir:** 4 görev çubuğu konumu, 9 görünüm stili, 4 açılır panel tasarımı, her vakit için ayrı ezan/zil ve ön hatırlatma.
- **Gizlilik:** Reklam ve telemetri yok; internet eşitlemesini kapatırsanız tamamen çevrimdışı çalışıyor.

Temel tüm özellikler (vakitler, bildirimler, ezan sesleri, imsakiye) sonsuza kadar ücretsiz. Ekstra tema/özel ses gibi kozmetik özellikler için 14 gün Pro deneme ve Microsoft Store üzerinden destek seçeneği bulunuyor.

- GitHub: https://github.com/ooemir23/PrayerBar
- Microsoft Store: https://apps.microsoft.com/detail/9MSQBZTPGWX6
```

---

## 2. Türkiye Teknoloji Forumları & Sözlükler

### Technopat Sosyal / DonanımHaber (*Kullanıcı İncelemeleri / Yazılım*)
**Başlık:** `PrayerBar: Windows 10/11 Görev Çubuğu İçin Saf C++20 Namaz Vakti Uygulaması (~3 MB RAM)`

**Metin:**
```markdown
Windows 10 ve 11 görev çubuğunda çalışan, sistem kaynaklarını neredeyse hiç tüketmeyen açık kaynaklı **PrayerBar** projesini paylaşmak istiyorum.

**Neden Saf C++20?**
Piyasadaki çoğu masaüstü vakit uygulaması Electron, .NET veya web tabanlı arayüzler kullandığı için 150–400 MB arası RAM tüketiyor. PrayerBar doğrudan Win32 API ve GDI+ ile yazıldığı için:
- **RAM Tüketimi:** ~3–5 MB
- **Boşta CPU:** %0.000 (yalnızca dakika değişiminde uyanan tek zamanlayıcı)
- **Oyun / Tam Ekran Dostu:** Tam ekran uygulamalarda ve oyunlarda bildirimleri otomatik susturma

**Başlıca Özellikler:**
- Görev çubuğunda 4 farklı konum (sistem tepsisi yanı, en sağ, orta, en sol) ve ince piksel ayarı
- 9 farklı görev çubuğu stili ve 4 açılır kart teması
- Diyanet dahil 23 dünya hesaplama metodu + Hanefi/Standart ikindi seçimi
- Her vakit için ayrı "vaktinde" ve "5–60 dk önce" hatırlatıcı + dahili kısa/uzun ezan sesleri
- Ramazan İftar/Sahur geri sayımı ve aylık imsakiye (CSV/HTML dışa aktarım)
- `Win+Alt+P` kısayolu, seyahat modu (saat dilimi değişince uyarı) ve 12 dil desteği

**İndirme ve Kaynak Kod:**
- Microsoft Store: https://apps.microsoft.com/detail/9MSQBZTPGWX6
- GitHub: https://github.com/ooemir23/PrayerBar
- Web Sitesi: https://ooemir23.github.io/PrayerBar/
```

---

## 3. Global Dizinler & Lansman Platformları

### A) Product Hunt
* **Name:** `PrayerBar`
* **Tagline (60 karakter altı):** `Ultra-light native Windows 11 taskbar prayer times (~3 MB)`
* **Topics:** `Windows`, `Productivity`, `Open Source`, `Developer Tools`
* **Maker's First Comment:**
```markdown
Hey Product Hunt! 👋

I built **PrayerBar** because I wanted a Windows 11 taskbar prayer timer that feels like a built-in OS widget rather than a heavy browser-wrapped app.

Written in pure C++20 + Win32/GDI+:
⚡ ~3–5 MB RAM, 0% idle CPU
🎨 4 taskbar dock positions, 9 styles, 4 flyout themes (Light/Dark + Accent sync)
🌍 12 languages & 23 global calculation methods
🌙 Ramadan Iftar/Suhoor countdown & Monthly Imsakiye export
🔔 Per-prayer notifications, Adhan audio, and fullscreen quiet mode

Core features are free forever, with a 14-day Pro trial for extra themes and custom sounds. Source code is on GitHub and distributed via the Microsoft Store. Would love your feedback!
```

### B) Hacker News (`Show HN`)
* **Title:** `Show HN: PrayerBar – A ~3 MB RAM native C++20 Windows 11 taskbar prayer timer`
* **URL:** `https://github.com/ooemir23/PrayerBar`
* **İlk Yorum:** `r/cpp` metnindeki teknik özet (Win32 `SetTimer` dakika hizalaması, `SetProcessWorkingSetSize`, GDI+ flat API, LLVM-MinGW statik derleme ve ASLR/DEP/High-Entropy VA güvenlik kontrolleri).

### C) AlternativeTo
* **Uygulama Adı:** `PrayerBar`
* **Kısa Açıklama:** `Ultra-lightweight (~3–5 MB RAM, 0% idle CPU) native C++20 Windows 10/11 taskbar countdown, system tray badge, and monthly Imsakiye app in 12 languages.`
* **Alternatif olarak ekleneceği sayfalar:** *Salaat Time*, *Athan (Azan)*, *Islamic Prayer Times*.

---

## 4. Kısa Video (YouTube Shorts / Reels / TikTok) & X/Twitter

### 20 Saniyelik Video Senaryosu
1. **0–3 sn (Hook):** Görev Yöneticisi ekranı açılır → *"Elektron uygulamalar 300 MB RAM yerken, bu Windows 11 namaz vakti widget'ı sadece **3 MB RAM** kullanıyor."*
2. **3–10 sn (Demo):** Görev çubuğundaki geri sayıma tıklanır → 4 farklı açılır panel teması ve sağ/orta/sol konum değişimi hızlıca gösterilir.
3. **10–16 sn (Özellikler):** Aylık İmsakiye, Ramazan İftar geri sayımı ve vakit bazlı ezan/bildirim ayarları gösterilir.
4. **16–20 sn (CTA):** *"Microsoft Store'da PrayerBar yazarak ücretsiz deneyebilirsiniz, kaynak kodu GitHub'da açık."*

### X / Twitter Kısa Paylaşım (İngilizce)
```text
Most desktop prayer apps run on Electron and eat 200+ MB of RAM.

So I built PrayerBar in pure C++20 + Win32:
• ~3–5 MB RAM, 0% idle CPU
• Native Windows 11 taskbar countdown & 4 flyout themes
• 12 languages, 23 calculation methods, Monthly Imsakiye

Microsoft Store: https://apps.microsoft.com/detail/9MSQBZTPGWX6
GitHub: https://github.com/ooemir23/PrayerBar
```

---

## 5. Önerilen Yayın Takvimi (Spam Riskini Önlemek İçin)

| Gün | Platform | Odak |
| :--- | :--- | :--- |
| **1. Gün (Salı)** | `r/Windows11` + GitHub Topics güncellemesi | Native C++20 & ~3 MB RAM performansı |
| **2. Gün (Cuma)** | `r/islam` + `r/Turkey` | Günlük kullanım, Ramazan/İmsakiye, ücretsiz çekirdek özellikler |
| **3. Gün** | Technopat Sosyal + DonanımHaber | Detaylı Türkçe inceleme ve indirme linkleri |
| **4. Gün** | AlternativeTo + `r/software` | Alternatif arayanlar için kalıcı SEO |
| **5. Gün** | Product Hunt + X/Twitter + `Show HN` | Küresel lansman günü (GIF/ekran görüntüleriyle) |
| **6. Gün** | `r/cpp` + `r/MuslimLounge` | Teknik detaylar ve topluluk geri bildirimi |
