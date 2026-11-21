#include "i18n.h"
#include "prayer_calc.h"
#include "utf_util.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace vakit {

namespace {

size_t LangIndex(AppLanguage lang) {
    AppLanguage eff = ResolveEffectiveLanguage(lang);
    int idx = static_cast<int>(eff) - 1;
    if (idx < 0 || idx > 5) return 0;
    return static_cast<size_t>(idx);
}

struct LangRow {
    const wchar_t* tr;
    const wchar_t* en;
    const wchar_t* ar;
    const wchar_t* de;
    const wchar_t* fr;
    const wchar_t* id;

    const wchar_t* At(size_t i) const {
        switch (i) {
            case 0: return tr;
            case 1: return en;
            case 2: return ar;
            case 3: return de;
            case 4: return fr;
            case 5: return id;
        }
        return tr;
    }
};

constexpr LangRow kUiStrings[] = {
    // ToggleFlyout
    {L"Vakitleri Göster / Gizle", L"Show / Hide Prayer Times", L"إظهار / إخفاء المواقيت", L"Gebetszeiten ein-/ausblenden", L"Afficher / Masquer les horaires", L"Tampilkan / Sembunyikan Jadwal"},
    // OpenImsakiye
    {L"Aylık İmsakiye...", L"Monthly Calendar (Imsakiye)...", L"الإمساكية الشهرية...", L"Monatskalender (Imsakiye)...", L"Calendrier mensuel...", L"Jadwal Imsakiyah Bulanan..."},
    // TaskbarStyleMenu
    {L"Görev Çubuğu Görünümü", L"Taskbar Display Style", L"نمط عرض شريط المهام", L"Taskleisten-Darstellung", L"Style dans la barre des tâches", L"Gaya Tampilan Taskbar"},
    // FlyoutDesignMenu
    {L"Açılır Pencere Tasarımı", L"Flyout Design", L"تصميم النافذة المنبثقة", L"Flyout-Design", L"Design du panneau", L"Desain Panel Popup"},
    // ShowInTrayMenu
    {L"Sistem Tepsisinde Göster", L"Show in System Tray", L"إظهار في علبة النظام", L"Im Infobereich anzeigen", L"Afficher dans la zone de notification", L"Tampilkan di System Tray"},
    // ShowInTaskbarMenu
    {L"Görev Çubuğunda Göster", L"Show on Taskbar", L"إظهار في شريط المهام", L"In der Taskleiste anzeigen", L"Afficher dans la barre des tâches", L"Tampilkan di Taskbar"},
    // RefreshNow
    {L"Vakitleri Şimdi Güncelle", L"Refresh Prayer Times", L"تحديث المواقيت الآن", L"Zeiten jetzt aktualisieren", L"Actualiser les horaires", L"Perbarui Jadwal Sekarang"},
    // SettingsEllipsis
    {L"Ayarlar...", L"Settings...", L"الإعدادات...", L"Einstellungen...", L"Paramètres...", L"Pengaturan..."},
    // ExitApp
    {L"Çıkış", L"Exit", L"خروج", L"Beenden", L"Quitter", L"Keluar"},
    // StopSoundMenu
    {L"■ Ezanı / Sesi Durdur", L"■ Stop Adhan / Audio", L"■ إيقاف الأذان / الصوت", L"■ Adhan / Ton stoppen", L"■ Arrêter l'Adhan / le son", L"■ Hentikan Adzan / Suara"},

    // BadgeNext
    {L"Sıradaki", L"Next", L"التالي", L"Nächstes", L"Suivant", L"Berikutnya"},
    // BadgeCurrent
    {L"Şu an", L"Now", L"الآن", L"Jetzt", L"Actuel", L"Saat ini"},
    // BadgeJumuah
    {L"Cuma", L"Jumu'ah", L"الجمعة", L"Dschuma", L"Joumou'a", L"Jumat"},
    // PrayerTimeHeader
    {L"Vakit saati", L"Prayer time", L"وقت الصلاة", L"Gebetszeit", L"Heure de prière", L"Waktu sholat"},
    // KerahatAfterSunrise
    {L"Kerahat · İşrak öncesi", L"Makruh · After sunrise", L"وقت كراهة · بعد الشروق", L"Karaha · Nach Sonnenaufgang", L"Karaha · Après le lever", L"Waktu Terlarang · Terbit"},
    // KerahatBeforeDhuhr
    {L"Kerahat · İstiva öncesi", L"Makruh · Zenith before Dhuhr", L"وقت كراهة · قبل الزوال", L"Karaha · Vor Dhuhr", L"Karaha · Avant Dhuhr", L"Waktu Terlarang · Zawal"},
    // KerahatBeforeMaghrib
    {L"Kerahat · İsfirar öncesi", L"Makruh · Before Maghrib", L"وقت كراهة · قبل الغروب", L"Karaha · Vor Maghrib", L"Karaha · Avant Maghrib", L"Waktu Terlarang · Jelang Maghrib"},
    // SourcePrefix
    {L"Kaynak: ", L"Source: ", L"المصدر: ", L"Quelle: ", L"Source : ", L"Sumber: "},
    // SourceLocalCalc
    {L"Yerel Hesaplama", L"Offline Calculation", L"حساب محلي", L"Offline-Berechnung", L"Calcul hors ligne", L"Perhitungan Lokal"},
    // TipStopAudio
    {L"Sesi Durdur", L"Stop Audio", L"إيقاف الصوت", L"Ton stoppen", L"Arrêter le son", L"Hentikan Suara"},
    // TipImsakiye
    {L"Aylık İmsakiye", L"Monthly Calendar", L"الإمساكية الشهرية", L"Monatskalender", L"Calendrier mensuel", L"Imsakiyah Bulanan"},
    // TipRefresh
    {L"Yenile", L"Refresh", L"تحديث", L"Aktualisieren", L"Actualiser", L"Segarkan"},
    // TipSettings
    {L"Ayarlar", L"Settings", L"الإعدادات", L"Einstellungen", L"Paramètres", L"Pengaturan"},

    // TestNotifTitle
    {L"VakitÇubuğu — Bildirim Testi", L"VakitÇubuğu — Notification Test", L"VakitÇubuğu — اختبار الإشعارات", L"VakitÇubuğu — Benachrichtigungstest", L"VakitÇubuğu — Test de notification", L"VakitÇubuğu — Uji Notifikasi"},
    // TestNotifBody
    {L"Vaktinden önce ve vaktinde Windows bildirimleri aktif.", L"Pre-prayer and on-time Windows notifications are active.", L"إشعارات ويندوز قبل الوقت وعند دخول الوقت مفعّلة.", L"Windows-Benachrichtigungen vor und zur Gebetszeit aktiv.", L"Les notifications Windows avant et à l'heure sont actives.", L"Notifikasi Windows sebelum dan tepat waktu aktif."},

    // ImsakiyeWindowTitle
    {L"VakitÇubuğu — Aylık ve Ramazan İmsakiyesi", L"VakitÇubuğu — Monthly & Ramadan Prayer Schedule", L"VakitÇubuğu — الإمساكية الشهرية ورمضان", L"VakitÇubuğu — Monats- & Ramadan-Gebetsplan", L"VakitÇubuğu — Calendrier mensuel & Ramadan", L"VakitÇubuğu — Jadwal Sholat Bulanan & Ramadan"},
    // BtnToday
    {L"Bugün", L"Today", L"اليوم", L"Heute", L"Aujourd'hui", L"Hari Ini"},
    // BtnRamadan
    {L"Ramazan İmsakiyesi", L"Ramadan Schedule", L"إمساكية رمضان", L"Ramadan-Plan", L"Calendrier Ramadan", L"Jadwal Ramadan"},
    // BtnMonthly
    {L"Aylık Görünüm", L"Monthly View", L"عرض شهري", L"Monatsansicht", L"Vue mensuelle", L"Tampilan Bulanan"},
    // BtnSync
    {L"Eşitle", L"Sync", L"مزامنة", L"Synchronisieren", L"Synchroniser", L"Sinkronkan"},
    // BtnExportCsv
    {L"CSV İndir", L"Export CSV", L"تصدير CSV", L"CSV exportieren", L"Exporter CSV", L"Ekspor CSV"},
    // BtnExportHtml
    {L"HTML / Yazdır", L"HTML / Print", L"HTML / طباعة", L"HTML / Drucken", L"HTML / Imprimer", L"HTML / Cetak"},
    // ColDate
    {L"Tarih", L"Date", L"التاريخ", L"Datum", L"Date", L"Tanggal"},
    // ColHijri
    {L"Hicri", L"Hijri", L"هجري", L"Hidschri", L"Hégirien", L"Hijriah"},
    // RamadanTitlePrefix
    {L"Ramazan İmsakiyesi", L"Ramadan Schedule", L"إمساكية رمضان", L"Ramadan-Zeitplan", L"Calendrier du Ramadan", L"Jadwal Ramadan"},
    // RamadanDayPrefix
    {L"Ramazan", L"Ramadan", L"رمضان", L"Ramadan", L"Ramadan", L"Ramadan"},
    // StatusExportSaved
    {L"Dışa aktarıldı: ", L"Exported: ", L"تم التصدير: ", L"Exportiert: ", L"Exporté : ", L"Diekspor: "},
    // StatusSyncDone
    {L"İmsakiye güncellendi.", L"Schedule updated.", L"تم تحديث الإمساكية.", L"Zeitplan aktualisiert.", L"Calendrier mis à jour.", L"Jadwal diperbarui."},
    // StatusSyncOffline
    {L"İnternet yok — çevrimdışı hesaplama gösteriliyor.", L"Offline — showing local astronomical calculation.", L"غير متصل — يتم عرض الحساب الفلكي المحلي.", L"Offline — lokale Berechnung wird angezeigt.", L"Hors ligne — calcul local affiché.", L"Offline — menampilkan perhitungan lokal."},
    // ImsakiyeFooterHint
    {L"Bugün vurgulu, Cuma altın rozetli · Ramazan modunda 30 günlük tam liste gösterilir.", L"Today highlighted, Friday gold badge · Ramadan view displays full 30-day schedule.", L"اليوم محدّد والجمعة مميّزة · وضع رمضان يعرض 30 يوماً كاملاً.", L"Heute hervorgehoben, Freitag gold · Ramadan-Ansicht zeigt 30 Tage.", L"Aujourd'hui surligné, vendredi doré · Vue Ramadan sur 30 jours.", L"Hari ini ditandai, Jumat emas · Mode Ramadan tampilkan 30 hari penuh."},

    // SettingsWindowTitle
    {L"VakitÇubuğu — Ayarlar", L"VakitÇubuğu — Settings", L"VakitÇubuğu — الإعدادات", L"VakitÇubuğu — Einstellungen", L"VakitÇubuğu — Paramètres", L"VakitÇubuğu — Pengaturan"},
    // TabAppearance
    {L"Görünüm", L"Appearance", L"المظهر", L"Darstellung", L"Apparence", L"Tampilan"},
    // TabLocation
    {L"Konum & Vakitler", L"Location & Times", L"الموقع والمواقيت", L"Ort & Zeiten", L"Lieu & Horaires", L"Lokasi & Waktu"},
    // TabNotifications
    {L"Bildirim & Ses", L"Alerts & Audio", L"التنبيهات والصوت", L"Hinweise & Ton", L"Alertes & Son", L"Notifikasi & Suara"},
    // TabLanguageSystem
    {L"Dil & Sistem", L"Language & System", L"اللغة والنظام", L"Sprache & System", L"Langue & Système", L"Bahasa & Sistem"},
    // LivePreviewLabel
    {L"Canlı Görev Çubuğu Önizlemesi", L"Live Taskbar Preview", L"معاينة شريط المهام", L"Live-Taskleistenvorschau", L"Aperçu barre des tâches", L"Pratinjau Taskbar"},
    // FooterAutoSave
    {L"Değişiklikler anında kaydedilir ve uygulanır.", L"Changes are saved and applied automatically.", L"يتم حفظ التغييرات وتطبيقها تلقائياً.", L"Änderungen werden sofort gespeichert.", L"Modifications enregistrées automatiquement.", L"Perubahan disimpan otomatis."},
    // BtnOk
    {L"Tamam", L"OK", L"موافق", L"OK", L"OK", L"Selesai"},

    // LblTaskbarStyle
    {L"Görev Çubuğu Stili:", L"Taskbar Display Style:", L"نمط شريط المهام:", L"Taskleisten-Stil:", L"Style barre des tâches :", L"Gaya Taskbar:"},
    // LblCustomTemplate
    {L"Özel Şablon (\\n alt satır):", L"Custom Template (\\n line 2):", L"قالب مخصص (\\n سطر جديد):", L"Vorlage (\\n neue Zeile):", L"Modèle (\\n ligne 2) :", L"Templat Kustom (\\n baris 2):"},
    // LblFlyoutDesign
    {L"Açılır Ekran Tasarımı:", L"Flyout Panel Design:", L"تصميم النافذة المنبثقة:", L"Flyout-Design:", L"Design du panneau :", L"Desain Panel Popup:"},
    // LblDockPos
    {L"Görev Çubuğu Konumu:", L"Taskbar Position:", L"موقع شريط المهام:", L"Taskleisten-Position:", L"Position barre des tâches :", L"Posisi Taskbar:"},
    // DockRightTray
    {L"Sağ — Sistem tepsisi yanı (^ solunda)", L"Right — Near system tray (left of ^)", L"يمين — بجانب علبة النظام", L"Rechts — Neben Infobereich", L"Droite — Près de la zone notif.", L"Kanan — Dekat System Tray"},
    // DockFarRight
    {L"En Sağ — Saatin sağı / sağ köşe", L"Far Right — Right edge / over clock", L"أقصى اليمين — فوق الساعة", L"Ganz rechts — Über der Uhr", L"Extrême droite — Sur l'horloge", L"Paling Kanan — Ujung jam"},
    // DockCenter
    {L"Orta — Görev çubuğu ortası", L"Center — Taskbar middle", L"وسط — منتصف شريط المهام", L"Mitte — Taskleisten-Mitte", L"Centre — Milieu de la barre", L"Tengah — Tengah Taskbar"},
    // DockLeft
    {L"En Sol — Görev çubuğu solu", L"Far Left — Taskbar left side", L"أقصى اليسار — يسار الشريط", L"Ganz links — Linke Seite", L"Extrême gauche — Côté gauche", L"Paling Kiri — Sisi kiri"},
    // LblOffset
    {L"Yatay İnce Kaydırma:", L"Horizontal Offset:", L"الإزاحة الأفقية:", L"Horizontaler Versatz:", L"Décalage horizontal :", L"Geser Horizontal:"},
    // ChkShowPill
    {L"Görev çubuğunda gösterge şeridini göster", L"Show countdown pill on taskbar", L"إظهار الشريط في شريط المهام", L"Anzeige auf Taskleiste einblenden", L"Afficher la pilule dans la barre", L"Tampilkan badge di taskbar"},
    // ChkOpaqueBg
    {L"Opak arka plan (üst üste binmeleri kapatır)", L"Opaque backdrop (prevents overlap)", L"خلفية معتمة (لمنع التداخل)", L"Deckender Hintergrund", L"Fond opaque", L"Latar belakang solid"},
    // ChkShowTray
    {L"Sistem tepsisinde dinamik ikon göster", L"Show dynamic icon in system tray", L"إظهار أيقونة في علبة النظام", L"Symbol im Infobereich anzeigen", L"Icône dynamique dans le tray", L"Tampilkan ikon di System Tray"},
    // ChkShowBar
    {L"Şerit altında ince ilerleme çubuğu göster", L"Show thin progress bar under pill", L"شريط تقدم رفيع أسفل العرض", L"Fortschrittsbalken anzeigen", L"Barre de progression fine", L"Tampilkan bilah progres"},
    // ChkShowSec
    {L"Saniyeyi göster (pil tasarrufu için kapalı önerilir)", L"Show seconds (off saves laptop battery)", L"إظهار الثواني (إطفاؤه يوفر البطارية)", L"Sekunden anzeigen", L"Afficher les secondes", L"Tampilkan detik"},
    // ChkAccentSync
    {L"Windows 11 sistem vurgu rengini kullan", L"Match Windows 11 system accent color", L"مطابقة لون تمييز ويندوز 11", L"Windows 11-Akzentfarbe verwenden", L"Utiliser la couleur d'accentuation Win11", L"Ikuti warna aksen Windows 11"},
    // LblTrayStyle
    {L"Tepsi İkon Stili:", L"System Tray Icon Style:", L"نمط أيقونة العلبة:", L"Infobereich-Symbolstil:", L"Style d'icône du tray :", L"Gaya Ikon Tray:"},
    // TrayTwoLine
    {L"İki Satır Rozet (İ / 23)", L"Two-Line Badge", L"شارة من سطرين", L"Zweizeiliges Abzeichen", L"Badge deux lignes", L"Lencana Dua Baris"},
    // TrayRing
    {L"İlerleme Halkalı Dakika", L"Progress Ring + Minutes", L"حلقة التقدم والدقائق", L"Fortschrittsring + Minuten", L"Anneau + Minutes", L"Cincin Progres + Menit"},
    // TrayCrescent
    {L"Klasik Hilal İkonu", L"Classic Crescent Icon", L"أيقونة الهلال الكلاسيكية", L"Klassischer Halbmond", L"Croissant classique", L"Ikon Bulan Sabit"},
    // LblFontSize
    {L"Yazı Boyutu:", L"Text Size:", L"حجم الخط:", L"Schriftgröße:", L"Taille du texte :", L"Ukuran Teks:"},
    // FontSmall
    {L"Küçük", L"Small", L"صغير", L"Klein", L"Petit", L"Kecil"},
    // FontNormal
    {L"Normal (Önerilen)", L"Normal (Recommended)", L"عادي (موصى به)", L"Normal (Empfohlen)", L"Normal (Recommandé)", L"Normal (Disarankan)"},
    // FontLarge
    {L"Büyük", L"Large", L"كبير", L"Groß", L"Grand", L"Besar"},
    // LblUrgentColor
    {L"Son Dakika Uyarı Rengi:", L"Urgent Highlight:", L"تمييز اللحظات الأخيرة:", L"Dringlichkeits-Farbe:", L"Couleur urgente :", L"Warna Peringatan Akhir:"},
    // UrgentOff
    {L"Kapalı", L"Off", L"معطّل", L"Aus", L"Désactivé", L"Mati"},
    // Urgent10
    {L"Son 10 dakika", L"Last 10 minutes", L"آخر 10 دقائق", L"Letzte 10 Minuten", L"10 dernières minutes", L"10 menit terakhir"},
    // Urgent15
    {L"Son 15 dakika", L"Last 15 minutes", L"آخر 15 دقيقة", L"Letzte 15 Minuten", L"15 dernières minutes", L"15 menit terakhir"},
    // Urgent20
    {L"Son 20 dakika", L"Last 20 minutes", L"آخر 20 دقيقة", L"Letzte 20 Minuten", L"20 dernières minutes", L"20 menit terakhir"},
    // Urgent30
    {L"Son 30 dakika", L"Last 30 minutes", L"آخر 30 دقيقة", L"Letzte 30 Minuten", L"30 dernières minutes", L"30 menit terakhir"},
    // LblTheme
    {L"Uygulama Teması:", L"App Theme:", L"سمة التطبيق:", L"App-Design:", L"Thème :", L"Tema Aplikasi:"},
    // ThemeSystem
    {L"Sistem (Otomatik)", L"System (Default)", L"النظام (تلقائي)", L"System (Standard)", L"Système (Auto)", L"Sistem (Otomatis)"},
    // ThemeDark
    {L"Koyu", L"Dark", L"داكن", L"Dunkel", L"Sombre", L"Gelap"},
    // ThemeLight
    {L"Açık", L"Light", L"فاتح", L"Hell", L"Clair", L"Terang"},

    // BtnDetectLocation
    {L"Konumumu Otomatik Bul", L"Auto-Detect Location", L"تحديد الموقع تلقائياً", L"Standort automatisch erkennen", L"Détecter ma position", L"Deteksi Lokasi Otomatis"},
    // LocDetectHint
    {L"Konum algılamak için butona tıklayın.", L"Click button to detect location by IP.", L"اضغط لتحديد الموقع عبر الإنترنت.", L"Klicken zur Standorterkennung.", L"Cliquez pour détecter votre ville.", L"Klik tombol untuk deteksi lokasi."},
    // LocDetecting
    {L"Konum algılanıyor...", L"Detecting location...", L"جاري تحديد الموقع...", L"Standort wird ermittelt...", L"Détection en cours...", L"Mendeteksi lokasi..."},
    // LocDetectFailed
    {L"Konum algılanamadı. İnterneti kontrol edin.", L"Location detection failed. Check internet.", L"تعذر تحديد الموقع. تحقق من الاتصال.", L"Erkennung fehlgeschlagen.", L"Échec de détection.", L"Deteksi lokasi gagal."},
    // ChkAutoMethod
    {L"Ülkeye göre uygun hesaplama metodunu otomatik seç", L"Auto-select calculation method for country", L"اختيار طريقة الحساب تلقائياً حسب الدولة", L"Berechnungsmethode nach Land wählen", L"Choisir la méthode selon le pays", L"Pilih metode otomatis sesuai negara"},
    // LblCitySearch
    {L"Şehir Ara (81 İl & Dünya):", L"Search City (Worldwide):", L"ابحث عن مدينة:", L"Stadt suchen:", L"Rechercher une ville :", L"Cari Kota:"},
    // ChkCustomCoord
    {L"Özel Enlem/Boylam:", L"Custom Lat / Lon:", L"إحداثيات مخصصة:", L"Eigene Koordinaten:", L"Coordonnées personnalisées :", L"Koordinat Kustom:"},
    // LblLat
    {L"Enlem:", L"Lat:", L"خط العرض:", L"Breite:", L"Lat :", L"Lintang:"},
    // LblLon
    {L"Boylam:", L"Lon:", L"خط الطول:", L"Länge:", L"Lon :", L"Bujur:"},
    // BtnApplyCoord
    {L"Koordinat Uygula", L"Apply Coords", L"تطبيق", L"Übernehmen", L"Appliquer", L"Terapkan"},
    // LblCalcMethod
    {L"Hesaplama Metodu:", L"Calculation Method:", L"طريقة الحساب:", L"Berechnungsmethode:", L"Méthode de calcul :", L"Metode Perhitungan:"},
    // LblAsrMethod
    {L"İkindi Mezhebi:", L"Asr Juristic Method:", L"مذهب العصر:", L"Asr-Rechtsschule:", L"Méthode Asr :", L"Mazhab Ashar:"},
    // AsrStandard
    {L"Standart (Şafii / Maliki / Hanbeli / Diyanet)", L"Standard (Shafi / Maliki / Hanbali)", L"الجمهور (الشافعي / المالكي / الحنبلي)", L"Standard (Schafi / Maliki / Hanbali)", L"Standard (Chaféite / Malékite)", L"Standar (Syafi'i / Maliki / Hanbali)"},
    // AsrHanafi
    {L"Hanefi (Asr-ı Sani)", L"Hanafi", L"الحنفي", L"Hanafitisch", L"Hanafite", L"Hanafi"},
    // ChkOnlineSync
    {L"Aylık vakitleri arka planda önbelleğe al (çevrimdışı yedekli)", L"Cache monthly schedule in background (offline ready)", L"حفظ المواقيت الشهرية في الخلفية", L"Monatszeiten im Hintergrund puffern", L"Mettre en cache le mois", L"Simpan jadwal bulanan offline"},
    // BtnSyncNow
    {L"Vakitleri Güncelle", L"Sync Now", L"تحديث الآن", L"Jetzt abrufen", L"Actualiser", L"Sinkronkan"},
    // BtnOpenImsakiye
    {L"Aylık İmsakiye", L"Open İmsakiye", L"الإمساكية الشهرية", L"Monatskalender", L"Calendrier mensuel", L"Buka Imsakiyah"},
    // SyncingText
    {L"Aylık vakitler eşitleniyor...", L"Syncing monthly prayer times...", L"جاري مزامنة المواقيت الشهرية...", L"Monatszeiten werden geladen...", L"Synchronisation du mois...", L"Menyinkronkan jadwal bulanan..."},
    // SyncOkText
    {L"Bu ayın vakitleri güncellendi ve önbelleğe alındı.", L"Monthly prayer times updated and cached.", L"تم تحديث المواقيت وحفظها.", L"Monatszeiten aktualisiert.", L"Horaires du mois mis à jour.", L"Jadwal bulanan diperbarui."},
    // SyncOfflineText
    {L"İnternet bağlantısı yok — çevrimdışı hesaplama kullanılıyor.", L"No internet — using local astronomical fallback.", L"لا يوجد اتصال — يتم استخدام الحساب المحلي.", L"Kein Internet — Offline-Berechnung aktiv.", L"Hors ligne — calcul local actif.", L"Tidak ada internet — pakai hitungan lokal."},
    // LblOffsets
    {L"Vakit İnce Ayarı (Dakika ±):", L"Manual Time Offsets (Min ±):", L"تعديل الدقائق (±):", L"Minuten-Feinjustierung (±):", L"Ajustement manuel (min ±) :", L"Koreksi Menit (±):"},
    // BtnApplyOffsets
    {L"Ofsetleri Uygula", L"Apply Offsets", L"تطبيق التعديل", L"Übernehmen", L"Appliquer", L"Terapkan"},

    // ColPrayer
    {L"Vakit", L"Prayer", L"الصلاة", L"Gebet", L"Prière", L"Sholat"},
    // ColAtTime
    {L"Vaktinde", L"On Time", L"عند الوقت", L"Zur Zeit", L"À l'heure", L"Tepat Waktu"},
    // ColPreRemind
    {L"Önce Hatırlat", L"Remind Before", L"التنبيه المسبق", L"Vorab erinnern", L"Rappel avant", L"Pengingat"},
    // ColSoundEzan
    {L"Ses / Ezan", L"Sound / Adhan", L"الصوت / الأذان", L"Ton / Adhan", L"Son / Adhan", L"Suara / Adzan"},
    // ChkNotify
    {L"Bildir", L"Notify", L"إشعار", L"Melden", L"Notifier", L"Beri tahu"},
    // ChkPlaySound
    {L"Ses Çal", L"Play", L"تشغيل", L"Abspielen", L"Jouer", L"Putar"},
    // PreOff
    {L"Kapalı", L"Off", L"معطّل", L"Aus", L"Désactivé", L"Mati"},
    // Pre5
    {L"5 dk önce", L"5 min before", L"قبل 5 د", L"5 Min. vorher", L"5 min avant", L"5 mnt sblm"},
    // Pre10
    {L"10 dk önce", L"10 min before", L"قبل 10 د", L"10 Min. vorher", L"10 min avant", L"10 mnt sblm"},
    // Pre15
    {L"15 dk önce", L"15 min before", L"قبل 15 د", L"15 Min. vorher", L"15 min avant", L"15 mnt sblm"},
    // Pre20
    {L"20 dk önce", L"20 min before", L"قبل 20 د", L"20 Min. vorher", L"20 min avant", L"20 mnt sblm"},
    // Pre30
    {L"30 dk önce", L"30 min before", L"قبل 30 د", L"30 Min. vorher", L"30 min avant", L"30 mnt sblm"},
    // Pre45
    {L"45 dk önce", L"45 min before", L"قبل 45 د", L"45 Min. vorher", L"45 min avant", L"45 mnt sblm"},
    // Pre60
    {L"60 dk önce", L"60 min before", L"قبل 60 د", L"60 Min. vorher", L"60 min avant", L"60 mnt sblm"},
    // LblEntrySound
    {L"Vakit Girdi Sesi:", L"On-Time Sound:", L"صوت دخول الوقت:", L"Gebetszeit-Ton:", L"Son à l'heure :", L"Suara Masuk Waktu:"},
    // LblPreSound
    {L"Vakit Öncesi Sesi:", L"Pre-Alert Sound:", L"صوت التنبيه المسبق:", L"Vorab-Hinweis-Ton:", L"Son de rappel :", L"Suara Pengingat:"},
    // SoundNone
    {L"Sessiz", L"Silent", L"صامت", L"Stumm", L"Silencieux", L"Hening"},
    // SoundChime
    {L"Yumuşak Çan (Sentez)", L"Gentle Chime", L"رنين هادئ", L"Sanfter Klang", L"Carillon doux", L"Lonceng Lembut"},
    // SoundBell
    {L"Huzurlu Zil (Sentez)", L"Warm Bell", L"جرس دافئ", L"Warme Glocke", L"Cloche douce", L"Bel Hangat"},
    // SoundAdhanShort
    {L"Ücretsiz Kısa Ezan (Nasser al-Qatami)", L"Free Short Adhan (Nasser al-Qatami)", L"أذان قصير مجاني (ناصر القطامي)", L"Kurzer Adhan (Nasser al-Qatami)", L"Adhan court (Nasser al-Qatami)", L"Adzan Singkat (Nasser al-Qatami)"},
    // SoundAdhanMakkah
    {L"Ücretsiz Uzun Ezan (Majed al-Hamathani)", L"Free Full Adhan (Majed al-Hamathani)", L"أذان طويل مجاني (ماجد الهمثاني)", L"Langer Adhan (Majed al-Hamathani)", L"Adhan long (Majed al-Hamathani)", L"Adzan Panjang (Majed al-Hamathani)"},
    // SoundCustom
    {L"Özel Ses Dosyası (.mp3 / .wav)", L"Custom Audio File (.mp3 / .wav)", L"ملف صوتي مخصص", L"Eigene Audiodatei", L"Fichier audio perso", L"File Audio Kustom"},
    // LblCustomSound
    {L"Özel Ses Dosyası:", L"Custom Audio:", L"ملف الصوت:", L"Eigene Datei:", L"Fichier perso :", L"File Kustom:"},
    // BtnBrowse
    {L"Gözat...", L"Browse...", L"استعراض...", L"Durchsuchen...", L"Parcourir...", L"Pilih..."},
    // LblVolume
    {L"Ses Düzeyi:", L"Volume:", L"مستوى الصوت:", L"Lautstärke:", L"Volume :", L"Volume:"},
    // BtnTestEntry
    {L"Vakit Sesi Test Et", L"Test Adhan", L"تجربة الأذان", L"Ton testen", L"Tester l'Adhan", L"Uji Adzan"},
    // BtnTestPre
    {L"Ön Uyarı Test Et", L"Test Pre-Alert", L"تجربة المسبق", L"Vorab testen", L"Tester rappel", L"Uji Pengingat"},
    // BtnStopSound
    {L"■ Sesi Durdur", L"■ Stop Audio", L"■ إيقاف", L"■ Stopp", L"■ Arrêter", L"■ Stop"},
    // BtnTestNotif
    {L"Bildirim Test Et", L"Test Notification", L"تجربة الإشعار", L"Benachrichtigung", L"Tester notif.", L"Uji Notifikasi"},
    // ChkQuietFullscreen
    {L"Tam ekran / oyun / Odak Yardımı sırasında sessize al", L"Mute during fullscreen apps, games & Focus Assist", L"كتم الصوت أثناء الألعاب ووضع التركيز", L"Bei Vollbild / Spielen / Fokus stummschalten", L"Silencieux en plein écran / jeux", L"Bisukan saat layar penuh / game"},
    // ChkKerahatIndicator
    {L"Kerahat vakti uyarısını ve Cuma vurgusunu göster", L"Show Makruh time indicator & Jumu'ah highlight", L"إظهار تنبيه الكراهة وتمييز الجمعة", L"Karaha-Hinweis & Dschuma hervorheben", L"Afficher alerte Karaha & Joumou'a", L"Tampilkan tanda Waktu Terlarang & Jumat"},

    // LblAppLanguage
    {L"Uygulama Dili:", L"App Language:", L"لغة التطبيق:", L"App-Sprache:", L"Langue :", L"Bahasa Aplikasi:"},
    // SectionCustomNames
    {L"Namaz İsimlerini Manuel Değiştir (Boş bırakılırsa dil varsayılanı kullanılır):", L"Customize Prayer Names Manually (Empty = language default):", L"تخصيص أسماء الصلوات يدوياً (الفارغ = الافتراضي):", L"Gebetsnamen manuell anpassen (Leer = Standard):", L"Personnaliser les noms des prières (Vide = défaut) :", L"Ubah Nama Sholat Manual (Kosong = default bahasa):"},
    // ColDefaultName
    {L"Varsayılan", L"Default", L"الافتراضي", L"Standard", L"Défaut", L"Default"},
    // ColCustomName
    {L"Özel İsim", L"Custom Name", L"اسم مخصص", L"Eigener Name", L"Nom personnalisé", L"Nama Kustom"},
    // ColShortCode
    {L"Kısa Kod", L"Short", L"اختصار", L"Kurz", L"Code", L"Singkat"},
    // BtnApplyNames
    {L"İsimleri Uygula", L"Apply Names", L"تطبيق الأسماء", L"Namen anwenden", L"Appliquer les noms", L"Terapkan Nama"},
    // BtnResetNames
    {L"Varsayılana Sıfırla", L"Reset Defaults", L"إعادة تعيين", L"Zurücksetzen", L"Réinitialiser", L"Reset Default"},
    // SectionSystem
    {L"Sistem, Otomatik Bellek ve Yedekleme:", L"System, Auto Memory & Backup:", L"النظام والذاكرة التلقائية والنسخ:", L"System, Speicher & Sicherung:", L"Système, Mémoire & Sauvegarde :", L"Sistem, Memori Otomatis & Cadangan:"},
    // ChkStartup
    {L"Windows açılışında otomatik başlat", L"Start automatically with Windows", L"التشغيل التلقائي مع ويندوز", L"Mit Windows starten", L"Démarrer avec Windows", L"Jalankan otomatis saat Windows mulai"},
    // ChkAutoMem
    {L"Belleği otomatik optimize et (boşta ve pencere kapanınca RAM düşür)", L"Optimize memory automatically (recommended)", L"تحسين الذاكرة تلقائياً (موصى به)", L"Speicher automatisch optimieren (Empfohlen)", L"Optimiser la mémoire automatiquement", L"Optimalkan memori secara otomatis (disarankan)"},
    // BtnExportSettings
    {L"Ayarları Dışa Aktar", L"Export Settings", L"تصدير الإعدادات", L"Einstellungen exportieren", L"Exporter paramètres", L"Ekspor Pengaturan"},
    // BtnImportSettings
    {L"Ayarları İçe Aktar", L"Import Settings", L"استيراد الإعدادات", L"Einstellungen importieren", L"Importer paramètres", L"Impor Pengaturan"},
    // MsgSettingsExported
    {L"Ayarlar JSON dosyasına dışa aktarıldı.", L"Settings exported to JSON file.", L"تم تصدير الإعدادات بنجاح.", L"Einstellungen exportiert.", L"Paramètres exportés.", L"Pengaturan berhasil diekspor."},
    // MsgSettingsImported
    {L"Ayarlar içe aktarıldı ve uygulandı.", L"Settings imported and applied.", L"تم استيراد الإعدادات وتطبيقها.", L"Einstellungen importiert.", L"Paramètres importés.", L"Pengaturan berhasil diimpor."},

    // ChkRamadanMode
    {L"Ramazan ayında İftar / Sahur özel geri sayımını göster", L"Show special Iftar / Suhoor countdown during Ramadan", L"إظهار عداد الإفطار والسحور الخاص في رمضان", L"Im Ramadan Iftar- / Suhoor-Countdown anzeigen", L"Afficher le décompte Iftar / Souhour pendant le Ramadan", L"Tampilkan hitung mundur Buka / Sahur saat Ramadan"},
    // MenuUpgradePro
    {L"★ VakitÇubuğu Pro'ya Yükselt...", L"★ Upgrade to VakitÇubuğu Pro...", L"★ الترقية إلى الإصدار الاحترافي...", L"★ Auf Pro upgraden...", L"★ Passer à VakitÇubuğu Pro...", L"★ Upgrade ke VakitÇubuğu Pro..."},
    // MenuManageLicense
    {L"✓ VakitÇubuğu Pro (Lisans Yönetimi)...", L"✓ VakitÇubuğu Pro (License)...", L"✓ الإصدار الاحترافي (الترخيص)...", L"✓ VakitÇubuğu Pro (Lizenz)...", L"✓ VakitÇubuğu Pro (Licence)...", L"✓ VakitÇubuğu Pro (Lisensi)..."},
    // ProWindowTitle
    {L"VakitÇubuğu Pro — Lisans & Yükseltme", L"VakitÇubuğu Pro — Upgrade & License", L"VakitÇubuğu Pro — الترقية والترخيص", L"VakitÇubuğu Pro — Upgrade & Lizenz", L"VakitÇubuğu Pro — Mise à niveau", L"VakitÇubuğu Pro — Upgrade & Lisensi"},
    // ProStatusTrial
    {L"14 Günlük Ücretsiz Pro Deneme Aktif", L"14-Day Free Pro Trial Active", L"الفترة التجريبية المجانية (14 يوماً) مفعّلة", L"14-Tage-Pro-Testversion aktiv", L"Essai Pro gratuit de 14 jours actif", L"Uji Coba Pro 14 Hari Aktif"},
    // ProStatusFree
    {L"Ücretsiz Temel Sürüm (Temel ibadet vakitleri daima ücretsizdir)", L"Free Edition (Core prayer times remain 100% free)", L"الإصدار المجاني (المواقيت الأساسية مجانية دائماً)", L"Kostenlose Basisversion", L"Version gratuite (Horaires toujours gratuits)", L"Edisi Gratis (Jadwal Sholat Selalu Gratis)"},
    // ProStatusActiveAnnual
    {L"Pro Yıllık Lisans Aktif ✓", L"Pro Annual License Active ✓", L"الترخيص السنوي الاحترافي مفعّل ✓", L"Pro-Jahreslizenz aktiv ✓", L"Licence Pro annuelle active ✓", L"Lisensi Pro Tahunan Aktif ✓"},
    // ProStatusActiveLifetime
    {L"Pro Ömür Boyu Lisans Aktif ✓", L"Pro Lifetime License Active ✓", L"ترخيص مدى الحياة الاحترافي مفعّل ✓", L"Lebenslange Pro-Lizenz aktiv ✓", L"Licence Pro à vie active ✓", L"Lisensi Pro Selamanya Aktif ✓"},
    // ProPlanAnnualTitle
    {L"Yıllık Pro", L"Annual Pro", L"اشتراك سنوي", L"Jährliches Pro", L"Pro Annuel", L"Pro Tahunan"},
    // ProPlanAnnualPrice
    {L"$4.99 / yıl  (~₺59)", L"$4.99 / year", L"$4.99 / سنة", L"4,99 € / Jahr", L"4,99 € / an", L"$4.99 / tahun"},
    // ProPlanLifetimeTitle
    {L"Ömür Boyu Pro", L"Lifetime Pro", L"مدى الحياة", L"Lebenslanges Pro", L"Pro à Vie", L"Pro Selamanya"},
    // ProPlanLifetimePrice
    {L"$9.99 Tek Seferlik", L"$9.99 One-Time", L"$9.99 دفعة واحدة", L"9,99 € Einmalig", L"9,99 € Paiement unique", L"$9.99 Sekali Bayar"},
    // ProBadgePopular
    {L"EN ÇOK TERCİH EDİLEN", L"MOST POPULAR", L"الأكثر طلباً", L"AM BELIEBTESTEN", L"LE PLUS POPULAIRE", L"PALING POPULER"},
    // ProFeat1
    {L"• 4 Windows 11 Açılır Ekran Tasarımı (Bento Kartları + Kıble Pusulası)", L"• All 4 Windows 11 Flyout Designs (Bento Cards + Qibla Compass)", L"• 4 تصاميم نوافذ منبثقة (بطاقات + بوصلة القبلة)", L"• Alle 4 Win11-Flyout-Designs + Qibla-Kompass", L"• 4 designs de panneau + Boussole Qibla", L"• 4 Desain Popup + Kompas Kiblat"},
    // ProFeat2
    {L"• 9 Görev Çubuğu Stili + Özel Şablon Düzenleyici", L"• All 9 Taskbar Display Styles + Custom Template Editor", L"• 9 أنماط لشريط المهام + محرر القوالب", L"• Alle 9 Taskleisten-Stile + Vorlageneditor", L"• 9 styles barre des tâches + Éditeur", L"• 9 Gaya Taskbar + Editor Templat"},
    // ProFeat3
    {L"• Özel MP3/WAV Ezan Yükleme & Namaz İsimlerini Değiştirme", L"• Custom MP3/WAV Adhan Audio & Editable Prayer Names", L"• إضافة أذان MP3 مخصص وتعديل أسماء الصلوات", L"• Eigene MP3-Adhans & anpassbare Gebetsnamen", L"• Adhan MP3 personnalisé & noms modifiables", L"• Adzan MP3 Kustom & Ubah Nama Sholat"},
    // ProFeat4
    {L"• Aylık & Ramazan İmsakiyesi HTML Yazdırma / CSV Dışa Aktarma", L"• Export & Print Monthly / Ramadan Schedule (CSV & HTML/PDF)", L"• تصدير وطباعة الإمساكية الشهرية ورمضان", L"• Monats- & Ramadan-Plan drucken / CSV exportieren", L"• Export CSV & Impression HTML/PDF du calendrier", L"• Ekspor CSV & Cetak Jadwal Bulanan/Ramadan"},
    // ProFeat5
    {L"• Ayar Yedekleme/Geri Yükleme & 3 Bilgisayara Kadar Tek Lisans", L"• Settings JSON Backup/Restore & Future Updates Included", L"• نسخ احتياطي للإعدادات مع التحديثات المستقبلية", L"• Einstellungs-Backup & alle Updates inklusive", L"• Sauvegarde JSON & mises à jour incluses", L"• Cadangan Pengaturan & Pembaruan Gratis"},
    // BtnBuyAnnual
    {L"Yıllık Al ($4.99)", L"Get Annual ($4.99)", L"شراء سنوي ($4.99)", L"Jährlich (4,99 €)", L"Annuel (4,99 €)", L"Beli Tahunan"},
    // BtnBuyLifetime
    {L"Ömür Boyu Al ($9.99)", L"Get Lifetime ($9.99)", L"مدى الحياة ($9.99)", L"Lebenslang (9,99 €)", L"À vie (9,99 €)", L"Beli Selamanya"},
    // BtnOpenStore
    {L"Microsoft Store", L"Microsoft Store", L"مايكروسوفت ستور", L"Microsoft Store", L"Microsoft Store", L"Microsoft Store"},
    // BtnSupportDev
    {L"Gönüllü Destek / Bahşiş", L"Support Developer", L"دعم المطور", L"Entwickler unterstützen", L"Soutenir le projet", L"Dukung Pengembang"},
    // BtnActivateKey
    {L"Anahtarı Etkinleştir", L"Activate Key", L"تفعيل المفتاح", L"Lizenz aktivieren", L"Activer la clé", L"Aktifkan Kunci"},
    // BtnDeactivateKey
    {L"Lisansı Kaldır", L"Deactivate", L"إلغاء التفعيل", L"Deaktivieren", L"Désactiver", L"Nonaktifkan"},
    // LblLicenseKey
    {L"Lisans Anahtarı (VKPR-... veya Web Sipariş Kodu):", L"License Key (VKPR-... or Web Order Key):", L"مفتاح الترخيص:", L"Lizenzschlüssel:", L"Clé de licence :", L"Kunci Lisensi:"},
    // ProActivating
    {L"Lisans anahtarı doğrulanıyor...", L"Verifying license key...", L"جاري التحقق من المفتاح...", L"Schlüssel wird geprüft...", L"Vérification en cours...", L"Memeriksa kunci..."},
    // ProActivatedOk
    {L"Teşekkürler! VakitÇubuğu Pro başarıyla etkinleştirildi.", L"Thank you! VakitÇubuğu Pro is now active.", L"شكراً لك! تم تفعيل الإصدار الاحترافي بنجاح.", L"Vielen Dank! VakitÇubuğu Pro aktiviert.", L"Merci ! VakitÇubuğu Pro activé.", L"Terima kasih! VakitÇubuğu Pro aktif."},
    // ProKeyInvalid
    {L"Geçersiz lisans anahtarı. Lütfen yazımı kontrol edin.", L"Invalid license key. Please check and try again.", L"مفتاح غير صالح. يرجى التحقق والمحاولة مجدداً.", L"Ungültiger Lizenzschlüssel.", L"Clé de licence invalide.", L"Kunci lisensi tidak valid."},
    // ProKeyExpired
    {L"Bu yıllık lisans anahtarının süresi dolmuş.", L"This annual license key has expired.", L"انتهت صلاحية هذا المفتاح السنوي.", L"Diese Jahreslizenz ist abgelaufen.", L"Cette licence annuelle a expiré.", L"Masa berlaku kunci tahunan telah habis."},
    // ProOnlineRequired
    {L"Web sipariş anahtarını etkinleştirmek için internet bağlantısı gerekir.", L"Internet connection required to activate this web license key.", L"مطلوب اتصال بالإنترنت لتفعيل هذا المفتاح.", L"Internetverbindung zur Aktivierung erforderlich.", L"Connexion Internet requise pour l'activation.", L"Koneksi internet diperlukan untuk aktivasi."},
    // ProLockedHint
    {L"Deneme süreniz doldu. Bu Pro özelliği açmak için lisans edinin — temel vakitler ücretsizdir.", L"Your free trial ended. Unlock this customization feature with Pro.", L"انتهت الفترة التجريبية. افتح هذه الميزة عبر النسخة الاحترافية.", L"Testzeit abgelaufen. Mit Pro freischalten.", L"Essai terminé. Débloquez cette fonction avec Pro.", L"Uji coba berakhir. Buka fitur ini dengan Pro."},
    // SectionLicense
    {L"Lisans Durumu:", L"License Status:", L"حالة الترخيص:", L"Lizenzstatus:", L"Statut de licence :", L"Status Lisensi:"},
    // BtnOpenProWindow
    {L"Pro / Lisans...", L"Pro / License...", L"الترخيص...", L"Pro / Lizenz...", L"Pro / Licence...", L"Pro / Lisensi..."},

    // BtnRestorePurchases
    {L"Satın Almaları Geri Yükle", L"Restore Purchases", L"استعادة المشتريات", L"Käufe wiederherstellen", L"Restaurer achats", L"Pulihkan Pembelian"},
    // BtnManageSubscription
    {L"Aboneliği Yönet", L"Manage Subscription", L"إدارة الاشتراك", L"Abo verwalten", L"Gérer l'abonnement", L"Kelola Langganan"},
    // BtnRateStore
    {L"Store'da Puanla", L"Rate in Store", L"تقييم في المتجر", L"Im Store bewerten", L"Noter sur le Store", L"Beri Nilai"},
    // StorePurchasing
    {L"Microsoft Store ödeme penceresi açılıyor...", L"Opening Microsoft Store checkout...", L"جاري فتح نافذة الدفع في مايكروسوفت ستور...", L"Microsoft Store-Checkout wird geöffnet...", L"Ouverture du paiement Microsoft Store...", L"Membuka pembayaran Microsoft Store..."},
    // StoreRestoring
    {L"Microsoft Store lisansları kontrol ediliyor...", L"Checking Microsoft Store licenses...", L"جاري التحقق من تراخيص مايكروسوفت ستور...", L"Microsoft Store-Lizenzen werden geprüft...", L"Vérification des licences Microsoft Store...", L"Memeriksa lisensi Microsoft Store..."},
    // StorePurchasedOk
    {L"Teşekkürler! VakitÇubuğu Pro Microsoft Store üzerinden etkinleştirildi.", L"Thank you! VakitÇubuğu Pro is now active via Microsoft Store.", L"شكراً لك! تم تفعيل الإصدار الاحترافي عبر مايكروسوفت ستور.", L"Vielen Dank! VakitÇubuğu Pro ist über den Microsoft Store aktiv.", L"Merci ! VakitÇubuğu Pro est actif via le Microsoft Store.", L"Terima kasih! VakitÇubuğu Pro aktif melalui Microsoft Store."},
    // StorePurchaseCanceled
    {L"Satın alma iptal edildi.", L"Purchase was canceled.", L"تم إلغاء الشراء.", L"Kauf abgebrochen.", L"Achat annulé.", L"Pembelian dibatalkan."},
    // StorePurchaseFailed
    {L"Microsoft Store'a bağlanılamadı. İnternet bağlantınızı kontrol edin.", L"Could not reach Microsoft Store. Check your internet connection.", L"تعذر الاتصال بمايكروسوفت ستور. تحقق من اتصالك بالإنترنت.", L"Verbindung zum Microsoft Store fehlgeschlagen.", L"Impossible de contacter le Microsoft Store.", L"Gagal terhubung ke Microsoft Store."},
    // StoreNotPackaged
    {L"Mağaza içi satın alma yalnızca Microsoft Store sürümünde çalışır.", L"In-app purchase is available in the Microsoft Store version.", L"الشراء داخل التطبيق متاح في نسخة مايكروسوفت ستور.", L"In-App-Käufe sind in der Microsoft Store-Version verfügbar.", L"L'achat intégré est disponible dans la version Microsoft Store.", L"Pembelian dalam aplikasi hanya tersedia di versi Microsoft Store."},
    // StoreRestoredNone
    {L"Aktif Microsoft Store Pro eklentisi bulunamadı.", L"No active Microsoft Store Pro add-on found.", L"لم يتم العثور على إضافة احترافية نشطة.", L"Kein aktives Microsoft Store Pro-Add-on gefunden.", L"Aucun module Pro Microsoft Store actif trouvé.", L"Tidak ada add-on Pro Microsoft Store yang aktif."},
    // StoreAddonNotConfigured
    {L"Store Eklenti Kimliği henüz tanımlanmadı (Partner Center ID gerekli).", L"Store Add-on ID not configured yet (Partner Center ID required).", L"معرف الإضافة غير معرّف بعد (مطلوب Partner Center ID).", L"Store-Add-on-ID noch nicht konfiguriert.", L"ID du module complémentaire non configuré.", L"ID Add-on Store belum dikonfigurasi."}
};

static_assert(sizeof(kUiStrings) / sizeof(kUiStrings[0]) == static_cast<size_t>(StrId::StoreAddonNotConfigured) + 1, "kUiStrings mismatch");

constexpr std::array<LangRow, 6> kPrayerNames = {{
    {L"İmsak",  L"Fajr",    L"الفجر",   L"Fadschr",  L"Fajr",    L"Subuh"},
    {L"Güneş",  L"Sunrise", L"الشروق",  L"Sonne",    L"Lever",   L"Terbit"},
    {L"Öğle",   L"Dhuhr",   L"الظهر",   L"Dhuhr",    L"Dhuhr",   L"Dzuhur"},
    {L"İkindi", L"Asr",     L"العصر",   L"Asr",      L"Asr",     L"Ashar"},
    {L"Akşam",  L"Maghrib", L"المغرب",  L"Maghrib",  L"Maghrib", L"Maghrib"},
    {L"Yatsı",  L"Isha",    L"العشاء",  L"Ischa",    L"Isha",    L"Isya"}
}};

constexpr std::array<LangRow, 6> kPrayerShortCodes = {{
    {L"İm", L"F", L"ف", L"F", L"F", L"Sb"},
    {L"G",  L"S", L"ش", L"S", L"L", L"Tb"},
    {L"Ö",  L"D", L"ظ", L"D", L"D", L"Dz"},
    {L"İ",  L"A", L"ع", L"A", L"A", L"As"},
    {L"A",  L"M", L"م", L"M", L"M", L"Mg"},
    {L"Y",  L"I", L"عش", L"I", L"I", L"Is"}
}};

} // namespace

AppLanguage ResolveEffectiveLanguage(AppLanguage configured) {
    if (configured != AppLanguage::Auto) {
        int val = static_cast<int>(configured);
        if (val >= 1 && val <= 6) return configured;
        return AppLanguage::Turkish;
    }

    LANGID langId = GetUserDefaultUILanguage();
    WORD primary = PRIMARYLANGID(langId);
    switch (primary) {
        case LANG_TURKISH:    return AppLanguage::Turkish;
        case LANG_ARABIC:     return AppLanguage::Arabic;
        case LANG_GERMAN:     return AppLanguage::German;
        case LANG_FRENCH:     return AppLanguage::French;
        case LANG_INDONESIAN: return AppLanguage::Indonesian;
        case LANG_ENGLISH:    return AppLanguage::English;
        default:              return AppLanguage::English;
    }
}

bool IsRtlLanguage(AppLanguage lang) {
    return ResolveEffectiveLanguage(lang) == AppLanguage::Arabic;
}

const wchar_t* GetLanguageDisplayName(AppLanguage lang) {
    switch (lang) {
        case AppLanguage::Auto:       return L"Sistem Dili (Otomatik / Auto)";
        case AppLanguage::Turkish:    return L"Türkçe";
        case AppLanguage::English:    return L"English";
        case AppLanguage::Arabic:     return L"العربية (Arabic)";
        case AppLanguage::German:     return L"Deutsch";
        case AppLanguage::French:     return L"Français";
        case AppLanguage::Indonesian: return L"Bahasa Indonesia";
    }
    return L"Türkçe";
}

const wchar_t* Tr(AppLanguage lang, StrId id) {
    size_t idx = static_cast<size_t>(id);
    constexpr size_t kCount = sizeof(kUiStrings) / sizeof(kUiStrings[0]);
    if (idx >= kCount) return L"";
    return kUiStrings[idx].At(LangIndex(lang));
}

const wchar_t* GetDefaultPrayerName(PrayerKind kind, AppLanguage lang) {
    size_t kIdx = std::clamp<size_t>(static_cast<size_t>(kind), 0, 5);
    return kPrayerNames[kIdx].At(LangIndex(lang));
}

const wchar_t* GetDefaultPrayerShortCode(PrayerKind kind, AppLanguage lang) {
    size_t kIdx = std::clamp<size_t>(static_cast<size_t>(kind), 0, 5);
    return kPrayerShortCodes[kIdx].At(LangIndex(lang));
}

std::wstring GetEffectivePrayerName(PrayerKind kind, const AppSettings& settings) {
    size_t idx = std::clamp<size_t>(static_cast<size_t>(kind), 0, 5);
    std::wstring custom = TrimWide(settings.customPrayerNames[idx]);
    if (!custom.empty()) return custom;
    return GetDefaultPrayerName(kind, settings.language);
}

std::wstring GetEffectivePrayerShortCode(PrayerKind kind, const AppSettings& settings) {
    size_t idx = std::clamp<size_t>(static_cast<size_t>(kind), 0, 5);
    std::wstring customShort = TrimWide(settings.customPrayerShortCodes[idx]);
    if (!customShort.empty()) return customShort;

    std::wstring customName = TrimWide(settings.customPrayerNames[idx]);
    if (!customName.empty()) {
        return customName.substr(0, 1);
    }
    return GetDefaultPrayerShortCode(kind, settings.language);
}

std::wstring GetEffectivePrayerDativeName(PrayerKind kind, const AppSettings& settings) {
    size_t idx = std::clamp<size_t>(static_cast<size_t>(kind), 0, 5);
    AppLanguage eff = ResolveEffectiveLanguage(settings.language);
    std::wstring custom = TrimWide(settings.customPrayerNames[idx]);
    if (custom.empty() && eff == AppLanguage::Turkish) {
        return GetPrayerDativeName(kind);
    }
    if (!custom.empty()) return custom;
    return GetDefaultPrayerName(kind, eff);
}

std::wstring FormatUntilPrayerLine(PrayerKind kind, const AppSettings& settings, bool isRamadan) {
    size_t idx = std::clamp<size_t>(static_cast<size_t>(kind), 0, 5);
    AppLanguage eff = ResolveEffectiveLanguage(settings.language);
    std::wstring custom = TrimWide(settings.customPrayerNames[idx]);
    if (isRamadan && custom.empty()) {
        if (kind == PrayerKind::Aksam) {
            switch (eff) {
                case AppLanguage::Turkish:    return L"İftara kalan";
                case AppLanguage::English:    return L"Until Iftar";
                case AppLanguage::Arabic:     return L"متبقي للإفطار";
                case AppLanguage::German:     return L"Bis Iftar";
                case AppLanguage::French:     return L"Avant l'Iftar";
                case AppLanguage::Indonesian: return L"Menuju Buka";
                default: break;
            }
        } else if (kind == PrayerKind::Imsak) {
            switch (eff) {
                case AppLanguage::Turkish:    return L"Sahura kalan";
                case AppLanguage::English:    return L"Until Suhoor";
                case AppLanguage::Arabic:     return L"متبقي للسحور";
                case AppLanguage::German:     return L"Bis Suhoor";
                case AppLanguage::French:     return L"Avant Souhour";
                case AppLanguage::Indonesian: return L"Menuju Imsak";
                default: break;
            }
        }
    }
    switch (eff) {
        case AppLanguage::Turkish:
            if (custom.empty()) return std::wstring(GetPrayerDativeName(kind)) + L" kalan";
            return custom + L" kalan";
        case AppLanguage::English:
            return L"Until " + GetEffectivePrayerName(kind, settings);
        case AppLanguage::Arabic:
            return L"متبقي لـ " + GetEffectivePrayerName(kind, settings);
        case AppLanguage::German:
            return L"Bis " + GetEffectivePrayerName(kind, settings);
        case AppLanguage::French:
            return L"Avant " + GetEffectivePrayerName(kind, settings);
        case AppLanguage::Indonesian:
            return L"Menuju " + GetEffectivePrayerName(kind, settings);
        default:
            return GetEffectivePrayerName(kind, settings);
    }
}

std::wstring FormatHeroHeading(PrayerKind kind, const AppSettings& settings, bool isRamadan) {
    AppLanguage eff = ResolveEffectiveLanguage(settings.language);
    if (isRamadan && TrimWide(settings.customPrayerNames[static_cast<size_t>(kind)]).empty()) {
        if (kind == PrayerKind::Aksam) {
            switch (eff) {
                case AppLanguage::Turkish:    return L"İFTARA KALAN";
                case AppLanguage::English:    return L"UNTIL IFTAR";
                case AppLanguage::Arabic:     return L"الوقت المتبقي للإفطار";
                case AppLanguage::German:     return L"BIS IFTAR";
                case AppLanguage::French:     return L"AVANT L'IFTAR";
                case AppLanguage::Indonesian: return L"MENUJU BUKA PUASA";
                default: break;
            }
        } else if (kind == PrayerKind::Imsak) {
            switch (eff) {
                case AppLanguage::Turkish:    return L"SAHURA KALAN";
                case AppLanguage::English:    return L"UNTIL SUHOOR";
                case AppLanguage::Arabic:     return L"الوقت المتبقي للسحور";
                case AppLanguage::German:     return L"BIS SUHOOR";
                case AppLanguage::French:     return L"AVANT LE SOUHOUR";
                case AppLanguage::Indonesian: return L"MENUJU IMSAK";
                default: break;
            }
        }
    }
    std::wstring name = GetEffectivePrayerName(kind, settings);
    std::wstring up = name;
    if (eff != AppLanguage::Arabic) {
        CharUpperW(up.data());
    }
    switch (eff) {
        case AppLanguage::Turkish:    return up + L" VAKTİNE";
        case AppLanguage::English:    return L"UNTIL " + up;
        case AppLanguage::Arabic:     return L"الوقت المتبقي · " + name;
        case AppLanguage::German:     return L"BIS " + up;
        case AppLanguage::French:     return L"AVANT " + up;
        case AppLanguage::Indonesian: return L"MENUJU " + up;
        default:                      return up;
    }
}

const wchar_t* GetShortMonthName(int month1To12, AppLanguage lang) {
    int m = std::clamp(month1To12, 1, 12) - 1;
    static constexpr const wchar_t* kTr[12] = {L"Oca", L"Şub", L"Mar", L"Nis", L"May", L"Haz", L"Tem", L"Ağu", L"Eyl", L"Eki", L"Kas", L"Ara"};
    static constexpr const wchar_t* kEn[12] = {L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"};
    static constexpr const wchar_t* kAr[12] = {L"يناير", L"فبراير", L"مارس", L"أبريل", L"مايو", L"يونيو", L"يوليو", L"أغسطس", L"سبتمبر", L"أكتوبر", L"نوفمبر", L"ديسمبر"};
    static constexpr const wchar_t* kDe[12] = {L"Jan", L"Feb", L"Mär", L"Apr", L"Mai", L"Jun", L"Jul", L"Aug", L"Sep", L"Okt", L"Nov", L"Dez"};
    static constexpr const wchar_t* kFr[12] = {L"Jan", L"Fév", L"Mar", L"Avr", L"Mai", L"Jun", L"Jul", L"Aoû", L"Sep", L"Oct", L"Nov", L"Déc"};
    static constexpr const wchar_t* kId[12] = {L"Jan", L"Feb", L"Mar", L"Apr", L"Mei", L"Jun", L"Jul", L"Agu", L"Sep", L"Okt", L"Nov", L"Des"};
    switch (LangIndex(lang)) {
        case 0: return kTr[m];
        case 1: return kEn[m];
        case 2: return kAr[m];
        case 3: return kDe[m];
        case 4: return kFr[m];
        case 5: return kId[m];
    }
    return kTr[m];
}

const wchar_t* GetFullMonthName(int month1To12, AppLanguage lang) {
    int m = std::clamp(month1To12, 1, 12) - 1;
    static constexpr const wchar_t* kTr[12] = {L"Ocak", L"Şubat", L"Mart", L"Nisan", L"Mayıs", L"Haziran", L"Temmuz", L"Ağustos", L"Eylül", L"Ekim", L"Kasım", L"Aralık"};
    static constexpr const wchar_t* kEn[12] = {L"January", L"February", L"March", L"April", L"May", L"June", L"July", L"August", L"September", L"October", L"November", L"December"};
    static constexpr const wchar_t* kAr[12] = {L"يناير", L"فبراير", L"مارس", L"أبريل", L"مايو", L"يونيو", L"يوليو", L"أغسطس", L"سبتمبر", L"أكتوبر", L"نوفمبر", L"ديسمبر"};
    static constexpr const wchar_t* kDe[12] = {L"Januar", L"Februar", L"März", L"April", L"Mai", L"Juni", L"Juli", L"August", L"September", L"Oktober", L"November", L"Dezember"};
    static constexpr const wchar_t* kFr[12] = {L"Janvier", L"Février", L"Mars", L"Avril", L"Mai", L"Juin", L"Juillet", L"Août", L"Septembre", L"Octobre", L"Novembre", L"Décembre"};
    static constexpr const wchar_t* kId[12] = {L"Januari", L"Februari", L"Maret", L"April", L"Mei", L"Juni", L"Juli", L"Agustus", L"September", L"Oktober", L"November", L"Desember"};
    switch (LangIndex(lang)) {
        case 0: return kTr[m];
        case 1: return kEn[m];
        case 2: return kAr[m];
        case 3: return kDe[m];
        case 4: return kFr[m];
        case 5: return kId[m];
    }
    return kTr[m];
}

const wchar_t* GetShortWeekdayName(int dow0SunTo6Sat, AppLanguage lang) {
    int d = std::clamp(dow0SunTo6Sat, 0, 6);
    static constexpr const wchar_t* kTr[7] = {L"Paz", L"Pzt", L"Sal", L"Çar", L"Per", L"Cum", L"Cmt"};
    static constexpr const wchar_t* kEn[7] = {L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat"};
    static constexpr const wchar_t* kAr[7] = {L"أحد", L"إثنين", L"ثلاثاء", L"أربعاء", L"خميس", L"جمعة", L"سبت"};
    static constexpr const wchar_t* kDe[7] = {L"So", L"Mo", L"Di", L"Mi", L"Do", L"Fr", L"Sa"};
    static constexpr const wchar_t* kFr[7] = {L"Dim", L"Lun", L"Mar", L"Mer", L"Jeu", L"Ven", L"Sam"};
    static constexpr const wchar_t* kId[7] = {L"Min", L"Sen", L"Sel", L"Rab", L"Kam", L"Jum", L"Sab"};
    switch (LangIndex(lang)) {
        case 0: return kTr[d];
        case 1: return kEn[d];
        case 2: return kAr[d];
        case 3: return kDe[d];
        case 4: return kFr[d];
        case 5: return kId[d];
    }
    return kTr[d];
}

const wchar_t* GetHijriMonthNameLocalized(int month1To12, AppLanguage lang) {
    int m = std::clamp(month1To12, 1, 12) - 1;
    static constexpr const wchar_t* kTr[12] = {L"Muharrem", L"Safer", L"Rebiülevvel", L"Rebiülahir", L"Cemaziyelevvel", L"Cemaziyelahir", L"Recep", L"Şaban", L"Ramazan", L"Şevval", L"Zilkade", L"Zilhicce"};
    static constexpr const wchar_t* kEn[12] = {L"Muharram", L"Safar", L"Rabi I", L"Rabi II", L"Jumada I", L"Jumada II", L"Rajab", L"Sha'ban", L"Ramadan", L"Shawwal", L"Dhu al-Qi'dah", L"Dhu al-Hijjah"};
    static constexpr const wchar_t* kAr[12] = {L"محرم", L"صفر", L"ربيع الأول", L"ربيع الآخر", L"جمادى الأولى", L"جمادى الآخرة", L"رجب", L"شعبان", L"رمضان", L"شوال", L"ذو القعدة", L"ذو الحجة"};
    static constexpr const wchar_t* kId[12] = {L"Muharram", L"Safar", L"Rabiul Awal", L"Rabiul Akhir", L"Jumadil Awal", L"Jumadil Akhir", L"Rajab", L"Sya'ban", L"Ramadhan", L"Syawal", L"Dzulkaidah", L"Dzulhijjah"};
    size_t li = LangIndex(lang);
    if (li == 0) return kTr[m];
    if (li == 2) return kAr[m];
    if (li == 5) return kId[m];
    return kEn[m];
}

std::wstring FormatHijriDateLocalized(DateYMD date, AppLanguage lang) {
    HijriDateYMD h = GregorianToHijri(date);
    wchar_t buf[64];
    swprintf_s(buf, L"%d %ls %d", h.day, GetHijriMonthNameLocalized(h.month, lang), h.year);
    return std::wstring(buf);
}

const wchar_t* GetCompassDirectionLocalized(double bearingDegrees, AppLanguage lang) {
    double normalized = std::fmod(std::fmod(bearingDegrees, 360.0) + 360.0, 360.0);
    int idx = static_cast<int>(std::llround(normalized / 45.0)) % 8;
    static constexpr const wchar_t* kTr[8] = {L"K", L"KD", L"D", L"GD", L"G", L"GB", L"B", L"KB"};
    static constexpr const wchar_t* kEn[8] = {L"N", L"NE", L"E", L"SE", L"S", L"SW", L"W", L"NW"};
    static constexpr const wchar_t* kAr[8] = {L"ش", L"ش.ق", L"ق", L"ج.ق", L"ج", L"ج.غ", L"غ", L"ش.غ"};
    static constexpr const wchar_t* kDe[8] = {L"N", L"NO", L"O", L"SO", L"S", L"SW", L"W", L"NW"};
    static constexpr const wchar_t* kId[8] = {L"U", L"TL", L"T", L"TG", L"S", L"BD", L"B", L"BL"};
    switch (LangIndex(lang)) {
        case 0: return kTr[idx];
        case 2: return kAr[idx];
        case 3: return kDe[idx];
        case 5: return kId[idx];
        default: return kEn[idx];
    }
}

std::wstring FormatQiblaLabelLocalized(double latitude, double longitude, AppLanguage lang) {
    double bearing = CalculateQiblaBearing(latitude, longitude);
    int rounded = static_cast<int>(std::llround(bearing)) % 360;
    const wchar_t* dir = GetCompassDirectionLocalized(bearing, lang);
    const wchar_t* prefix = L"Qibla";
    switch (LangIndex(lang)) {
        case 0: prefix = L"Kıble"; break;
        case 1: prefix = L"Qibla"; break;
        case 2: prefix = L"القبلة"; break;
        case 3: prefix = L"Qibla"; break;
        case 4: prefix = L"Qibla"; break;
        case 5: prefix = L"Kiblat"; break;
    }
    wchar_t buf[64];
    swprintf_s(buf, L"%ls %d° %ls", prefix, rounded, dir);
    return std::wstring(buf);
}

const wchar_t* GetStyleFriendlyNameLocalized(TaskbarDisplayStyle style, AppLanguage lang) {
    static constexpr LangRow kNames[9] = {
        {L"İki Satır Klasik (İkindiye kalan / 23 dk)", L"Two-Line Classic (Until Asr / 23 min)", L"سطرين كلاسيكي", L"Zweizeilig Klassisch", L"Deux lignes classique", L"Dua Baris Klasik"},
        {L"Kompakt Ok (İ→23Dk)", L"Compact Arrow (A→23m)", L"سهم مضغوط", L"Kompakt Pfeil", L"Flèche compacte", L"Panah Ringkas"},
        {L"Aralıklı Ok (İ → 23 dk)", L"Spaced Arrow (A → 23 min)", L"سهم متباعد", L"Pfeil mit Abstand", L"Flèche espacée", L"Panah Berjarak"},
        {L"Saatli İki Satır (İkindi · 15:25 / 23 dk)", L"Two-Line + Time (Asr · 15:25 / 23 min)", L"سطرين مع الوقت", L"Zweizeilig + Zeit", L"Deux lignes + Heure", L"Dua Baris + Jam"},
        {L"İkon + Sayaç (◐ 23 dk)", L"Icon + Countdown", L"أيقونة + عداد", L"Symbol + Zähler", L"Icône + Compte", L"Ikon + Hitung Mundur"},
        {L"Kısa İki Satır (İkindi / 23dk)", L"Short Two-Line", L"سطرين قصير", L"Kurz Zweizeilig", L"Deux lignes court", L"Dua Baris Singkat"},
        {L"Minimal Nokta Sayaç (● 00:23)", L"Minimal Dot Timer (● 00:23)", L"مؤقت بنقطة", L"Minimal Punkt-Timer", L"Point minimal", L"Timer Titik Minimal"},
        {L"Vakit Saati (İkindi / 15:25)", L"Prayer + Time (Asr / 15:25)", L"الاسم + الوقت", L"Gebet + Zeit", L"Prière + Heure", L"Sholat + Jam"},
        {L"Özel Şablon", L"Custom Template", L"قالب مخصص", L"Eigene Vorlage", L"Modèle personnalisé", L"Templat Kustom"}
    };
    int idx = std::clamp(static_cast<int>(style), 0, 8);
    return kNames[idx].At(LangIndex(lang));
}

const wchar_t* GetFlyoutDesignFriendlyName(FlyoutDesignStyle design, AppLanguage lang) {
    static constexpr LangRow kNames[4] = {
        {L"Kompakt Fluent (Windows 11 Klasik)", L"Compact Fluent (Win11 Classic)", L"فلوينت المدمج (ويندوز 11)", L"Kompakt Fluent (Win11)", L"Fluent Compact (Win11)", L"Fluent Ringkas (Win11)"},
        {L"Minimal İnce Liste (Cam)", L"Minimal Sleek List", L"قائمة بسيطة فائقة النعومة", L"Minimalistisch Schlicht", L"Liste Épurée Minimal", L"Minimalis Elegan"},
        {L"Modern 2×3 Kart Izgarası (Bento)", L"Modern 2×3 Bento Cards", L"بطاقات شبكية حديثة 2×3", L"2×3 Bento-Karten", L"Grille de cartes 2×3", L"Kartu Bento 2×3"},
        {L"Zaman Çizelgesi + Kıble Pusulası", L"Timeline + Qibla Compass", L"الخط الزمني + بوصلة القبلة", L"Zeitstrahl + Qibla-Kompass", L"Chronologie + Boussole Qibla", L"Linimasa + Kompas Kiblat"}
    };
    int idx = std::clamp(static_cast<int>(design), 0, 3);
    return kNames[idx].At(LangIndex(lang));
}

std::wstring FormatNotificationAtTimeTitle(PrayerKind kind, const AppSettings& settings) {
    std::wstring name = GetEffectivePrayerName(kind, settings);
    switch (LangIndex(settings.language)) {
        case 0: return name + L" Vakti Girdi";
        case 1: return name + L" Prayer Time";
        case 2: return L"حان الآن وقت " + name;
        case 3: return name + L"-Gebetszeit erreicht";
        case 4: return L"Heure de " + name;
        case 5: return L"Waktu " + name + L" Telah Tiba";
    }
    return name;
}

std::wstring FormatNotificationAtTimeBody(const std::wstring& cityName, PrayerKind kind, std::wstring_view hhmm, const AppSettings& settings) {
    std::wstring name = GetEffectivePrayerName(kind, settings);
    std::wstring lower = (LangIndex(settings.language) == 0) ? NormalizeTurkishLower(name) : name;
    switch (LangIndex(settings.language)) {
        case 0: return cityName + L" için " + lower + L" vakti (" + std::wstring(hhmm) + L").";
        case 1: return L"It is " + name + L" time (" + std::wstring(hhmm) + L") in " + cityName + L".";
        case 2: return L"وقت " + name + L" (" + std::wstring(hhmm) + L") في " + cityName + L".";
        case 3: return name + L" (" + std::wstring(hhmm) + L") in " + cityName + L".";
        case 4: return L"Il est l'heure de " + name + L" (" + std::wstring(hhmm) + L") à " + cityName + L".";
        case 5: return L"Waktu " + name + L" (" + std::wstring(hhmm) + L") untuk " + cityName + L".";
    }
    return cityName + L" · " + std::wstring(hhmm);
}

std::wstring FormatNotificationPreTitle(PrayerKind kind, int minutes, const AppSettings& settings) {
    wchar_t buf[160];
    std::wstring name = GetEffectivePrayerName(kind, settings);
    std::wstring dative = GetEffectivePrayerDativeName(kind, settings);
    switch (LangIndex(settings.language)) {
        case 0:
            swprintf_s(buf, L"%ls %d dakika kaldı", dative.c_str(), minutes);
            break;
        case 1:
            swprintf_s(buf, L"%d minutes until %ls", minutes, name.c_str());
            break;
        case 2:
            swprintf_s(buf, L"متبقي %d دقيقة على %ls", minutes, name.c_str());
            break;
        case 3:
            swprintf_s(buf, L"Noch %d Minuten bis %ls", minutes, name.c_str());
            break;
        case 4:
            swprintf_s(buf, L"%d minutes avant %ls", minutes, name.c_str());
            break;
        case 5:
            swprintf_s(buf, L"%d menit menuju %ls", minutes, name.c_str());
            break;
    }
    return std::wstring(buf);
}

std::wstring FormatNotificationPreBody(PrayerKind kind, std::wstring_view hhmm, const std::wstring& cityName, const AppSettings& settings) {
    std::wstring name = GetEffectivePrayerName(kind, settings);
    return name + L": " + std::wstring(hhmm) + L" · " + cityName;
}

} // namespace vakit
