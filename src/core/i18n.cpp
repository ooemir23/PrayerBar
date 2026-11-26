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
    if (idx < 0 || idx > 11) return 0;
    return static_cast<size_t>(idx);
}

struct LangRow {
    const wchar_t* tr;
    const wchar_t* en;
    const wchar_t* ar;
    const wchar_t* de;
    const wchar_t* fr;
    const wchar_t* id;
    const wchar_t* ur = nullptr;
    const wchar_t* bn = nullptr;
    const wchar_t* ms = nullptr;
    const wchar_t* fa = nullptr;
    const wchar_t* ru = nullptr;
    const wchar_t* es = nullptr;

    const wchar_t* At(size_t i) const {
        const wchar_t* v = nullptr;
        switch (i) {
            case 0: v = tr; break;
            case 1: v = en; break;
            case 2: v = ar; break;
            case 3: v = de; break;
            case 4: v = fr; break;
            case 5: v = id; break;
            case 6: v = ur; break;
            case 7: v = bn; break;
            case 8: v = ms; break;
            case 9: v = fa; break;
            case 10: v = ru; break;
            case 11: v = es; break;
        }
        return v ? v : en;
    }
};

constexpr LangRow kUiStrings[] = {
    // ToggleFlyout
    {L"Vakitleri Göster / Gizle", L"Show / Hide Prayer Times", L"إظهار / إخفاء المواقيت", L"Gebetszeiten ein-/ausblenden", L"Afficher / Masquer les horaires", L"Tampilkan / Sembunyikan Jadwal", L"نماز کے اوقات دکھائیں / چھپائیں", L"নামাজের সময় দেখান / লুকান", L"Tunjuk / Sembunyi Waktu Solat", L"نمایش / پنهان کردن اوقات نماز", L"Показать / скрыть время намаза", L"Mostrar / ocultar horarios de oración"},
    // OpenImsakiye
    {L"Aylık İmsakiye...", L"Monthly Calendar (Imsakiye)...", L"الإمساكية الشهرية...", L"Monatskalender (Imsakiye)...", L"Calendrier mensuel...", L"Jadwal Imsakiyah Bulanan...", L"ماہانہ کیلنڈر (امساکیہ)...", L"মাসিক ক্যালেন্ডার (ইমসাকিয়া)...", L"Kalendar Bulanan (Imsakiah)...", L"تقویم ماهانه (امساکیه)...", L"Календарь на месяц (Имсакия)...", L"Calendario mensual (Imsakiye)..."},
    // TaskbarStyleMenu
    {L"Görev Çubuğu Görünümü", L"Taskbar Display Style", L"نمط عرض شريط المهام", L"Taskleisten-Darstellung", L"Style dans la barre des tâches", L"Gaya Tampilan Taskbar", L"ٹاسک بار ڈسپلے انداز", L"টাস্কবার প্রদর্শন শৈলী", L"Gaya Paparan Bar Tugas", L"سبک نمایش نوار وظیفه", L"Стиль отображения на панели задач", L"Estilo en la barra de tareas"},
    // FlyoutDesignMenu
    {L"Açılır Pencere Tasarımı", L"Flyout Design", L"تصميم النافذة المنبثقة", L"Flyout-Design", L"Design du panneau", L"Desain Panel Popup", L"پاپ اپ ڈیزائن", L"পপআপ ডিজাইন", L"Reka Bentuk Panel", L"طراحی پنجره بازشو", L"Дизайн всплывающей панели", L"Diseño del panel"},
    // ShowInTrayMenu
    {L"Sistem Tepsisinde Göster", L"Show in System Tray", L"إظهار في علبة النظام", L"Im Infobereich anzeigen", L"Afficher dans la zone de notification", L"Tampilkan di System Tray", L"سسٹم ٹرے میں دکھائیں", L"সিস্টেম ট্রেতে দেখান", L"Tunjuk dalam Dulang Sistem", L"نمایش در سینی سیستم", L"Показывать в области уведомлений", L"Mostrar en la bandeja del sistema"},
    // ShowInTaskbarMenu
    {L"Görev Çubuğunda Göster", L"Show on Taskbar", L"إظهار في شريط المهام", L"In der Taskleiste anzeigen", L"Afficher dans la barre des tâches", L"Tampilkan di Taskbar", L"ٹاسک بار پر دکھائیں", L"টাস্কবারে দেখান", L"Tunjuk pada Bar Tugas", L"نمایش در نوار وظیفه", L"Показывать на панели задач", L"Mostrar en la barra de tareas"},
    // RefreshNow
    {L"Vakitleri Şimdi Güncelle", L"Refresh Prayer Times", L"تحديث المواقيت الآن", L"Zeiten jetzt aktualisieren", L"Actualiser les horaires", L"Perbarui Jadwal Sekarang", L"نماز کے اوقات تازہ کریں", L"নামাজের সময় রিফ্রেশ করুন", L"Muat Semula Waktu Solat", L"به‌روزرسانی اوقات نماز", L"Обновить время намаза", L"Actualizar horarios"},
    // SettingsEllipsis
    {L"Ayarlar...", L"Settings...", L"الإعدادات...", L"Einstellungen...", L"Paramètres...", L"Pengaturan...", L"ترتیبات...", L"সেটিংস...", L"Tetapan...", L"تنظیمات...", L"Настройки...", L"Configuración..."},
    // ExitApp
    {L"Çıkış", L"Exit", L"خروج", L"Beenden", L"Quitter", L"Keluar", L"بند کریں", L"বন্ধ করুন", L"Keluar", L"خروج", L"Выход", L"Salir"},
    // StopSoundMenu
    {L"■ Ezanı / Sesi Durdur", L"■ Stop Adhan / Audio", L"■ إيقاف الأذان / الصوت", L"■ Adhan / Ton stoppen", L"■ Arrêter l'Adhan / le son", L"■ Hentikan Adzan / Suara", L"■ اذان / آواز بند کریں", L"■ আজান / শব্দ বন্ধ করুন", L"■ Hentikan Azan / Audio", L"■ توقف اذان / صدا", L"■ Остановить азан / звук", L"■ Detener adhan / audio"},

    // BadgeNext
    {L"Sıradaki", L"Next", L"التالي", L"Nächstes", L"Suivant", L"Berikutnya", L"اگلی", L"পরবর্তী", L"Seterusnya", L"بعدی", L"Далее", L"Siguiente"},
    // BadgeCurrent
    {L"Şu an", L"Now", L"الآن", L"Jetzt", L"Actuel", L"Saat ini", L"ابھی", L"এখন", L"Sekarang", L"اکنون", L"Сейчас", L"Ahora"},
    // BadgeJumuah
    {L"Cuma", L"Jumu'ah", L"الجمعة", L"Dschuma", L"Joumou'a", L"Jumat", L"جمعہ", L"জুমা", L"Jumaat", L"جمعه", L"Джума", L"Yumu'a"},
    // PrayerTimeHeader
    {L"Vakit saati", L"Prayer time", L"وقت الصلاة", L"Gebetszeit", L"Heure de prière", L"Waktu sholat", L"نماز کا وقت", L"নামাজের সময়", L"Waktu solat", L"وقت نماز", L"Время намаза", L"Hora de oración"},
    // KerahatAfterSunrise
    {L"Kerahat · İşrak öncesi", L"Makruh · After sunrise", L"وقت كراهة · بعد الشروق", L"Karaha · Nach Sonnenaufgang", L"Karaha · Après le lever", L"Waktu Terlarang · Terbit", L"مکروہ · طلوع آفتاب کے بعد", L"মাকরুহ · সূর্যোদয়ের পর", L"Makruh · Selepas syuruk", L"مکروه · پس از طلوع آفتاب", L"Макрух · После восхода", L"Makruh · Tras el amanecer"},
    // KerahatBeforeDhuhr
    {L"Kerahat · İstiva öncesi", L"Makruh · Zenith before Dhuhr", L"وقت كراهة · قبل الزوال", L"Karaha · Vor Dhuhr", L"Karaha · Avant Dhuhr", L"Waktu Terlarang · Zawal", L"مکروہ · ظہر سے پہلے زوال", L"মাকরুহ · জোহরের আগে মধ্যাহ্ন", L"Makruh · Rembang sebelum Zohor", L"مکروه · زوال پیش از ظهر", L"Макрух · Зенит перед Зухром", L"Makruh · Cénit antes de Dhuhr"},
    // KerahatBeforeMaghrib
    {L"Kerahat · İsfirar öncesi", L"Makruh · Before Maghrib", L"وقت كراهة · قبل الغروب", L"Karaha · Vor Maghrib", L"Karaha · Avant Maghrib", L"Waktu Terlarang · Jelang Maghrib", L"مکروہ · مغرب سے پہلے", L"মাকরুহ · মাগরিবের আগে", L"Makruh · Sebelum Maghrib", L"مکروه · پیش از مغرب", L"Макрух · Перед Магрибом", L"Makruh · Antes de Maghrib"},
    // SourcePrefix
    {L"Kaynak: ", L"Source: ", L"المصدر: ", L"Quelle: ", L"Source : ", L"Sumber: ", L"ماخذ: ", L"উৎস: ", L"Sumber: ", L"منبع: ", L"Источник: ", L"Fuente: "},
    // SourceLocalCalc
    {L"Yerel Hesaplama", L"Offline Calculation", L"حساب محلي", L"Offline-Berechnung", L"Calcul hors ligne", L"Perhitungan Lokal", L"آف لائن حساب", L"অফলাইন গণনা", L"Pengiraan Luar Talian", L"محاسبه آفلاین", L"Офлайн-расчёт", L"Cálculo sin conexión"},
    // TipStopAudio
    {L"Sesi Durdur", L"Stop Audio", L"إيقاف الصوت", L"Ton stoppen", L"Arrêter le son", L"Hentikan Suara", L"آواز بند کریں", L"শব্দ বন্ধ করুন", L"Hentikan Audio", L"توقف صدا", L"Остановить звук", L"Detener audio"},
    // TipImsakiye
    {L"Aylık İmsakiye", L"Monthly Calendar", L"الإمساكية الشهرية", L"Monatskalender", L"Calendrier mensuel", L"Imsakiyah Bulanan", L"ماہانہ کیلنڈر", L"মাসিক ক্যালেন্ডার", L"Kalendar Bulanan", L"تقویم ماهانه", L"Календарь на месяц", L"Calendario mensual"},
    // TipRefresh
    {L"Yenile", L"Refresh", L"تحديث", L"Aktualisieren", L"Actualiser", L"Segarkan", L"تازہ کریں", L"রিফ্রেশ", L"Muat Semula", L"به‌روزرسانی", L"Обновить", L"Actualizar"},
    // TipSettings
    {L"Ayarlar", L"Settings", L"الإعدادات", L"Einstellungen", L"Paramètres", L"Pengaturan", L"ترتیبات", L"সেটিংস", L"Tetapan", L"تنظیمات", L"Настройки", L"Configuración"},

    // TestNotifTitle
    {L"PrayerBar — Bildirim Testi", L"PrayerBar — Notification Test", L"PrayerBar — اختبار الإشعارات", L"PrayerBar — Benachrichtigungstest", L"PrayerBar — Test de notification", L"PrayerBar — Uji Notifikasi", L"PrayerBar — اطلاع کی جانچ", L"PrayerBar — বিজ্ঞপ্তি পরীক্ষা", L"PrayerBar — Ujian Pemberitahuan", L"PrayerBar — آزمایش اعلان", L"PrayerBar — Проверка уведомлений", L"PrayerBar — Prueba de notificación"},
    // TestNotifBody
    {L"Vaktinden önce ve vaktinde Windows bildirimleri aktif.", L"Pre-prayer and on-time Windows notifications are active.", L"إشعارات ويندوز قبل الوقت وعند دخول الوقت مفعّلة.", L"Windows-Benachrichtigungen vor und zur Gebetszeit aktiv.", L"Les notifications Windows avant et à l'heure sont actives.", L"Notifikasi Windows sebelum dan tepat waktu aktif.", L"نماز سے پہلے اور وقت پر ونڈوز اطلاعات فعال ہیں۔", L"নামাজের আগে ও সময়মতো উইন্ডোজ বিজ্ঞপ্তি চালু আছে।", L"Pemberitahuan Windows sebelum dan tepat waktu solat aktif.", L"اعلان‌های ویندوز پیش از نماز و سر وقت فعال هستند.", L"Уведомления Windows перед намазом и в его время включены.", L"Las notificaciones de Windows previas y a la hora están activas."},

    // ImsakiyeWindowTitle
    {L"PrayerBar — Aylık ve Ramazan İmsakiyesi", L"PrayerBar — Monthly & Ramadan Prayer Schedule", L"PrayerBar — الإمساكية الشهرية ورمضان", L"PrayerBar — Monats- & Ramadan-Gebetsplan", L"PrayerBar — Calendrier mensuel & Ramadan", L"PrayerBar — Jadwal Sholat Bulanan & Ramadan", L"PrayerBar — ماہانہ اور رمضان نظام الاوقات", L"PrayerBar — মাসিক ও রমজানের সময়সূচি", L"PrayerBar — Jadual Bulanan & Ramadan", L"PrayerBar — جدول ماهانه و رمضان", L"PrayerBar — Расписание на месяц и Рамадан", L"PrayerBar — Horario mensual y de Ramadán"},
    // BtnToday
    {L"Bugün", L"Today", L"اليوم", L"Heute", L"Aujourd'hui", L"Hari Ini", L"آج", L"আজ", L"Hari Ini", L"امروز", L"Сегодня", L"Hoy"},
    // BtnRamadan
    {L"Ramazan İmsakiyesi", L"Ramadan Schedule", L"إمساكية رمضان", L"Ramadan-Plan", L"Calendrier Ramadan", L"Jadwal Ramadan", L"رمضان نظام الاوقات", L"রমজানের সময়সূচি", L"Jadual Ramadan", L"جدول رمضان", L"Расписание Рамадана", L"Horario de Ramadán"},
    // BtnMonthly
    {L"Aylık Görünüm", L"Monthly View", L"عرض شهري", L"Monatsansicht", L"Vue mensuelle", L"Tampilan Bulanan", L"ماہانہ منظر", L"মাসিক দৃশ্য", L"Paparan Bulanan", L"نمای ماهانه", L"Месяц", L"Vista mensual"},
    // BtnSync
    {L"Eşitle", L"Sync", L"مزامنة", L"Synchronisieren", L"Synchroniser", L"Sinkronkan", L"ہم آہنگ کریں", L"সিঙ্ক", L"Segerak", L"همگام‌سازی", L"Синхронизировать", L"Sincronizar"},
    // BtnExportCsv
    {L"CSV İndir", L"Export CSV", L"تصدير CSV", L"CSV exportieren", L"Exporter CSV", L"Ekspor CSV", L"CSV برآمد کریں", L"CSV রপ্তানি", L"Eksport CSV", L"خروجی CSV", L"Экспорт CSV", L"Exportar CSV"},
    // BtnExportHtml
    {L"HTML / Yazdır", L"HTML / Print", L"HTML / طباعة", L"HTML / Drucken", L"HTML / Imprimer", L"HTML / Cetak", L"HTML / پرنٹ", L"HTML / প্রিন্ট", L"HTML / Cetak", L"HTML / چاپ", L"HTML / Печать", L"HTML / Imprimir"},
    // ColDate
    {L"Tarih", L"Date", L"التاريخ", L"Datum", L"Date", L"Tanggal", L"تاریخ", L"তারিখ", L"Tarikh", L"تاریخ", L"Дата", L"Fecha"},
    // ColHijri
    {L"Hicri", L"Hijri", L"هجري", L"Hidschri", L"Hégirien", L"Hijriah", L"ہجری", L"হিজরি", L"Hijrah", L"قمری", L"Хиджра", L"Hégira"},
    // RamadanTitlePrefix
    {L"Ramazan İmsakiyesi", L"Ramadan Schedule", L"إمساكية رمضان", L"Ramadan-Zeitplan", L"Calendrier du Ramadan", L"Jadwal Ramadan", L"رمضان نظام الاوقات", L"রমজানের সময়সূচি", L"Jadual Ramadan", L"جدول رمضان", L"Расписание Рамадана", L"Horario de Ramadán"},
    // RamadanDayPrefix
    {L"Ramazan", L"Ramadan", L"رمضان", L"Ramadan", L"Ramadan", L"Ramadan", L"رمضان", L"রমজান", L"Ramadan", L"رمضان", L"Рамадан", L"Ramadán"},
    // StatusExportSaved
    {L"Dışa aktarıldı: ", L"Exported: ", L"تم التصدير: ", L"Exportiert: ", L"Exporté : ", L"Diekspor: ", L"برآمد ہو گیا: ", L"রপ্তানি হয়েছে: ", L"Dieksport: ", L"ذخیره شد: ", L"Экспортировано: ", L"Exportado: "},
    // StatusSyncDone
    {L"İmsakiye güncellendi.", L"Schedule updated.", L"تم تحديث الإمساكية.", L"Zeitplan aktualisiert.", L"Calendrier mis à jour.", L"Jadwal diperbarui.", L"نظام الاوقات اپ ڈیٹ ہو گیا۔", L"সময়সূচি হালনাগাদ হয়েছে।", L"Jadual dikemas kini.", L"جدول به‌روز شد.", L"Расписание обновлено.", L"Horario actualizado."},
    // StatusSyncOffline
    {L"İnternet yok — çevrimdışı hesaplama gösteriliyor.", L"Offline — showing local astronomical calculation.", L"غير متصل — يتم عرض الحساب الفلكي المحلي.", L"Offline — lokale Berechnung wird angezeigt.", L"Hors ligne — calcul local affiché.", L"Offline — menampilkan perhitungan lokal.", L"آف لائن — مقامی فلکیاتی حساب دکھایا جا رہا ہے۔", L"অফলাইন — স্থানীয় জ্যোতির্বিদ্যা গণনা দেখানো হচ্ছে।", L"Luar talian — memaparkan pengiraan astronomi tempatan.", L"آفلاین — محاسبه نجومی محلی نمایش داده می‌شود.", L"Нет сети — показан локальный астрономический расчёт.", L"Sin conexión — se muestra el cálculo astronómico local."},
    // ImsakiyeFooterHint
    {L"Bugün vurgulu, Cuma altın rozetli · Ramazan modunda 30 günlük tam liste gösterilir.", L"Today highlighted, Friday gold badge · Ramadan view displays full 30-day schedule.", L"اليوم محدّد والجمعة مميّزة · وضع رمضان يعرض 30 يوماً كاملاً.", L"Heute hervorgehoben, Freitag gold · Ramadan-Ansicht zeigt 30 Tage.", L"Aujourd'hui surligné, vendredi doré · Vue Ramadan sur 30 jours.", L"Hari ini ditandai, Jumat emas · Mode Ramadan tampilkan 30 hari penuh.", L"آج نمایاں، جمعہ سنہری نشان · رمضان منظر مکمل 30 دن دکھاتا ہے۔", L"আজ চিহ্নিত, শুক্রবার সোনালি ব্যাজ · রমজান দৃশ্যে পূর্ণ ৩০ দিনের সূচি।", L"Hari ini diserlahkan, Jumaat lencana emas · Paparan Ramadan menunjukkan jadual 30 hari.", L"امروز برجسته، جمعه با نشان طلایی · نمای رمضان جدول کامل ۳۰ روزه را نشان می‌دهد.", L"Сегодня выделено, пятница — золотой значок · Режим Рамадана показывает все 30 дней.", L"Hoy resaltado, viernes con insignia dorada · La vista de Ramadán muestra los 30 días."},

    // SettingsWindowTitle
    {L"PrayerBar — Ayarlar", L"PrayerBar — Settings", L"PrayerBar — الإعدادات", L"PrayerBar — Einstellungen", L"PrayerBar — Paramètres", L"PrayerBar — Pengaturan", L"PrayerBar — ترتیبات", L"PrayerBar — সেটিংস", L"PrayerBar — Tetapan", L"PrayerBar — تنظیمات", L"PrayerBar — Настройки", L"PrayerBar — Configuración"},
    // TabAppearance
    {L"Görünüm", L"Appearance", L"المظهر", L"Darstellung", L"Apparence", L"Tampilan", L"ظاہری شکل", L"চেহারা", L"Penampilan", L"ظاهر", L"Внешний вид", L"Apariencia"},
    // TabLocation
    {L"Konum & Vakitler", L"Location & Times", L"الموقع والمواقيت", L"Ort & Zeiten", L"Lieu & Horaires", L"Lokasi & Waktu", L"مقام اور اوقات", L"অবস্থান ও সময়", L"Lokasi & Waktu", L"مکان و اوقات", L"Место и время", L"Ubicación y horarios"},
    // TabNotifications
    {L"Bildirim & Ses", L"Alerts & Audio", L"التنبيهات والصوت", L"Hinweise & Ton", L"Alertes & Son", L"Notifikasi & Suara", L"اطلاعات اور آواز", L"সতর্কতা ও শব্দ", L"Peringatan & Audio", L"هشدار و صدا", L"Оповещения и звук", L"Avisos y audio"},
    // TabLanguageSystem
    {L"Dil & Sistem", L"Language & System", L"اللغة والنظام", L"Sprache & System", L"Langue & Système", L"Bahasa & Sistem", L"زبان اور سسٹم", L"ভাষা ও সিস্টেম", L"Bahasa & Sistem", L"زبان و سیستم", L"Язык и система", L"Idioma y sistema"},
    // LivePreviewLabel
    {L"Canlı Görev Çubuğu Önizlemesi", L"Live Taskbar Preview", L"معاينة شريط المهام", L"Live-Taskleistenvorschau", L"Aperçu barre des tâches", L"Pratinjau Taskbar", L"ٹاسک بار کا براہ راست پیش منظر", L"টাস্কবারের লাইভ প্রিভিউ", L"Pratonton Langsung Bar Tugas", L"پیش‌نمایش زنده نوار وظیفه", L"Предпросмотр панели задач", L"Vista previa de la barra de tareas"},
    // FooterAutoSave
    {L"Değişiklikler anında kaydedilir ve uygulanır.", L"Changes are saved and applied automatically.", L"يتم حفظ التغييرات وتطبيقها تلقائياً.", L"Änderungen werden sofort gespeichert.", L"Modifications enregistrées automatiquement.", L"Perubahan disimpan otomatis.", L"تبدیلیاں خود بخود محفوظ اور لاگو ہوتی ہیں۔", L"পরিবর্তন স্বয়ংক্রিয়ভাবে সংরক্ষিত ও প্রয়োগ হয়।", L"Perubahan disimpan dan digunakan secara automatik.", L"تغییرات به‌طور خودکار ذخیره و اعمال می‌شوند.", L"Изменения сохраняются и применяются автоматически.", L"Los cambios se guardan y aplican automáticamente."},
    // BtnOk
    {L"Tamam", L"OK", L"موافق", L"OK", L"OK", L"Selesai", L"ٹھیک ہے", L"ঠিক আছে", L"OK", L"تأیید", L"ОК", L"Aceptar"},

    // LblTaskbarStyle
    {L"Görev Çubuğu Stili:", L"Taskbar Display Style:", L"نمط شريط المهام:", L"Taskleisten-Stil:", L"Style barre des tâches :", L"Gaya Taskbar:", L"ٹاسک بار ڈسپلے انداز:", L"টাস্কবার প্রদর্শন শৈলী:", L"Gaya Paparan Bar Tugas:", L"سبک نمایش نوار وظیفه:", L"Стиль на панели задач:", L"Estilo en la barra de tareas:"},
    // LblCustomTemplate
    {L"Özel Şablon (\\n alt satır):", L"Custom Template (\\n line 2):", L"قالب مخصص (\\n سطر جديد):", L"Vorlage (\\n neue Zeile):", L"Modèle (\\n ligne 2) :", L"Templat Kustom (\\n baris 2):", L"حسب ضرورت ٹیمپلیٹ (\\n دوسری لائن):", L"কাস্টম টেমপ্লেট (\\n দ্বিতীয় লাইন):", L"Templat Tersuai (\\n baris 2):", L"قالب سفارشی (\\n خط دوم):", L"Свой шаблон (\\n — вторая строка):", L"Plantilla personalizada (\\n línea 2):"},
    // LblFlyoutDesign
    {L"Açılır Ekran Tasarımı:", L"Flyout Panel Design:", L"تصميم النافذة المنبثقة:", L"Flyout-Design:", L"Design du panneau :", L"Desain Panel Popup:", L"پاپ اپ پینل ڈیزائن:", L"পপআপ প্যানেল ডিজাইন:", L"Reka Bentuk Panel:", L"طراحی پنل بازشو:", L"Дизайн панели:", L"Diseño del panel:"},
    // LblDockPos
    {L"Görev Çubuğu Konumu:", L"Taskbar Position:", L"موقع شريط المهام:", L"Taskleisten-Position:", L"Position barre des tâches :", L"Posisi Taskbar:", L"ٹاسک بار پر مقام:", L"টাস্কবারে অবস্থান:", L"Kedudukan Bar Tugas:", L"موقعیت در نوار وظیفه:", L"Положение на панели:", L"Posición en la barra:"},
    // DockRightTray
    {L"Sağ — Sistem tepsisi yanı (^ solunda)", L"Right — Near system tray (left of ^)", L"يمين — بجانب علبة النظام", L"Rechts — Neben Infobereich", L"Droite — Près de la zone notif.", L"Kanan — Dekat System Tray", L"دائیں — سسٹم ٹرے کے قریب (^ کے بائیں)", L"ডান — সিস্টেম ট্রের কাছে (^ এর বাঁয়ে)", L"Kanan — Dekat dulang sistem (kiri ^)", L"راست — کنار سینی سیستم (چپ ^)", L"Справа — у области уведомлений (слева от ^)", L"Derecha — Junto a la bandeja (izq. de ^)"},
    // DockFarRight
    {L"En Sağ — Saatin sağı / sağ köşe", L"Far Right — Right edge / over clock", L"أقصى اليمين — فوق الساعة", L"Ganz rechts — Über der Uhr", L"Extrême droite — Sur l'horloge", L"Paling Kanan — Ujung jam", L"بالکل دائیں — دائیں کنارہ / گھڑی پر", L"সবচেয়ে ডানে — ডান প্রান্ত / ঘড়ির ওপর", L"Paling Kanan — Tepi kanan / atas jam", L"منتهی‌الیه راست — لبه راست / روی ساعت", L"Крайний правый — у часов", L"Extremo derecho — Borde / sobre el reloj"},
    // DockCenter
    {L"Orta — Görev çubuğu ortası", L"Center — Taskbar middle", L"وسط — منتصف شريط المهام", L"Mitte — Taskleisten-Mitte", L"Centre — Milieu de la barre", L"Tengah — Tengah Taskbar", L"درمیان — ٹاسک بار کا وسط", L"মাঝখানে — টাস্কবারের মাঝে", L"Tengah — Tengah bar tugas", L"وسط — میانه نوار وظیفه", L"По центру панели задач", L"Centro — Mitad de la barra"},
    // DockLeft
    {L"En Sol — Görev çubuğu solu", L"Far Left — Taskbar left side", L"أقصى اليسار — يسار الشريط", L"Ganz links — Linke Seite", L"Extrême gauche — Côté gauche", L"Paling Kiri — Sisi kiri", L"بالکل بائیں — ٹاسک بار کی بائیں جانب", L"সবচেয়ে বাঁয়ে — টাস্কবারের বাঁ দিক", L"Paling Kiri — Sebelah kiri bar tugas", L"منتهی‌الیه چپ — سمت چپ نوار وظیفه", L"Крайний левый край панели", L"Extremo izquierdo — Lado izquierdo"},
    // LblOffset
    {L"Yatay İnce Kaydırma:", L"Horizontal Offset:", L"الإزاحة الأفقية:", L"Horizontaler Versatz:", L"Décalage horizontal :", L"Geser Horizontal:", L"افقی فاصلہ:", L"অনুভূমিক সরণ:", L"Ofset Mendatar:", L"جابه‌جایی افقی:", L"Сдвиг по горизонтали:", L"Desplazamiento horizontal:"},
    // ChkShowPill
    {L"Görev çubuğunda gösterge şeridini göster", L"Show countdown pill on taskbar", L"إظهار الشريط في شريط المهام", L"Anzeige auf Taskleiste einblenden", L"Afficher la pilule dans la barre", L"Tampilkan badge di taskbar", L"ٹاسک بار پر الٹی گنتی دکھائیں", L"টাস্কবারে কাউন্টডাউন দেখান", L"Tunjuk kiraan detik pada bar tugas", L"نمایش شمارش معکوس در نوار وظیفه", L"Показывать обратный отсчёт на панели задач", L"Mostrar cuenta atrás en la barra de tareas"},
    // ChkOpaqueBg
    {L"Opak arka plan (üst üste binmeleri kapatır)", L"Opaque backdrop (prevents overlap)", L"خلفية معتمة (لمنع التداخل)", L"Deckender Hintergrund", L"Fond opaque", L"Latar belakang solid", L"ٹھوس پس منظر (اوورلیپ سے بچاؤ)", L"অস্বচ্ছ পটভূমি (ওভারল্যাপ রোধ করে)", L"Latar legap (elak pertindihan)", L"پس‌زمینه مات (جلوگیری از هم‌پوشانی)", L"Непрозрачный фон (без наложения)", L"Fondo opaco (evita superposición)"},
    // ChkShowTray
    {L"Sistem tepsisinde dinamik ikon göster", L"Show dynamic icon in system tray", L"إظهار أيقونة في علبة النظام", L"Symbol im Infobereich anzeigen", L"Icône dynamique dans le tray", L"Tampilkan ikon di System Tray", L"سسٹم ٹرے میں متحرک آئیکن دکھائیں", L"সিস্টেম ট্রেতে ডায়নামিক আইকন দেখান", L"Tunjuk ikon dinamik dalam dulang sistem", L"نمایش آیکون پویا در سینی سیستم", L"Показывать динамический значок в трее", L"Mostrar icono dinámico en la bandeja"},
    // ChkShowBar
    {L"Şerit altında ince ilerleme çubuğu göster", L"Show thin progress bar under pill", L"شريط تقدم رفيع أسفل العرض", L"Fortschrittsbalken anzeigen", L"Barre de progression fine", L"Tampilkan bilah progres", L"نیچے پتلی پیش رفت پٹی دکھائیں", L"নিচে পাতলা অগ্রগতি বার দেখান", L"Tunjuk bar kemajuan nipis di bawah", L"نمایش نوار پیشرفت باریک در پایین", L"Показывать тонкую полосу прогресса", L"Mostrar barra de progreso fina"},
    // ChkShowSec
    {L"Saniyeyi göster (pil tasarrufu için kapalı önerilir)", L"Show seconds (off saves laptop battery)", L"إظهار الثواني (إطفاؤه يوفر البطارية)", L"Sekunden anzeigen", L"Afficher les secondes", L"Tampilkan detik", L"سیکنڈ دکھائیں (بند رکھنے سے بیٹری بچتی ہے)", L"সেকেন্ড দেখান (বন্ধ রাখলে ব্যাটারি বাঁচে)", L"Tunjuk saat (matikan untuk jimat bateri)", L"نمایش ثانیه (خاموش = صرفه‌جویی باتری)", L"Показывать секунды (без них экономнее)", L"Mostrar segundos (desactivar ahorra batería)"},
    // ChkAccentSync
    {L"Windows 11 sistem vurgu rengini kullan", L"Match Windows 11 system accent color", L"مطابقة لون تمييز ويندوز 11", L"Windows 11-Akzentfarbe verwenden", L"Utiliser la couleur d'accentuation Win11", L"Ikuti warna aksen Windows 11", L"ونڈوز 11 کے ایکسنٹ رنگ سے ملائیں", L"উইন্ডোজ ১১-এর অ্যাকসেন্ট রঙের সাথে মেলান", L"Ikut warna aksen Windows 11", L"هماهنگی با رنگ تأکیدی ویندوز ۱۱", L"Использовать цвет акцента Windows 11", L"Usar el color de énfasis de Windows 11"},
    // LblTrayStyle
    {L"Tepsi İkon Stili:", L"System Tray Icon Style:", L"نمط أيقونة العلبة:", L"Infobereich-Symbolstil:", L"Style d'icône du tray :", L"Gaya Ikon Tray:", L"سسٹم ٹرے آئیکن انداز:", L"ট্রে আইকন শৈলী:", L"Gaya Ikon Dulang:", L"سبک آیکون سینی:", L"Стиль значка в трее:", L"Estilo del icono de bandeja:"},
    // TrayTwoLine
    {L"İki Satır Rozet (İ / 23)", L"Two-Line Badge", L"شارة من سطرين", L"Zweizeiliges Abzeichen", L"Badge deux lignes", L"Lencana Dua Baris", L"دو لائن بیج", L"দুই লাইনের ব্যাজ", L"Lencana Dua Baris", L"نشان دوخطی", L"Значок в две строки", L"Insignia de dos líneas"},
    // TrayRing
    {L"İlerleme Halkalı Dakika", L"Progress Ring + Minutes", L"حلقة التقدم والدقائق", L"Fortschrittsring + Minuten", L"Anneau + Minutes", L"Cincin Progres + Menit", L"پیش رفت حلقہ + منٹ", L"অগ্রগতি রিং + মিনিট", L"Gelang Kemajuan + Minit", L"حلقه پیشرفت + دقیقه", L"Кольцо прогресса + минуты", L"Anillo de progreso + minutos"},
    // TrayCrescent
    {L"Klasik Hilal İkonu", L"Classic Crescent Icon", L"أيقونة الهلال الكلاسيكية", L"Klassischer Halbmond", L"Croissant classique", L"Ikon Bulan Sabit", L"کلاسک ہلال آئیکن", L"ক্লাসিক অর্ধচন্দ্র আইকন", L"Ikon Bulan Sabit Klasik", L"آیکون هلال کلاسیک", L"Классический полумесяц", L"Icono clásico de media luna"},
    // LblFontSize
    {L"Yazı Boyutu:", L"Text Size:", L"حجم الخط:", L"Schriftgröße:", L"Taille du texte :", L"Ukuran Teks:", L"متن کا سائز:", L"লেখার আকার:", L"Saiz Teks:", L"اندازه متن:", L"Размер текста:", L"Tamaño del texto:"},
    // FontSmall
    {L"Küçük", L"Small", L"صغير", L"Klein", L"Petit", L"Kecil", L"چھوٹا", L"ছোট", L"Kecil", L"کوچک", L"Мелкий", L"Pequeño"},
    // FontNormal
    {L"Normal (Önerilen)", L"Normal (Recommended)", L"عادي (موصى به)", L"Normal (Empfohlen)", L"Normal (Recommandé)", L"Normal (Disarankan)", L"عام (تجویز کردہ)", L"স্বাভাবিক (প্রস্তাবিত)", L"Biasa (Disyorkan)", L"معمولی (پیشنهادی)", L"Обычный (рекомендуется)", L"Normal (recomendado)"},
    // FontLarge
    {L"Büyük", L"Large", L"كبير", L"Groß", L"Grand", L"Besar", L"بڑا", L"বড়", L"Besar", L"بزرگ", L"Крупный", L"Grande"},
    // LblUrgentColor
    {L"Son Dakika Uyarı Rengi:", L"Urgent Highlight:", L"تمييز اللحظات الأخيرة:", L"Dringlichkeits-Farbe:", L"Couleur urgente :", L"Warna Peringatan Akhir:", L"فوری نمایاں رنگ:", L"জরুরি হাইলাইট:", L"Serlahan Segera:", L"رنگ هشدار فوری:", L"Срочная подсветка:", L"Resaltado urgente:"},
    // UrgentOff
    {L"Kapalı", L"Off", L"معطّل", L"Aus", L"Désactivé", L"Mati", L"بند", L"বন্ধ", L"Mati", L"خاموش", L"Выкл.", L"Desactivado"},
    // Urgent10
    {L"Son 10 dakika", L"Last 10 minutes", L"آخر 10 دقائق", L"Letzte 10 Minuten", L"10 dernières minutes", L"10 menit terakhir", L"آخری 10 منٹ", L"শেষ ১০ মিনিট", L"10 minit terakhir", L"۱۰ دقیقه آخر", L"Последние 10 минут", L"Últimos 10 minutos"},
    // Urgent15
    {L"Son 15 dakika", L"Last 15 minutes", L"آخر 15 دقيقة", L"Letzte 15 Minuten", L"15 dernières minutes", L"15 menit terakhir", L"آخری 15 منٹ", L"শেষ ১৫ মিনিট", L"15 minit terakhir", L"۱۵ دقیقه آخر", L"Последние 15 минут", L"Últimos 15 minutos"},
    // Urgent20
    {L"Son 20 dakika", L"Last 20 minutes", L"آخر 20 دقيقة", L"Letzte 20 Minuten", L"20 dernières minutes", L"20 menit terakhir", L"آخری 20 منٹ", L"শেষ ২০ মিনিট", L"20 minit terakhir", L"۲۰ دقیقه آخر", L"Последние 20 минут", L"Últimos 20 minutos"},
    // Urgent30
    {L"Son 30 dakika", L"Last 30 minutes", L"آخر 30 دقيقة", L"Letzte 30 Minuten", L"30 dernières minutes", L"30 menit terakhir", L"آخری 30 منٹ", L"শেষ ৩০ মিনিট", L"30 minit terakhir", L"۳۰ دقیقه آخر", L"Последние 30 минут", L"Últimos 30 minutos"},
    // LblTheme
    {L"Uygulama Teması:", L"App Theme:", L"سمة التطبيق:", L"App-Design:", L"Thème :", L"Tema Aplikasi:", L"ایپ تھیم:", L"অ্যাপ থিম:", L"Tema Aplikasi:", L"پوسته برنامه:", L"Тема:", L"Tema:"},
    // ThemeSystem
    {L"Sistem (Otomatik)", L"System (Default)", L"النظام (تلقائي)", L"System (Standard)", L"Système (Auto)", L"Sistem (Otomatis)", L"سسٹم (طے شدہ)", L"সিস্টেম (ডিফল্ট)", L"Sistem (Lalai)", L"سیستم (پیش‌فرض)", L"Системная (по умолчанию)", L"Sistema (predeterminado)"},
    // ThemeDark
    {L"Koyu", L"Dark", L"داكن", L"Dunkel", L"Sombre", L"Gelap", L"گہرا", L"গাঢ়", L"Gelap", L"تیره", L"Тёмная", L"Oscuro"},
    // ThemeLight
    {L"Açık", L"Light", L"فاتح", L"Hell", L"Clair", L"Terang", L"ہلکا", L"হালকা", L"Cerah", L"روشن", L"Светлая", L"Claro"},

    // BtnDetectLocation
    {L"Konumumu Otomatik Bul", L"Auto-Detect Location", L"تحديد الموقع تلقائياً", L"Standort automatisch erkennen", L"Détecter ma position", L"Deteksi Lokasi Otomatis", L"مقام خود معلوم کریں", L"স্বয়ংক্রিয় অবস্থান শনাক্ত", L"Kesan Lokasi Automatik", L"تشخیص خودکار مکان", L"Определить местоположение", L"Detectar ubicación"},
    // LocDetectHint
    {L"Konum algılamak için butona tıklayın.", L"Click button to detect location by IP.", L"اضغط لتحديد الموقع عبر الإنترنت.", L"Klicken zur Standorterkennung.", L"Cliquez pour détecter votre ville.", L"Klik tombol untuk deteksi lokasi.", L"IP سے مقام معلوم کرنے کے لیے بٹن دبائیں۔", L"IP দিয়ে অবস্থান শনাক্ত করতে বোতাম চাপুন।", L"Klik butang untuk mengesan lokasi melalui IP.", L"برای تشخیص مکان از روی IP دکمه را بزنید.", L"Нажмите кнопку, чтобы определить место по IP.", L"Pulse el botón para detectar la ubicación por IP."},
    // LocDetecting
    {L"Konum algılanıyor...", L"Detecting location...", L"جاري تحديد الموقع...", L"Standort wird ermittelt...", L"Détection en cours...", L"Mendeteksi lokasi...", L"مقام معلوم کیا جا رہا ہے...", L"অবস্থান শনাক্ত হচ্ছে...", L"Mengesan lokasi...", L"در حال تشخیص مکان...", L"Определение местоположения...", L"Detectando ubicación..."},
    // LocDetectFailed
    {L"Konum algılanamadı. İnterneti kontrol edin.", L"Location detection failed. Check internet.", L"تعذر تحديد الموقع. تحقق من الاتصال.", L"Erkennung fehlgeschlagen.", L"Échec de détection.", L"Deteksi lokasi gagal.", L"مقام معلوم نہیں ہو سکا۔ انٹرنیٹ چیک کریں۔", L"অবস্থান শনাক্ত ব্যর্থ। ইন্টারনেট দেখুন।", L"Pengesanan lokasi gagal. Semak internet.", L"تشخیص مکان ناموفق بود. اینترنت را بررسی کنید.", L"Не удалось определить место. Проверьте интернет.", L"No se pudo detectar la ubicación. Revise internet."},
    // ChkAutoDetectLocation
    {L"Uygulama açılışında konumumu otomatik güncelle", L"Automatically update my location at startup", L"تحديث موقعي تلقائياً عند بدء التطبيق", L"Standort beim Start automatisch aktualisieren", L"Mettre à jour ma position au démarrage", L"Perbarui lokasi otomatis saat aplikasi dimulai", L"ایپ شروع ہونے پر میرا مقام خودکار اپ ڈیٹ کریں", L"অ্যাপ চালু হলে আমার অবস্থান স্বয়ংক্রিয়ভাবে আপডেট করুন", L"Kemas kini lokasi saya secara automatik semasa aplikasi bermula", L"به‌روزرسانی خودکار مکان هنگام شروع برنامه", L"Автоматически обновлять местоположение при запуске", L"Actualizar mi ubicación al iniciar la aplicación"},
    // ChkAutoMethod
    {L"Ülkeye göre uygun hesaplama metodunu otomatik seç", L"Auto-select calculation method for country", L"اختيار طريقة الحساب تلقائياً حسب الدولة", L"Berechnungsmethode nach Land wählen", L"Choisir la méthode selon le pays", L"Pilih metode otomatis sesuai negara", L"ملک کے مطابق حساب کا طریقہ خود منتخب کریں", L"দেশ অনুযায়ী গণনা পদ্ধতি স্বয়ংক্রিয় বাছাই", L"Pilih kaedah pengiraan mengikut negara", L"انتخاب خودکار روش محاسبه بر اساس کشور", L"Выбирать метод расчёта по стране", L"Elegir método de cálculo según el país"},
    // LblCitySearch
    {L"Şehir Ara (81 İl & Dünya):", L"Search City (Worldwide):", L"ابحث عن مدينة:", L"Stadt suchen:", L"Rechercher une ville :", L"Cari Kota:", L"شہر تلاش کریں (دنیا بھر):", L"শহর খুঁজুন (বিশ্বব্যাপী):", L"Cari Bandar (Seluruh Dunia):", L"جستجوی شهر (سراسر جهان):", L"Поиск города (весь мир):", L"Buscar ciudad (mundial):"},
    // ChkCustomCoord
    {L"Özel Enlem/Boylam:", L"Custom Lat / Lon:", L"إحداثيات مخصصة:", L"Eigene Koordinaten:", L"Coordonnées personnalisées :", L"Koordinat Kustom:", L"حسب ضرورت عرض / طول:", L"কাস্টম অক্ষাংশ / দ্রাঘিমা:", L"Lat / Lon Tersuai:", L"عرض / طول دلخواه:", L"Свои широта / долгота:", L"Lat / Lon personalizadas:"},
    // LblLat
    {L"Enlem:", L"Lat:", L"خط العرض:", L"Breite:", L"Lat :", L"Lintang:", L"عرض:", L"অক্ষাংশ:", L"Lat:", L"عرض:", L"Шир.:", L"Lat:"},
    // LblLon
    {L"Boylam:", L"Lon:", L"خط الطول:", L"Länge:", L"Lon :", L"Bujur:", L"طول:", L"দ্রাঘিমা:", L"Lon:", L"طول:", L"Долг.:", L"Lon:"},
    // BtnApplyCoord
    {L"Koordinat Uygula", L"Apply Coords", L"تطبيق", L"Übernehmen", L"Appliquer", L"Terapkan", L"کوآرڈینیٹس لاگو کریں", L"স্থানাঙ্ক প্রয়োগ", L"Guna Koordinat", L"اعمال مختصات", L"Применить", L"Aplicar coordenadas"},
    // LblCalcMethod
    {L"Hesaplama Metodu:", L"Calculation Method:", L"طريقة الحساب:", L"Berechnungsmethode:", L"Méthode de calcul :", L"Metode Perhitungan:", L"حساب کا طریقہ:", L"গণনা পদ্ধতি:", L"Kaedah Pengiraan:", L"روش محاسبه:", L"Метод расчёта:", L"Método de cálculo:"},
    // LblAsrMethod
    {L"İkindi Mezhebi:", L"Asr Juristic Method:", L"مذهب العصر:", L"Asr-Rechtsschule:", L"Méthode Asr :", L"Mazhab Ashar:", L"عصر کا فقہی طریقہ:", L"আসরের মাজহাব পদ্ধতি:", L"Kaedah Fiqh Asar:", L"روش فقهی عصر:", L"Мазхаб для Асра:", L"Método jurídico del Asr:"},
    // AsrStandard
    {L"Standart (Şafii / Maliki / Hanbeli / Diyanet)", L"Standard (Shafi / Maliki / Hanbali)", L"الجمهور (الشافعي / المالكي / الحنبلي)", L"Standard (Schafi / Maliki / Hanbali)", L"Standard (Chaféite / Malékite)", L"Standar (Syafi'i / Maliki / Hanbali)", L"معیاری (شافعی / مالکی / حنبلی)", L"স্ট্যান্ডার্ড (শাফেয়ি / মালিকি / হাম্বলি)", L"Standard (Syafie / Maliki / Hanbali)", L"استاندارد (شافعی / مالکی / حنبلی)", L"Стандартный (шафиитский / маликитский / ханбалитский)", L"Estándar (Shafi'i / Maliki / Hanbali)"},
    // AsrHanafi
    {L"Hanefi (Asr-ı Sani)", L"Hanafi", L"الحنفي", L"Hanafitisch", L"Hanafite", L"Hanafi", L"حنفی", L"হানাফি", L"Hanafi", L"حنفی", L"Ханафитский", L"Hanafi"},
    // ChkOnlineSync
    {L"Aylık vakitleri arka planda önbelleğe al (çevrimdışı yedekli)", L"Cache monthly schedule in background (offline ready)", L"حفظ المواقيت الشهرية في الخلفية", L"Monatszeiten im Hintergrund puffern", L"Mettre en cache le mois", L"Simpan jadwal bulanan offline", L"پس منظر میں ماہانہ نظام الاوقات محفوظ کریں (آف لائن تیار)", L"পটভূমিতে মাসিক সূচি সংরক্ষণ (অফলাইনেও চলবে)", L"Simpan jadual bulanan di latar (sedia luar talian)", L"ذخیره جدول ماهانه در پس‌زمینه (آماده آفلاین)", L"Кэшировать расписание на месяц (работает офлайн)", L"Guardar el horario mensual en segundo plano (sin conexión)"},
    // BtnSyncNow
    {L"Vakitleri Güncelle", L"Sync Now", L"تحديث الآن", L"Jetzt abrufen", L"Actualiser", L"Sinkronkan", L"ابھی ہم آہنگ کریں", L"এখনই সিঙ্ক", L"Segerak Sekarang", L"همگام‌سازی اکنون", L"Синхронизировать", L"Sincronizar ahora"},
    // BtnOpenImsakiye
    {L"Aylık İmsakiye", L"Open İmsakiye", L"الإمساكية الشهرية", L"Monatskalender", L"Calendrier mensuel", L"Buka Imsakiyah", L"امساکیہ کھولیں", L"ইমসাকিয়া খুলুন", L"Buka Imsakiah", L"باز کردن امساکیه", L"Открыть Имсакию", L"Abrir Imsakiye"},
    // SyncingText
    {L"Aylık vakitler eşitleniyor...", L"Syncing monthly prayer times...", L"جاري مزامنة المواقيت الشهرية...", L"Monatszeiten werden geladen...", L"Synchronisation du mois...", L"Menyinkronkan jadwal bulanan...", L"ماہانہ اوقات ہم آہنگ ہو رہے ہیں...", L"মাসিক নামাজের সময় সিঙ্ক হচ্ছে...", L"Menyegerak waktu solat bulanan...", L"در حال همگام‌سازی اوقات ماهانه...", L"Синхронизация расписания на месяц...", L"Sincronizando horarios mensuales..."},
    // SyncOkText
    {L"Bu ayın vakitleri güncellendi ve önbelleğe alındı.", L"Monthly prayer times updated and cached.", L"تم تحديث المواقيت وحفظها.", L"Monatszeiten aktualisiert.", L"Horaires du mois mis à jour.", L"Jadwal bulanan diperbarui.", L"ماہانہ اوقات اپ ڈیٹ اور محفوظ ہو گئے۔", L"মাসিক নামাজের সময় হালনাগাদ ও সংরক্ষিত হয়েছে।", L"Waktu solat bulanan dikemas kini dan disimpan.", L"اوقات ماهانه به‌روز و ذخیره شد.", L"Расписание на месяц обновлено и сохранено.", L"Horarios mensuales actualizados y guardados."},
    // SyncOfflineText
    {L"İnternet bağlantısı yok — çevrimdışı hesaplama kullanılıyor.", L"No internet — using local astronomical fallback.", L"لا يوجد اتصال — يتم استخدام الحساب المحلي.", L"Kein Internet — Offline-Berechnung aktiv.", L"Hors ligne — calcul local actif.", L"Tidak ada internet — pakai hitungan lokal.", L"انٹرنیٹ نہیں — مقامی فلکیاتی حساب استعمال ہو رہا ہے۔", L"ইন্টারনেট নেই — স্থানীয় জ্যোতির্বিদ্যা গণনা ব্যবহার হচ্ছে।", L"Tiada internet — menggunakan pengiraan astronomi tempatan.", L"اینترنت نیست — از محاسبه نجومی محلی استفاده می‌شود.", L"Нет интернета — используется локальный расчёт.", L"Sin internet — se usa el cálculo astronómico local."},
    // LblOffsets
    {L"Vakit İnce Ayarı (Dakika ±):", L"Manual Time Offsets (Min ±):", L"تعديل الدقائق (±):", L"Minuten-Feinjustierung (±):", L"Ajustement manuel (min ±) :", L"Koreksi Menit (±):", L"دستی وقت کی تصحیح (منٹ ±):", L"হাতে সময় সংশোধন (মিনিট ±):", L"Pelarasan Waktu Manual (Minit ±):", L"تنظیم دستی زمان (دقیقه ±):", L"Ручная поправка (мин ±):", L"Ajustes manuales (min ±):"},
    // BtnApplyOffsets
    {L"Ofsetleri Uygula", L"Apply Offsets", L"تطبيق التعديل", L"Übernehmen", L"Appliquer", L"Terapkan", L"تصحیح لاگو کریں", L"সংশোধন প্রয়োগ", L"Guna Pelarasan", L"اعمال تنظیم", L"Применить поправки", L"Aplicar ajustes"},

    // ColPrayer
    {L"Vakit", L"Prayer", L"الصلاة", L"Gebet", L"Prière", L"Sholat", L"نماز", L"নামাজ", L"Solat", L"نماز", L"Намаз", L"Oración"},
    // ColAtTime
    {L"Vaktinde", L"On Time", L"عند الوقت", L"Zur Zeit", L"À l'heure", L"Tepat Waktu", L"وقت پر", L"সময়মতো", L"Tepat Waktu", L"سر وقت", L"Вовремя", L"A la hora"},
    // ColPreRemind
    {L"Önce Hatırlat", L"Remind Before", L"التنبيه المسبق", L"Vorab erinnern", L"Rappel avant", L"Pengingat", L"پہلے یاد دہانی", L"আগে মনে করান", L"Ingatkan Awal", L"یادآوری پیشاپیش", L"Напомнить заранее", L"Recordar antes"},
    // ColSoundEzan
    {L"Ses / Ezan", L"Sound / Adhan", L"الصوت / الأذان", L"Ton / Adhan", L"Son / Adhan", L"Suara / Adzan", L"آواز / اذان", L"শব্দ / আজান", L"Bunyi / Azan", L"صدا / اذان", L"Звук / азан", L"Sonido / adhan"},
    // ChkNotify
    {L"Bildir", L"Notify", L"إشعار", L"Melden", L"Notifier", L"Beri tahu", L"اطلاع", L"জানান", L"Maklum", L"اعلان", L"Уведомлять", L"Avisar"},
    // ChkPlaySound
    {L"Ses Çal", L"Play", L"تشغيل", L"Abspielen", L"Jouer", L"Putar", L"چلائیں", L"বাজান", L"Main", L"پخش", L"Звук", L"Sonar"},
    // PreOff
    {L"Kapalı", L"Off", L"معطّل", L"Aus", L"Désactivé", L"Mati", L"بند", L"বন্ধ", L"Mati", L"خاموش", L"Выкл.", L"Desactivado"},
    // Pre5
    {L"5 dk önce", L"5 min before", L"قبل 5 د", L"5 Min. vorher", L"5 min avant", L"5 mnt sblm", L"5 منٹ پہلے", L"৫ মিনিট আগে", L"5 minit sebelum", L"۵ دقیقه قبل", L"За 5 минут", L"5 min antes"},
    // Pre10
    {L"10 dk önce", L"10 min before", L"قبل 10 د", L"10 Min. vorher", L"10 min avant", L"10 mnt sblm", L"10 منٹ پہلے", L"১০ মিনিট আগে", L"10 minit sebelum", L"۱۰ دقیقه قبل", L"За 10 минут", L"10 min antes"},
    // Pre15
    {L"15 dk önce", L"15 min before", L"قبل 15 د", L"15 Min. vorher", L"15 min avant", L"15 mnt sblm", L"15 منٹ پہلے", L"১৫ মিনিট আগে", L"15 minit sebelum", L"۱۵ دقیقه قبل", L"За 15 минут", L"15 min antes"},
    // Pre20
    {L"20 dk önce", L"20 min before", L"قبل 20 د", L"20 Min. vorher", L"20 min avant", L"20 mnt sblm", L"20 منٹ پہلے", L"২০ মিনিট আগে", L"20 minit sebelum", L"۲۰ دقیقه قبل", L"За 20 минут", L"20 min antes"},
    // Pre30
    {L"30 dk önce", L"30 min before", L"قبل 30 د", L"30 Min. vorher", L"30 min avant", L"30 mnt sblm", L"30 منٹ پہلے", L"৩০ মিনিট আগে", L"30 minit sebelum", L"۳۰ دقیقه قبل", L"За 30 минут", L"30 min antes"},
    // Pre45
    {L"45 dk önce", L"45 min before", L"قبل 45 د", L"45 Min. vorher", L"45 min avant", L"45 mnt sblm", L"45 منٹ پہلے", L"৪৫ মিনিট আগে", L"45 minit sebelum", L"۴۵ دقیقه قبل", L"За 45 минут", L"45 min antes"},
    // Pre60
    {L"60 dk önce", L"60 min before", L"قبل 60 د", L"60 Min. vorher", L"60 min avant", L"60 mnt sblm", L"60 منٹ پہلے", L"৬০ মিনিট আগে", L"60 minit sebelum", L"۶۰ دقیقه قبل", L"За 60 минут", L"60 min antes"},
    // LblEntrySound
    {L"Vakit Girdi Sesi:", L"On-Time Sound:", L"صوت دخول الوقت:", L"Gebetszeit-Ton:", L"Son à l'heure :", L"Suara Masuk Waktu:", L"وقت پر آواز:", L"সময়মতো শব্দ:", L"Bunyi Tepat Waktu:", L"صدای سر وقت:", L"Звук в начале:", L"Sonido a la hora:"},
    // LblPreSound
    {L"Vakit Öncesi Sesi:", L"Pre-Alert Sound:", L"صوت التنبيه المسبق:", L"Vorab-Hinweis-Ton:", L"Son de rappel :", L"Suara Pengingat:", L"پیشگی اطلاع کی آواز:", L"আগাম সতর্কতার শব্দ:", L"Bunyi Peringatan Awal:", L"صدای هشدار پیشاپیش:", L"Звук напоминания:", L"Sonido de aviso previo:"},
    // SoundNone
    {L"Sessiz", L"Silent", L"صامت", L"Stumm", L"Silencieux", L"Hening", L"خاموش", L"নীরব", L"Senyap", L"بی‌صدا", L"Без звука", L"Silencio"},
    // SoundChime
    {L"Yumuşak Çan (Sentez)", L"Gentle Chime", L"رنين هادئ", L"Sanfter Klang", L"Carillon doux", L"Lonceng Lembut", L"ہلکی گھنٹی", L"মৃদু চাইম", L"Loceng Lembut", L"زنگ ملایم", L"Мягкий звон", L"Campanilla suave"},
    // SoundBell
    {L"Huzurlu Zil (Sentez)", L"Warm Bell", L"جرس دافئ", L"Warme Glocke", L"Cloche douce", L"Bel Hangat", L"گرم گھنٹی", L"উষ্ণ ঘণ্টা", L"Loceng Hangat", L"زنگ گرم", L"Тёплый колокол", L"Campana cálida"},
    // SoundAdhanShort
    {L"Ücretsiz Kısa Ezan (Nasser al-Qatami)", L"Free Short Adhan (Nasser al-Qatami)", L"أذان قصير مجاني (ناصر القطامي)", L"Kurzer Adhan (Nasser al-Qatami)", L"Adhan court (Nasser al-Qatami)", L"Adzan Singkat (Nasser al-Qatami)", L"مفت مختصر اذان (ناصر القطامی)", L"বিনামূল্যে সংক্ষিপ্ত আজান (নাসের আল-কাতামি)", L"Azan Pendek Percuma (Nasser al-Qatami)", L"اذان کوتاه رایگان (ناصر القطامی)", L"Бесплатный короткий азан (Насер аль-Катами)", L"Adhan corto gratuito (Nasser al-Qatami)"},
    // SoundAdhanMakkah
    {L"Ücretsiz Uzun Ezan (Majed al-Hamathani)", L"Free Full Adhan (Majed al-Hamathani)", L"أذان طويل مجاني (ماجد الهمثاني)", L"Langer Adhan (Majed al-Hamathani)", L"Adhan long (Majed al-Hamathani)", L"Adzan Panjang (Majed al-Hamathani)", L"مفت مکمل اذان (ماجد الحماثانی)", L"বিনামূল্যে পূর্ণ আজান (মাজেদ আল-হামাথানি)", L"Azan Penuh Percuma (Majed al-Hamathani)", L"اذان کامل رایگان (ماجد الحماثانی)", L"Бесплатный полный азан (Маджид аль-Хамасани)", L"Adhan completo gratuito (Majed al-Hamathani)"},
    // SoundCustom
    {L"Özel Ses Dosyası (.mp3 / .wav)", L"Custom Audio File (.mp3 / .wav)", L"ملف صوتي مخصص", L"Eigene Audiodatei", L"Fichier audio perso", L"File Audio Kustom", L"حسب ضرورت آڈیو فائل (.mp3 / .wav)", L"কাস্টম অডিও ফাইল (.mp3 / .wav)", L"Fail Audio Tersuai (.mp3 / .wav)", L"فایل صوتی دلخواه (.mp3 / .wav)", L"Свой аудиофайл (.mp3 / .wav)", L"Archivo de audio propio (.mp3 / .wav)"},
    // LblCustomSound
    {L"Özel Ses Dosyası:", L"Custom Audio:", L"ملف الصوت:", L"Eigene Datei:", L"Fichier perso :", L"File Kustom:", L"حسب ضرورت آڈیو:", L"কাস্টম অডিও:", L"Audio Tersuai:", L"صدای دلخواه:", L"Свой звук:", L"Audio propio:"},
    // BtnBrowse
    {L"Gözat...", L"Browse...", L"استعراض...", L"Durchsuchen...", L"Parcourir...", L"Pilih...", L"براؤز...", L"ব্রাউজ...", L"Semak Imbas...", L"مرور...", L"Обзор...", L"Examinar..."},
    // LblVolume
    {L"Ses Düzeyi:", L"Volume:", L"مستوى الصوت:", L"Lautstärke:", L"Volume :", L"Volume:", L"آواز کی سطح:", L"ভলিউম:", L"Kelantangan:", L"بلندی صدا:", L"Громкость:", L"Volumen:"},
    // BtnTestEntry
    {L"Vakit Sesi Test Et", L"Test Adhan", L"تجربة الأذان", L"Ton testen", L"Tester l'Adhan", L"Uji Adzan", L"اذان آزمائیں", L"আজান পরীক্ষা", L"Uji Azan", L"آزمایش اذان", L"Проверить азан", L"Probar adhan"},
    // BtnTestPre
    {L"Ön Uyarı Test Et", L"Test Pre-Alert", L"تجربة المسبق", L"Vorab testen", L"Tester rappel", L"Uji Pengingat", L"پیشگی اطلاع آزمائیں", L"আগাম সতর্কতা পরীক্ষা", L"Uji Peringatan Awal", L"آزمایش هشدار", L"Проверить напоминание", L"Probar aviso previo"},
    // BtnStopSound
    {L"■ Sesi Durdur", L"■ Stop Audio", L"■ إيقاف", L"■ Stopp", L"■ Arrêter", L"■ Stop", L"■ آواز بند کریں", L"■ শব্দ বন্ধ", L"■ Hentikan Audio", L"■ توقف صدا", L"■ Остановить звук", L"■ Detener audio"},
    // BtnTestNotif
    {L"Bildirim Test Et", L"Test Notification", L"تجربة الإشعار", L"Benachrichtigung", L"Tester notif.", L"Uji Notifikasi", L"اطلاع آزمائیں", L"বিজ্ঞপ্তি পরীক্ষা", L"Uji Pemberitahuan", L"آزمایش اعلان", L"Проверить уведомление", L"Probar notificación"},
    // ChkQuietFullscreen
    {L"Tam ekran / oyun / Odak Yardımı sırasında sessize al", L"Mute during fullscreen apps, games & Focus Assist", L"كتم الصوت أثناء الألعاب ووضع التركيز", L"Bei Vollbild / Spielen / Fokus stummschalten", L"Silencieux en plein écran / jeux", L"Bisukan saat layar penuh / game", L"فل اسکرین ایپس، گیمز اور فوکس اسسٹ میں خاموش رہیں", L"ফুলস্ক্রিন অ্যাপ, গেম ও ফোকাস অ্যাসিস্টে নীরব থাকুন", L"Senyap semasa aplikasi skrin penuh, permainan & Bantuan Fokus", L"بی‌صدا در برنامه‌های تمام‌صفحه، بازی‌ها و دستیار تمرکز", L"Без звука в полноэкранных приложениях, играх и «Фокусировке»", L"Silenciar en pantalla completa, juegos y Asistente de concentración"},
    // ChkKerahatIndicator
    {L"Kerahat vakti uyarısını ve Cuma vurgusunu göster", L"Show Makruh time indicator & Jumu'ah highlight", L"إظهار تنبيه الكراهة وتمييز الجمعة", L"Karaha-Hinweis & Dschuma hervorheben", L"Afficher alerte Karaha & Joumou'a", L"Tampilkan tanda Waktu Terlarang & Jumat", L"مکروہ وقت کا اشارہ اور جمعہ نمایاں دکھائیں", L"মাকরুহ সময় নির্দেশক ও জুমা হাইলাইট দেখান", L"Tunjuk penunjuk waktu makruh & serlahan Jumaat", L"نمایش نشانگر وقت مکروه و برجسته‌سازی جمعه", L"Показывать время макрух и выделять джуму", L"Mostrar indicador makruh y resaltar el viernes"},

    // LblAppLanguage
    {L"Uygulama Dili:", L"App Language:", L"لغة التطبيق:", L"App-Sprache:", L"Langue :", L"Bahasa Aplikasi:", L"ایپ کی زبان:", L"অ্যাপের ভাষা:", L"Bahasa Aplikasi:", L"زبان برنامه:", L"Язык приложения:", L"Idioma de la aplicación:"},
    // SectionCustomNames
    {L"Namaz İsimlerini Manuel Değiştir (Boş bırakılırsa dil varsayılanı kullanılır):", L"Customize Prayer Names Manually (Empty = language default):", L"تخصيص أسماء الصلوات يدوياً (الفارغ = الافتراضي):", L"Gebetsnamen manuell anpassen (Leer = Standard):", L"Personnaliser les noms des prières (Vide = défaut) :", L"Ubah Nama Sholat Manual (Kosong = default bahasa):", L"نمازوں کے نام دستی طور پر بدلیں (خالی = زبان کا طے شدہ):", L"নামাজের নাম নিজে বদলান (খালি = ভাষার ডিফল্ট):", L"Ubah Nama Solat Secara Manual (Kosong = lalai bahasa):", L"تغییر دستی نام نمازها (خالی = پیش‌فرض زبان):", L"Свои названия намазов (пусто = по умолчанию):", L"Personalizar nombres de las oraciones (vacío = predeterminado):"},
    // ColDefaultName
    {L"Varsayılan", L"Default", L"الافتراضي", L"Standard", L"Défaut", L"Default", L"طے شدہ", L"ডিফল্ট", L"Lalai", L"پیش‌فرض", L"По умолчанию", L"Predeterminado"},
    // ColCustomName
    {L"Özel İsim", L"Custom Name", L"اسم مخصص", L"Eigener Name", L"Nom personnalisé", L"Nama Kustom", L"حسب ضرورت نام", L"কাস্টম নাম", L"Nama Tersuai", L"نام دلخواه", L"Своё название", L"Nombre propio"},
    // ColShortCode
    {L"Kısa Kod", L"Short", L"اختصار", L"Kurz", L"Code", L"Singkat", L"مختصر", L"সংক্ষেপ", L"Ringkas", L"کوتاه", L"Кратко", L"Corto"},
    // BtnApplyNames
    {L"İsimleri Uygula", L"Apply Names", L"تطبيق الأسماء", L"Namen anwenden", L"Appliquer les noms", L"Terapkan Nama", L"نام لاگو کریں", L"নাম প্রয়োগ", L"Guna Nama", L"اعمال نام‌ها", L"Применить названия", L"Aplicar nombres"},
    // BtnResetNames
    {L"Varsayılana Sıfırla", L"Reset Defaults", L"إعادة تعيين", L"Zurücksetzen", L"Réinitialiser", L"Reset Default", L"طے شدہ پر واپس", L"ডিফল্টে ফেরান", L"Set Semula Lalai", L"بازگشت به پیش‌فرض", L"Сбросить", L"Restablecer"},
    // SectionSystem
    {L"Sistem, Otomatik Bellek ve Yedekleme:", L"System, Auto Memory & Backup:", L"النظام والذاكرة التلقائية والنسخ:", L"System, Speicher & Sicherung:", L"Système, Mémoire & Sauvegarde :", L"Sistem, Memori Otomatis & Cadangan:", L"سسٹم، خودکار میموری اور بیک اپ:", L"সিস্টেম, স্বয়ংক্রিয় মেমরি ও ব্যাকআপ:", L"Sistem, Memori Automatik & Sandaran:", L"سیستم، حافظه خودکار و پشتیبان:", L"Система, память и резервная копия:", L"Sistema, memoria automática y copia de seguridad:"},
    // ChkStartup
    {L"Windows açılışında otomatik başlat", L"Start automatically with Windows", L"التشغيل التلقائي مع ويندوز", L"Mit Windows starten", L"Démarrer avec Windows", L"Jalankan otomatis saat Windows mulai", L"ونڈوز کے ساتھ خود بخود شروع کریں", L"উইন্ডোজের সাথে স্বয়ংক্রিয়ভাবে চালু করুন", L"Mula secara automatik bersama Windows", L"اجرای خودکار با ویندوز", L"Запускать вместе с Windows", L"Iniciar automáticamente con Windows"},
    // ChkAutoMem
    {L"Belleği otomatik optimize et (boşta ve pencere kapanınca RAM düşür)", L"Optimize memory automatically (recommended)", L"تحسين الذاكرة تلقائياً (موصى به)", L"Speicher automatisch optimieren (Empfohlen)", L"Optimiser la mémoire automatiquement", L"Optimalkan memori secara otomatis (disarankan)", L"میموری خود بخود بہتر بنائیں (تجویز کردہ)", L"স্বয়ংক্রিয়ভাবে মেমরি অপ্টিমাইজ করুন (প্রস্তাবিত)", L"Optimumkan memori secara automatik (disyorkan)", L"بهینه‌سازی خودکار حافظه (پیشنهادی)", L"Автоматически оптимизировать память (рекомендуется)", L"Optimizar memoria automáticamente (recomendado)"},
    // BtnExportSettings
    {L"Ayarları Dışa Aktar", L"Export Settings", L"تصدير الإعدادات", L"Einstellungen exportieren", L"Exporter paramètres", L"Ekspor Pengaturan", L"ترتیبات برآمد کریں", L"সেটিংস রপ্তানি", L"Eksport Tetapan", L"خروجی تنظیمات", L"Экспорт настроек", L"Exportar configuración"},
    // BtnImportSettings
    {L"Ayarları İçe Aktar", L"Import Settings", L"استيراد الإعدادات", L"Einstellungen importieren", L"Importer paramètres", L"Impor Pengaturan", L"ترتیبات درآمد کریں", L"সেটিংস আমদানি", L"Import Tetapan", L"ورود تنظیمات", L"Импорт настроек", L"Importar configuración"},
    // MsgSettingsExported
    {L"Ayarlar JSON dosyasına dışa aktarıldı.", L"Settings exported to JSON file.", L"تم تصدير الإعدادات بنجاح.", L"Einstellungen exportiert.", L"Paramètres exportés.", L"Pengaturan berhasil diekspor.", L"ترتیبات JSON فائل میں برآمد ہو گئیں۔", L"সেটিংস JSON ফাইলে রপ্তানি হয়েছে।", L"Tetapan dieksport ke fail JSON.", L"تنظیمات در فایل JSON ذخیره شد.", L"Настройки сохранены в JSON-файл.", L"Configuración exportada a un archivo JSON."},
    // MsgSettingsImported
    {L"Ayarlar içe aktarıldı ve uygulandı.", L"Settings imported and applied.", L"تم استيراد الإعدادات وتطبيقها.", L"Einstellungen importiert.", L"Paramètres importés.", L"Pengaturan berhasil diimpor.", L"ترتیبات درآمد اور لاگو ہو گئیں۔", L"সেটিংস আমদানি ও প্রয়োগ হয়েছে।", L"Tetapan diimport dan digunakan.", L"تنظیمات وارد و اعمال شد.", L"Настройки импортированы и применены.", L"Configuración importada y aplicada."},

    // ChkRamadanMode
    {L"Ramazan ayında İftar / Sahur özel geri sayımını göster", L"Show special Iftar / Suhoor countdown during Ramadan", L"إظهار عداد الإفطار والسحور الخاص في رمضان", L"Im Ramadan Iftar- / Suhoor-Countdown anzeigen", L"Afficher le décompte Iftar / Souhour pendant le Ramadan", L"Tampilkan hitung mundur Buka / Sahur saat Ramadan", L"رمضان میں افطار / سحری کی خاص الٹی گنتی دکھائیں", L"রমজানে ইফতার / সাহরির বিশেষ কাউন্টডাউন দেখান", L"Tunjuk kiraan detik Berbuka / Sahur semasa Ramadan", L"نمایش شمارش معکوس افطار / سحر در رمضان", L"Отсчёт до ифтара / сухура в Рамадан", L"Mostrar cuenta atrás de Iftar / Suhur en Ramadán"},
    // MenuUpgradePro
    {L"★ PrayerBar Pro'ya Yükselt...", L"★ Upgrade to PrayerBar Pro...", L"★ الترقية إلى الإصدار الاحترافي...", L"★ Auf Pro upgraden...", L"★ Passer à PrayerBar Pro...", L"★ Upgrade ke PrayerBar Pro...", L"★ PrayerBar Pro حاصل کریں...", L"★ PrayerBar Pro-তে আপগ্রেড করুন...", L"★ Naik Taraf ke PrayerBar Pro...", L"★ ارتقا به PrayerBar Pro...", L"★ Перейти на PrayerBar Pro...", L"★ Mejorar a PrayerBar Pro..."},
    // MenuManageLicense
    {L"✓ PrayerBar Pro (Lisans Yönetimi)...", L"✓ PrayerBar Pro (License)...", L"✓ الإصدار الاحترافي (الترخيص)...", L"✓ PrayerBar Pro (Lizenz)...", L"✓ PrayerBar Pro (Licence)...", L"✓ PrayerBar Pro (Lisensi)...", L"✓ PrayerBar Pro (لائسنس)...", L"✓ PrayerBar Pro (লাইসেন্স)...", L"✓ PrayerBar Pro (Lesen)...", L"✓ PrayerBar Pro (مجوز)...", L"✓ PrayerBar Pro (лицензия)...", L"✓ PrayerBar Pro (licencia)..."},
    // ProWindowTitle
    {L"PrayerBar Pro — Lisans & Yükseltme", L"PrayerBar Pro — Upgrade & License", L"PrayerBar Pro — الترقية والترخيص", L"PrayerBar Pro — Upgrade & Lizenz", L"PrayerBar Pro — Mise à niveau", L"PrayerBar Pro — Upgrade & Lisensi", L"PrayerBar Pro — اپ گریڈ اور لائسنس", L"PrayerBar Pro — আপগ্রেড ও লাইসেন্স", L"PrayerBar Pro — Naik Taraf & Lesen", L"PrayerBar Pro — ارتقا و مجوز", L"PrayerBar Pro — Покупка и лицензия", L"PrayerBar Pro — Mejora y licencia"},
    // ProStatusTrial
    {L"14 Günlük Ücretsiz Pro Deneme Aktif", L"14-Day Free Pro Trial Active", L"الفترة التجريبية المجانية (14 يوماً) مفعّلة", L"14-Tage-Pro-Testversion aktiv", L"Essai Pro gratuit de 14 jours actif", L"Uji Coba Pro 14 Hari Aktif", L"14 دن کا مفت Pro ٹرائل فعال ہے", L"১৪ দিনের বিনামূল্যে Pro ট্রায়াল চালু", L"Percubaan Pro Percuma 14 Hari Aktif", L"دوره آزمایشی رایگان ۱۴ روزه Pro فعال است", L"Активен бесплатный 14-дневный пробный Pro", L"Prueba gratuita de Pro de 14 días activa"},
    // ProStatusFree
    {L"Ücretsiz Temel Sürüm (Temel ibadet vakitleri daima ücretsizdir)", L"Free Edition (Core prayer times remain 100% free)", L"الإصدار المجاني (المواقيت الأساسية مجانية دائماً)", L"Kostenlose Basisversion", L"Version gratuite (Horaires toujours gratuits)", L"Edisi Gratis (Jadwal Sholat Selalu Gratis)", L"مفت ایڈیشن (بنیادی نماز اوقات ہمیشہ مفت)", L"ফ্রি সংস্করণ (মূল নামাজের সময় সবসময় বিনামূল্যে)", L"Edisi Percuma (waktu solat asas sentiasa percuma)", L"نسخه رایگان (اوقات اصلی نماز همیشه رایگان)", L"Бесплатная версия (время намаза всегда бесплатно)", L"Edición gratuita (los horarios básicos siempre gratis)"},
    // ProStatusActiveAnnual
    {L"Pro Yıllık Lisans Aktif ✓", L"Pro Annual License Active ✓", L"الترخيص السنوي الاحترافي مفعّل ✓", L"Pro-Jahreslizenz aktiv ✓", L"Licence Pro annuelle active ✓", L"Lisensi Pro Tahunan Aktif ✓", L"Pro سالانہ لائسنس فعال ✓", L"Pro বার্ষিক লাইসেন্স সক্রিয় ✓", L"Lesen Pro Tahunan Aktif ✓", L"مجوز سالانه Pro فعال ✓", L"Годовая лицензия Pro активна ✓", L"Licencia Pro anual activa ✓"},
    // ProStatusActiveLifetime
    {L"Pro Ömür Boyu Lisans Aktif ✓", L"Pro Lifetime License Active ✓", L"ترخيص مدى الحياة الاحترافي مفعّل ✓", L"Lebenslange Pro-Lizenz aktiv ✓", L"Licence Pro à vie active ✓", L"Lisensi Pro Selamanya Aktif ✓", L"Pro تاحیات لائسنس فعال ✓", L"Pro আজীবন লাইসেন্স সক্রিয় ✓", L"Lesen Pro Seumur Hidup Aktif ✓", L"مجوز مادام‌العمر Pro فعال ✓", L"Бессрочная лицензия Pro активна ✓", L"Licencia Pro de por vida activa ✓"},
    // ProPlanAnnualTitle
    {L"Yıllık Pro", L"Annual Pro", L"اشتراك سنوي", L"Jährliches Pro", L"Pro Annuel", L"Pro Tahunan", L"سالانہ Pro", L"বার্ষিক Pro", L"Pro Tahunan", L"Pro سالانه", L"Pro на год", L"Pro anual"},
    // ProPlanAnnualPrice
    {L"₺100 / yıl", L"$4.99 / year", L"$4.99 / سنة", L"4,99 € / Jahr", L"4,99 € / an", L"$4.99 / tahun", L"$4.99 / سال", L"$4.99 / বছর", L"$4.99 / tahun", L"$4.99 / سال", L"$4.99 / год", L"$4.99 / año"},
    // ProPlanLifetimeTitle
    {L"Ömür Boyu Pro", L"Lifetime Pro", L"مدى الحياة", L"Lebenslanges Pro", L"Pro à Vie", L"Pro Selamanya", L"تاحیات Pro", L"আজীবন Pro", L"Pro Seumur Hidup", L"Pro مادام‌العمر", L"Pro навсегда", L"Pro de por vida"},
    // ProPlanLifetimePrice
    {L"₺250 Tek Seferlik", L"$9.99 One-Time", L"$9.99 دفعة واحدة", L"9,99 € Einmalig", L"9,99 € Paiement unique", L"$9.99 Sekali Bayar", L"$9.99 ایک بار", L"$9.99 এককালীন", L"$9.99 Sekali Bayar", L"$9.99 یک‌باره", L"$9.99 единоразово", L"$9.99 pago único"},
    // ProBadgePopular
    {L"EN ÇOK TERCİH EDİLEN", L"MOST POPULAR", L"الأكثر طلباً", L"AM BELIEBTESTEN", L"LE PLUS POPULAIRE", L"PALING POPULER", L"سب سے مقبول", L"সবচেয়ে জনপ্রিয়", L"PALING POPULAR", L"محبوب‌ترین", L"ПОПУЛЯРНОЕ", L"MÁS POPULAR"},
    // ProFeat1
    {L"• 4 Windows 11 Açılır Ekran Tasarımı (Bento Kartları + Kıble Pusulası)", L"• All 4 Windows 11 Flyout Designs (Bento Cards + Qibla Compass)", L"• 4 تصاميم نوافذ منبثقة (بطاقات + بوصلة القبلة)", L"• Alle 4 Win11-Flyout-Designs + Qibla-Kompass", L"• 4 designs de panneau + Boussole Qibla", L"• 4 Desain Popup + Kompas Kiblat", L"• ونڈوز 11 کے تمام 4 پاپ اپ ڈیزائن (بینٹو کارڈز + قبلہ کمپاس)", L"• উইন্ডোজ ১১-এর ৪টি পপআপ ডিজাইন (বেন্টো কার্ড + কিবলা কম্পাস)", L"• Semua 4 Reka Bentuk Panel Windows 11 (Kad Bento + Kompas Kiblat)", L"• هر ۴ طرح پنجره بازشوی ویندوز ۱۱ (کارت‌های بنتو + قطب‌نمای قبله)", L"• Все 4 дизайна панели Windows 11 (карточки Bento + компас киблы)", L"• Los 4 diseños de panel de Windows 11 (tarjetas Bento + brújula de Qibla)"},
    // ProFeat2
    {L"• 9 Görev Çubuğu Stili + Özel Şablon Düzenleyici", L"• All 9 Taskbar Display Styles + Custom Template Editor", L"• 9 أنماط لشريط المهام + محرر القوالب", L"• Alle 9 Taskleisten-Stile + Vorlageneditor", L"• 9 styles barre des tâches + Éditeur", L"• 9 Gaya Taskbar + Editor Templat", L"• ٹاسک بار کے تمام 9 انداز + حسب ضرورت ٹیمپلیٹ ایڈیٹر", L"• টাস্কবারের ৯টি শৈলী + কাস্টম টেমপ্লেট এডিটর", L"• Semua 9 Gaya Bar Tugas + Editor Templat Tersuai", L"• هر ۹ سبک نوار وظیفه + ویرایشگر قالب سفارشی", L"• Все 9 стилей панели задач + редактор шаблона", L"• Los 9 estilos de barra de tareas + editor de plantillas"},
    // ProFeat3
    {L"• Özel MP3/WAV Ezan Yükleme & Namaz İsimlerini Değiştirme", L"• Custom MP3/WAV Adhan Audio & Editable Prayer Names", L"• إضافة أذان MP3 مخصص وتعديل أسماء الصلوات", L"• Eigene MP3-Adhans & anpassbare Gebetsnamen", L"• Adhan MP3 personnalisé & noms modifiables", L"• Adzan MP3 Kustom & Ubah Nama Sholat", L"• حسب ضرورت MP3/WAV اذان اور قابل تدوین نماز کے نام", L"• কাস্টম MP3/WAV আজান ও নামাজের নাম সম্পাদনা", L"• Azan MP3/WAV Tersuai & Nama Solat Boleh Diubah", L"• اذان دلخواه MP3/WAV و نام‌های قابل ویرایش نماز", L"• Свой азан MP3/WAV и изменяемые названия намазов", L"• Adhan MP3/WAV propio y nombres de oración editables"},
    // ProFeat4
    {L"• Aylık & Ramazan İmsakiyesi HTML Yazdırma / CSV Dışa Aktarma", L"• Export & Print Monthly / Ramadan Schedule (CSV & HTML/PDF)", L"• تصدير وطباعة الإمساكية الشهرية ورمضان", L"• Monats- & Ramadan-Plan drucken / CSV exportieren", L"• Export CSV & Impression HTML/PDF du calendrier", L"• Ekspor CSV & Cetak Jadwal Bulanan/Ramadan", L"• ماہانہ / رمضان نظام الاوقات برآمد اور پرنٹ (CSV اور HTML/PDF)", L"• মাসিক / রমজান সূচি রপ্তানি ও প্রিন্ট (CSV ও HTML/PDF)", L"• Eksport & Cetak Jadual Bulanan / Ramadan (CSV & HTML/PDF)", L"• خروجی و چاپ جدول ماهانه / رمضان (CSV و HTML/PDF)", L"• Экспорт и печать расписания (CSV и HTML/PDF)", L"• Exportar e imprimir horario mensual / Ramadán (CSV y HTML/PDF)"},
    // ProFeat5
    {L"• Ayar Yedekleme/Geri Yükleme & 3 Bilgisayara Kadar Tek Lisans", L"• Settings JSON Backup/Restore & Future Updates Included", L"• نسخ احتياطي للإعدادات مع التحديثات المستقبلية", L"• Einstellungs-Backup & alle Updates inklusive", L"• Sauvegarde JSON & mises à jour incluses", L"• Cadangan Pengaturan & Pembaruan Gratis", L"• ترتیبات کا JSON بیک اپ/بحالی اور آئندہ اپ ڈیٹس شامل", L"• সেটিংসের JSON ব্যাকআপ/পুনরুদ্ধার ও ভবিষ্যৎ আপডেট", L"• Sandaran/Pulih Tetapan JSON & Kemas Kini Akan Datang", L"• پشتیبان‌گیری/بازیابی JSON تنظیمات و به‌روزرسانی‌های آینده", L"• Резервная копия настроек JSON и будущие обновления", L"• Copia de seguridad JSON y futuras actualizaciones incluidas"},
    // BtnBuyAnnual
    {L"Yıllık Al (₺100)", L"Get Annual ($4.99)", L"شراء سنوي ($4.99)", L"Jährlich (4,99 €)", L"Annuel (4,99 €)", L"Beli Tahunan", L"سالانہ لیں ($4.99)", L"বার্ষিক নিন ($4.99)", L"Dapatkan Tahunan ($4.99)", L"خرید سالانه ($4.99)", L"Купить на год ($4.99)", L"Obtener anual ($4.99)"},
    // BtnBuyLifetime
    {L"Ömür Boyu Al (₺250)", L"Get Lifetime ($9.99)", L"مدى الحياة ($9.99)", L"Lebenslang (9,99 €)", L"À vie (9,99 €)", L"Beli Selamanya", L"تاحیات لیں ($9.99)", L"আজীবন নিন ($9.99)", L"Dapatkan Seumur Hidup ($9.99)", L"خرید مادام‌العمر ($9.99)", L"Купить навсегда ($9.99)", L"Obtener de por vida ($9.99)"},
    // BtnOpenStore
    {L"Microsoft Store", L"Microsoft Store", L"مايكروسوفت ستور", L"Microsoft Store", L"Microsoft Store", L"Microsoft Store", L"Microsoft Store", L"Microsoft Store", L"Microsoft Store", L"Microsoft Store", L"Microsoft Store", L"Microsoft Store"},
    // ProLockedHint
    {L"Deneme süreniz doldu. Bu Pro özelliği açmak için lisans edinin — temel vakitler ücretsizdir.", L"Your free trial ended. Unlock this customization feature with Pro.", L"انتهت الفترة التجريبية. افتح هذه الميزة عبر النسخة الاحترافية.", L"Testzeit abgelaufen. Mit Pro freischalten.", L"Essai terminé. Débloquez cette fonction avec Pro.", L"Uji coba berakhir. Buka fitur ini dengan Pro.", L"آپ کا مفت ٹرائل ختم ہو گیا۔ یہ سہولت Pro کے ساتھ کھولیں۔", L"আপনার ফ্রি ট্রায়াল শেষ। Pro দিয়ে এই সুবিধাটি খুলুন।", L"Percubaan percuma anda tamat. Buka ciri ini dengan Pro.", L"دوره آزمایشی رایگان شما تمام شد. این قابلیت را با Pro باز کنید.", L"Пробный период закончился. Откройте эту функцию с Pro.", L"Su prueba gratuita terminó. Desbloquee esta función con Pro."},
    // SectionLicense
    {L"Lisans Durumu:", L"License Status:", L"حالة الترخيص:", L"Lizenzstatus:", L"Statut de licence :", L"Status Lisensi:", L"لائسنس کی حیثیت:", L"লাইসেন্সের অবস্থা:", L"Status Lesen:", L"وضعیت مجوز:", L"Статус лицензии:", L"Estado de la licencia:"},
    // BtnOpenProWindow
    {L"Pro / Lisans...", L"Pro / License...", L"الترخيص...", L"Pro / Lizenz...", L"Pro / Licence...", L"Pro / Lisensi...", L"Pro / لائسنس...", L"Pro / লাইসেন্স...", L"Pro / Lesen...", L"Pro / مجوز...", L"Pro / лицензия...", L"Pro / licencia..."},

    // BtnRestorePurchases
    {L"Satın Almaları Geri Yükle", L"Restore Purchases", L"استعادة المشتريات", L"Käufe wiederherstellen", L"Restaurer achats", L"Pulihkan Pembelian", L"خریداریاں بحال کریں", L"কেনাকাটা পুনরুদ্ধার", L"Pulihkan Pembelian", L"بازیابی خریدها", L"Восстановить покупки", L"Restaurar compras"},
    // BtnManageSubscription
    {L"Aboneliği Yönet", L"Manage Subscription", L"إدارة الاشتراك", L"Abo verwalten", L"Gérer l'abonnement", L"Kelola Langganan", L"سبسکرپشن کا انتظام", L"সাবস্ক্রিপশন পরিচালনা", L"Urus Langganan", L"مدیریت اشتراک", L"Управление подпиской", L"Gestionar suscripción"},
    // BtnRateStore
    {L"Store'da Puanla", L"Rate in Store", L"تقييم في المتجر", L"Im Store bewerten", L"Noter sur le Store", L"Beri Nilai", L"اسٹور میں ریٹ کریں", L"স্টোরে রেটিং দিন", L"Nilai di Store", L"امتیاز در فروشگاه", L"Оценить в Store", L"Valorar en la tienda"},
    // StorePurchasing
    {L"Microsoft Store ödeme penceresi açılıyor...", L"Opening Microsoft Store checkout...", L"جاري فتح نافذة الدفع في مايكروسوفت ستور...", L"Microsoft Store-Checkout wird geöffnet...", L"Ouverture du paiement Microsoft Store...", L"Membuka pembayaran Microsoft Store...", L"Microsoft Store کی ادائیگی کھل رہی ہے...", L"Microsoft Store চেকআউট খুলছে...", L"Membuka pembayaran Microsoft Store...", L"در حال باز کردن پرداخت Microsoft Store...", L"Открывается оплата в Microsoft Store...", L"Abriendo el pago de Microsoft Store..."},
    // StoreRestoring
    {L"Microsoft Store lisansları kontrol ediliyor...", L"Checking Microsoft Store licenses...", L"جاري التحقق من تراخيص مايكروسوفت ستور...", L"Microsoft Store-Lizenzen werden geprüft...", L"Vérification des licences Microsoft Store...", L"Memeriksa lisensi Microsoft Store...", L"Microsoft Store لائسنس چیک ہو رہے ہیں...", L"Microsoft Store লাইসেন্স যাচাই হচ্ছে...", L"Menyemak lesen Microsoft Store...", L"در حال بررسی مجوزهای Microsoft Store...", L"Проверка лицензий Microsoft Store...", L"Comprobando licencias de Microsoft Store..."},
    // StorePurchasedOk
    {L"Teşekkürler! PrayerBar Pro Microsoft Store üzerinden etkinleştirildi.", L"Thank you! PrayerBar Pro is now active via Microsoft Store.", L"شكراً لك! تم تفعيل الإصدار الاحترافي عبر مايكروسوفت ستور.", L"Vielen Dank! PrayerBar Pro ist über den Microsoft Store aktiv.", L"Merci ! PrayerBar Pro est actif via le Microsoft Store.", L"Terima kasih! PrayerBar Pro aktif melalui Microsoft Store.", L"شکریہ! PrayerBar Pro اب Microsoft Store کے ذریعے فعال ہے۔", L"ধন্যবাদ! PrayerBar Pro এখন Microsoft Store-এর মাধ্যমে সক্রিয়।", L"Terima kasih! PrayerBar Pro kini aktif melalui Microsoft Store.", L"سپاس! PrayerBar Pro اکنون از طریق Microsoft Store فعال است.", L"Спасибо! PrayerBar Pro активирован через Microsoft Store.", L"¡Gracias! PrayerBar Pro ya está activo mediante Microsoft Store."},
    // StorePurchaseCanceled
    {L"Satın alma iptal edildi.", L"Purchase was canceled.", L"تم إلغاء الشراء.", L"Kauf abgebrochen.", L"Achat annulé.", L"Pembelian dibatalkan.", L"خریداری منسوخ ہو گئی۔", L"কেনাকাটা বাতিল হয়েছে।", L"Pembelian dibatalkan.", L"خرید لغو شد.", L"Покупка отменена.", L"Compra cancelada."},
    // StorePurchaseFailed
    {L"Microsoft Store'a bağlanılamadı. İnternet bağlantınızı kontrol edin.", L"Could not reach Microsoft Store. Check your internet connection.", L"تعذر الاتصال بمايكروسوفت ستور. تحقق من اتصالك بالإنترنت.", L"Verbindung zum Microsoft Store fehlgeschlagen.", L"Impossible de contacter le Microsoft Store.", L"Gagal terhubung ke Microsoft Store.", L"Microsoft Store تک رسائی نہ ہو سکی۔ انٹرنیٹ چیک کریں۔", L"Microsoft Store-এ পৌঁছানো যায়নি। ইন্টারনেট দেখুন।", L"Tidak dapat menghubungi Microsoft Store. Semak internet anda.", L"دسترسی به Microsoft Store ممکن نشد. اینترنت را بررسی کنید.", L"Не удалось подключиться к Microsoft Store. Проверьте интернет.", L"No se pudo conectar con Microsoft Store. Revise su conexión."},
    // StoreNotPackaged
    {L"Mağaza içi satın alma yalnızca Microsoft Store sürümünde çalışır.", L"In-app purchase is available in the Microsoft Store version.", L"الشراء داخل التطبيق متاح في نسخة مايكروسوفت ستور.", L"In-App-Käufe sind in der Microsoft Store-Version verfügbar.", L"L'achat intégré est disponible dans la version Microsoft Store.", L"Pembelian dalam aplikasi hanya tersedia di versi Microsoft Store.", L"ایپ میں خریداری Microsoft Store ورژن میں دستیاب ہے۔", L"অ্যাপের ভেতরে কেনাকাটা Microsoft Store সংস্করণে পাওয়া যায়।", L"Pembelian dalam aplikasi tersedia dalam versi Microsoft Store.", L"خرید درون‌برنامه‌ای در نسخه Microsoft Store در دسترس است.", L"Встроенные покупки доступны в версии из Microsoft Store.", L"La compra integrada está disponible en la versión de Microsoft Store."},
    // StoreRestoredNone
    {L"Aktif Microsoft Store Pro eklentisi bulunamadı.", L"No active Microsoft Store Pro add-on found.", L"لم يتم العثور على إضافة احترافية نشطة.", L"Kein aktives Microsoft Store Pro-Add-on gefunden.", L"Aucun module Pro Microsoft Store actif trouvé.", L"Tidak ada add-on Pro Microsoft Store yang aktif.", L"کوئی فعال Microsoft Store Pro ایڈ آن نہیں ملا۔", L"কোনো সক্রিয় Microsoft Store Pro অ্যাড-অন পাওয়া যায়নি।", L"Tiada tambahan Pro Microsoft Store yang aktif ditemui.", L"هیچ افزونه فعال Pro در Microsoft Store یافت نشد.", L"Активное дополнение Pro в Microsoft Store не найдено.", L"No se encontró ningún complemento Pro activo de Microsoft Store."},
    // StoreAddonNotConfigured
    {L"Store Eklenti Kimliği henüz tanımlanmadı (Partner Center ID gerekli).", L"Store Add-on ID not configured yet (Partner Center ID required).", L"معرف الإضافة غير معرّف بعد (مطلوب Partner Center ID).", L"Store-Add-on-ID noch nicht konfiguriert.", L"ID du module complémentaire non configuré.", L"ID Add-on Store belum dikonfigurasi.", L"اسٹور ایڈ آن ID ابھی ترتیب نہیں دی گئی (Partner Center ID درکار)۔", L"স্টোর অ্যাড-অন ID এখনো সেট করা হয়নি (Partner Center ID দরকার)।", L"ID Tambahan Store belum dikonfigurasi (perlu ID Partner Center).", L"شناسه افزونه فروشگاه هنوز تنظیم نشده است (شناسه Partner Center لازم است).", L"ID дополнения Store ещё не задан (нужен ID из Partner Center).", L"El ID del complemento aún no está configurado (se requiere ID de Partner Center)."},
    // ChkFridayReminder
    {L"Cuma günü öğleden 45 dk önce Cuma namazını hatırlat", L"Remind me of Jumu'ah prayer 45 min before Dhuhr on Fridays", L"تذكير بصلاة الجمعة قبل الظهر بـ 45 دقيقة", L"Freitags 45 Min. vor Dhuhr an das Freitagsgebet erinnern", L"Rappeler la prière du vendredi 45 min avant Dhuhr", L"Ingatkan salat Jumat 45 menit sebelum Zuhur", L"جمعہ کو ظہر سے 45 منٹ پہلے نماز جمعہ کی یاد دلائیں", L"শুক্রবার জোহরের ৪৫ মিনিট আগে জুমার নামাজ মনে করিয়ে দিন", L"Ingatkan solat Jumaat 45 minit sebelum Zohor pada hari Jumaat", L"یادآوری نماز جمعه ۴۵ دقیقه پیش از ظهر در روز جمعه", L"Напоминать о джуме за 45 минут до Зухра по пятницам", L"Recordar la oración del viernes 45 min antes de Dhuhr"},
    // NotifFridayTitle
    {L"Cuma Namazı", L"Jumu'ah Prayer", L"صلاة الجمعة", L"Freitagsgebet", L"Prière du vendredi", L"Salat Jumat", L"نماز جمعہ", L"জুমার নামাজ", L"Solat Jumaat", L"نماز جمعه", L"Пятничный намаз", L"Oración del viernes"},
    // NotifFridayBody
    {L"Cuma namazına yaklaşık 45 dakika kaldı. Hayırlı Cumalar!", L"About 45 minutes until Jumu'ah. Jumu'ah Mubarak!", L"بقي نحو 45 دقيقة على صلاة الجمعة. جمعة مباركة!", L"Noch etwa 45 Minuten bis zum Freitagsgebet. Jumu'ah Mubarak!", L"Environ 45 minutes avant la prière du vendredi. Jumu'ah Moubarak !", L"Sekitar 45 menit lagi menuju salat Jumat. Jumat Berkah!", L"نماز جمعہ میں تقریباً 45 منٹ باقی ہیں۔ جمعہ مبارک!", L"জুমার নামাজের প্রায় ৪৫ মিনিট বাকি। জুমা মোবারক!", L"Kira-kira 45 minit lagi menuju solat Jumaat. Jumaat Mubarak!", L"حدود ۴۵ دقیقه تا نماز جمعه مانده است. جمعه مبارک!", L"До джума-намаза около 45 минут. Джума мубарак!", L"Faltan unos 45 minutos para la oración del viernes. ¡Yumu'a Mubarak!"},
    // NotifReviewTitle
    {L"Uygulamayı beğendiniz mi?", L"Enjoying the app?", L"هل أعجبك التطبيق؟", L"Gefällt Ihnen die App?", L"Vous aimez l'application ?", L"Suka aplikasinya?", L"کیا آپ کو ایپ پسند آئی؟", L"অ্যাপটি কি ভালো লাগছে?", L"Suka aplikasi ini?", L"از برنامه راضی هستید؟", L"Нравится приложение?", L"¿Le gusta la aplicación?"},
    // NotifReviewBody
    {L"Microsoft Store'da puan vermek için tıklayın. Desteğiniz daha çok kişiye ulaşmamızı sağlar.", L"Click to rate us on the Microsoft Store. It helps others find the app.", L"انقر لتقييمنا في مايكروسوفت ستور، فذلك يساعد الآخرين على إيجاد التطبيق.", L"Klicken Sie, um uns im Microsoft Store zu bewerten. Das hilft anderen, die App zu finden.", L"Cliquez pour nous noter sur le Microsoft Store. Cela aide d'autres personnes à trouver l'application.", L"Klik untuk memberi nilai di Microsoft Store. Ini membantu orang lain menemukan aplikasi.", L"Microsoft Store پر ریٹنگ دینے کے لیے کلک کریں۔ اس سے دوسروں کو ایپ ملتی ہے۔", L"Microsoft Store-এ রেটিং দিতে ক্লিক করুন। এতে অন্যরা অ্যাপটি খুঁজে পায়।", L"Klik untuk menilai kami di Microsoft Store. Ia membantu orang lain menemui aplikasi ini.", L"برای امتیازدهی در Microsoft Store کلیک کنید. این به دیگران کمک می‌کند برنامه را پیدا کنند.", L"Нажмите, чтобы оценить нас в Microsoft Store. Это помогает другим найти приложение.", L"Haga clic para valorarnos en Microsoft Store. Ayuda a otros a encontrar la aplicación."},
    // MenuNotifications
    {L"Bildirimler", L"Notifications", L"الإشعارات", L"Benachrichtigungen", L"Notifications", L"Notifikasi", L"اطلاعات", L"বিজ্ঞপ্তি", L"Pemberitahuan", L"اعلان‌ها", L"Уведомления", L"Notificaciones"},
    // MenuMute1h
    {L"1 saat sessize al", L"Mute for 1 hour", L"كتم لمدة ساعة", L"1 Stunde stummschalten", L"Silence pendant 1 heure", L"Bisukan 1 jam", L"1 گھنٹے کے لیے خاموش", L"১ ঘণ্টার জন্য নীরব", L"Senyapkan 1 jam", L"بی‌صدا به مدت ۱ ساعت", L"Без звука на 1 час", L"Silenciar 1 hora"},
    // MenuMuteToday
    {L"Gün sonuna kadar sessize al", L"Mute for the rest of today", L"كتم حتى نهاية اليوم", L"Für heute stummschalten", L"Silence jusqu'à la fin de la journée", L"Bisukan sampai akhir hari", L"آج دن ختم ہونے تک خاموش", L"আজ দিনের শেষ পর্যন্ত নীরব", L"Senyapkan hingga hujung hari", L"بی‌صدا تا پایان امروز", L"Без звука до конца дня", L"Silenciar el resto del día"},
    // MenuUnmute
    {L"Sessizi kapat", L"Unmute", L"إلغاء الكتم", L"Stummschaltung aufheben", L"Réactiver le son", L"Aktifkan suara", L"خاموشی ختم کریں", L"নীরবতা বন্ধ করুন", L"Nyahsenyap", L"لغو بی‌صدا", L"Включить звук", L"Reactivar"},
    // MenuMutedUntil
    {L"Sessizde · bitiş %ls", L"Muted until %ls", L"مكتوم حتى %ls", L"Stumm bis %ls", L"Silencieux jusqu'à %ls", L"Dibisukan hingga %ls", L"%ls تک خاموش", L"%ls পর্যন্ত নীরব", L"Disenyapkan hingga %ls", L"بی‌صدا تا %ls", L"Без звука до %ls", L"Silenciado hasta las %ls"},
    // MenuAppearance
    {L"Görünüm", L"Appearance", L"المظهر", L"Darstellung", L"Apparence", L"Tampilan", L"ظاہری شکل", L"চেহারা", L"Penampilan", L"ظاهر", L"Внешний вид", L"Apariencia"},
    // MenuTheme
    {L"Tema", L"Theme", L"السمة", L"Design", L"Thème", L"Tema", L"تھیم", L"থিম", L"Tema", L"پوسته", L"Тема", L"Tema"},
    // MenuShowSeconds
    {L"Saniyeleri göster", L"Show seconds", L"إظهار الثواني", L"Sekunden anzeigen", L"Afficher les secondes", L"Tampilkan detik", L"سیکنڈ دکھائیں", L"সেকেন্ড দেখান", L"Tunjuk saat", L"نمایش ثانیه", L"Показывать секунды", L"Mostrar segundos"},
    // MenuChangeLocation
    {L"Konum: %ls — Değiştir...", L"Location: %ls — Change...", L"الموقع: %ls — تغيير...", L"Standort: %ls — Ändern...", L"Lieu : %ls — Modifier...", L"Lokasi: %ls — Ubah...", L"مقام: %ls — تبدیل کریں...", L"অবস্থান: %ls — পরিবর্তন...", L"Lokasi: %ls — Tukar...", L"مکان: %ls — تغییر...", L"Место: %ls — Изменить...", L"Ubicación: %ls — Cambiar..."},
    // MenuHelp
    {L"Yardım ve Geri Bildirim", L"Help & Feedback", L"المساعدة والملاحظات", L"Hilfe & Feedback", L"Aide et commentaires", L"Bantuan & Masukan", L"مدد اور رائے", L"সহায়তা ও মতামত", L"Bantuan & Maklum Balas", L"راهنما و بازخورد", L"Справка и отзывы", L"Ayuda y comentarios"},
    // MenuAbout
    {L"PrayerBar Hakkında", L"About PrayerBar", L"حول PrayerBar", L"Über PrayerBar", L"À propos de PrayerBar", L"Tentang PrayerBar", L"PrayerBar کے بارے میں", L"PrayerBar সম্পর্কে", L"Perihal PrayerBar", L"درباره PrayerBar", L"О PrayerBar", L"Acerca de PrayerBar"},
    // AboutBody
    {L"PrayerBar sürüm %ls\n\nVakitler: Aladhan API (isteğe bağlı) veya çevrimdışı astronomik hesaplama.\nEzan sesleri kamu malıdır.\n\nKonum geçmişi veya kullanım telemetrisi saklanmaz.", L"PrayerBar version %ls\n\nPrayer times: Aladhan API (optional) or offline astronomical calculation.\nAdhan recordings are public domain.\n\nNo location history or usage telemetry is stored.", L"PrayerBar الإصدار %ls\n\nالمواقيت: واجهة Aladhan (اختياري) أو حساب فلكي دون اتصال.\nتسجيلات الأذان ملكية عامة.\n\nلا يتم تخزين سجل للموقع أو بيانات الاستخدام.", L"PrayerBar Version %ls\n\nGebetszeiten: Aladhan-API (optional) oder astronomische Offline-Berechnung.\nAdhan-Aufnahmen sind gemeinfrei.\n\nKein Standortverlauf oder Nutzungs-Telemetrie gespeichert.", L"PrayerBar version %ls\n\nHoraires : API Aladhan (facultatif) ou calcul astronomique hors ligne.\nLes enregistrements d'adhan sont du domaine public.\n\nAucun historique de position ni télémétrie d'utilisation n'est stocké.", L"PrayerBar versi %ls\n\nJadwal: API Aladhan (opsional) atau perhitungan astronomi offline.\nRekaman azan adalah domain publik.\n\nRiwayat lokasi dan telemetri penggunaan tidak disimpan.", L"PrayerBar ورژن %ls\n\nاوقات: Aladhan API (اختیاری) یا آف لائن فلکیاتی حساب۔\nاذان کی ریکارڈنگز عوامی ملکیت ہیں۔\n\nمقام کی تاریخ یا استعمال کی ٹیلی میٹری محفوظ نہیں کی جاتی۔", L"PrayerBar সংস্করণ %ls\n\nসময়: Aladhan API (ঐচ্ছিক) বা অফলাইন জ্যোতির্বিদ্যা গণনা।\nআজানের রেকর্ডিং পাবলিক ডোমেইন।\n\nঅবস্থানের ইতিহাস বা ব্যবহারের টেলিমেট্রি সংরক্ষণ করা হয় না।", L"PrayerBar versi %ls\n\nWaktu solat: API Aladhan (pilihan) atau pengiraan astronomi luar talian.\nRakaman azan ialah domain awam.\n\nSejarah lokasi atau telemetri penggunaan tidak disimpan.", L"PrayerBar نسخه %ls\n\nاوقات: API Aladhan (اختیاری) یا محاسبه نجومی آفلاین.\nضبط‌های اذان مالکیت عمومی دارند.\n\nتاریخچه مکان یا تله‌متری استفاده ذخیره نمی‌شود.", L"PrayerBar версия %ls\n\nВремя намаза: API Aladhan (по желанию) или офлайн-расчёт.\nЗаписи азана — общественное достояние.\n\nИстория местоположений и телеметрия использования не сохраняются.", L"PrayerBar versión %ls\n\nHorarios: API de Aladhan (opcional) o cálculo astronómico sin conexión.\nLas grabaciones del adhan son de dominio público.\n\nNo se guarda el historial de ubicación ni la telemetría de uso."},
    // TabFeatures
    {L"Özellikler", L"Features", L"الميزات", L"Funktionen", L"Fonctions", L"Fitur", L"خصوصیات", L"ফিচার", L"Ciri", L"ویژگی‌ها", L"Функции", L"Funciones"},
    // SectionPrayerAlerts
    {L"Vakit bildirimleri", L"Prayer alerts", L"تنبيهات الصلاة", L"Gebetsbenachrichtigungen", L"Alertes de prière", L"Pengingat sholat", L"نماز کی اطلاعات", L"নামাজের সতর্কতা", L"Peringatan solat", L"اعلان‌های نماز", L"Оповещения о намазе", L"Avisos de oración"},
    // SectionSounds
    {L"Sesler", L"Sounds", L"الأصوات", L"Töne", L"Sons", L"Suara", L"آوازیں", L"শব্দ", L"Bunyi", L"صداها", L"Звуки", L"Sonidos"},
    // LblFajrSound
    {L"Sabah (İmsak) sesi:", L"Fajr sound:", L"صوت الفجر:", L"Fadschr-Ton:", L"Son du Fajr :", L"Suara Subuh:", L"فجر کی آواز:", L"ফজরের শব্দ:", L"Bunyi Subuh:", L"صدای صبح:", L"Звук Фаджра:", L"Sonido del Fajr:"},
    // SoundSameAsEntry
    {L"Vakit sesiyle aynı", L"Same as on-time sound", L"مثل صوت دخول الوقت", L"Wie Gebetszeit-Ton", L"Identique au son de l'heure", L"Sama dengan suara waktu", L"وقت کی آواز جیسی", L"সময়ের শব্দের মতো", L"Sama seperti bunyi waktu", L"مانند صدای سر وقت", L"Как звук начала", L"Igual que el sonido a la hora"},
    // SectionReminders
    {L"Hatırlatmalar", L"Reminders", L"التذكيرات", L"Erinnerungen", L"Rappels", L"Pengingat", L"یاد دہانیاں", L"রিমাইন্ডার", L"Peringatan", L"یادآوری‌ها", L"Напоминания", L"Recordatorios"},
    // SectionDisplayTracking
    {L"Ekran ve takip", L"Display & tracking", L"العرض والمتابعة", L"Anzeige & Verlauf", L"Affichage et suivi", L"Tampilan & pelacakan", L"ڈسپلے اور ٹریکنگ", L"প্রদর্শন ও ট্র্যাকিং", L"Paparan & penjejakan", L"نمایش و پیگیری", L"Отображение и учёт", L"Pantalla y seguimiento"},
    // ChkHolidays
    {L"Dini gün ve kandil hatırlatmaları", L"Islamic holiday reminders", L"تذكير بالمناسبات الإسلامية", L"Erinnerung an islamische Feiertage", L"Rappels des fêtes islamiques", L"Pengingat hari besar Islam", L"اسلامی تہواروں کی یاد دہانی", L"ইসলামী দিবসের রিমাইন্ডার", L"Peringatan hari kebesaran Islam", L"یادآوری مناسبت‌های اسلامی", L"Напоминания об исламских праздниках", L"Recordatorios de fechas islámicas"},
    // DescHolidays
    {L"Bayram, Kandil, Kadir Gecesi, Arefe ve Ramazan başlangıcı için önceki akşam 20:00'de bildirir.", L"Notifies at 20:00 the evening before Eid, holy nights, Arafah and the start of Ramadan.", L"يُنبّه الساعة 20:00 مساء اليوم السابق للعيد والليالي المباركة وعرفة وبداية رمضان.", L"Meldet sich um 20:00 am Vorabend von Festen, heiligen Nächten, Arafa und Ramadanbeginn.", L"Prévient à 20h la veille des fêtes, nuits bénies, Arafat et du début du Ramadan.", L"Memberi tahu pukul 20:00 malam sebelum Idul Fitri/Adha, malam mulia, Arafah dan awal Ramadan.", L"عید، مقدس راتوں، عرفہ اور رمضان سے پہلی شام 20:00 بجے اطلاع دیتا ہے۔", L"ঈদ, পবিত্র রাত, আরাফা ও রমজান শুরুর আগের সন্ধ্যা ২০:০০-তে জানায়।", L"Memaklumkan pada 20:00 malam sebelum Hari Raya, malam mulia, Arafah dan awal Ramadan.", L"ساعت ۲۰:۰۰ شبِ قبل از عید، شب‌های مبارک، عرفه و آغاز رمضان اطلاع می‌دهد.", L"Сообщает в 20:00 накануне праздников, священных ночей, Арафы и начала Рамадана.", L"Avisa a las 20:00 la víspera del Eid, noches sagradas, Arafat y el inicio del Ramadán."},
    // ChkExtraTimes
    {L"Ek vakitleri göster", L"Show extra times", L"إظهار الأوقات الإضافية", L"Zusätzliche Zeiten anzeigen", L"Afficher les horaires supplémentaires", L"Tampilkan waktu tambahan", L"اضافی اوقات دکھائیں", L"অতিরিক্ত সময় দেখান", L"Tunjuk waktu tambahan", L"نمایش اوقات اضافی", L"Показывать дополнительное время", L"Mostrar horarios adicionales"},
    // DescExtraTimes
    {L"Kuşluk, gece yarısı ve teheccüd (gecenin son üçte biri) vakitlerini açılır pencerede gösterir.", L"Shows Duha, Islamic midnight and Tahajjud (last third of the night) in the flyout.", L"يعرض الضحى ومنتصف الليل والتهجد (الثلث الأخير من الليل) في النافذة.", L"Zeigt Duha, islamische Mitternacht und Tahadschud (letztes Nachtdrittel) im Panel.", L"Affiche Duha, minuit islamique et Tahajjud (dernier tiers de la nuit) dans le panneau.", L"Menampilkan Dhuha, tengah malam, dan Tahajud (sepertiga malam terakhir) di panel.", L"پینل میں چاشت، آدھی رات اور تہجد (رات کا آخری تہائی) دکھاتا ہے۔", L"প্যানেলে চাশত, মধ্যরাত ও তাহাজ্জুদ (রাতের শেষ তৃতীয়াংশ) দেখায়।", L"Menunjukkan Dhuha, tengah malam dan Tahajud (sepertiga malam terakhir) dalam panel.", L"چاشت، نیمه‌شب شرعی و تهجد (یک‌سوم پایانی شب) را در پنجره نشان می‌دهد.", L"Показывает Духа, полночь и Тахаджуд (последнюю треть ночи) в панели.", L"Muestra Duha, medianoche islámica y Tahajjud (último tercio de la noche) en el panel."},
    // ChkTravelMode
    {L"Seyahat modu", L"Travel mode", L"وضع السفر", L"Reisemodus", L"Mode voyage", L"Mode perjalanan", L"سفر موڈ", L"ভ্রমণ মোড", L"Mod perjalanan", L"حالت سفر", L"Режим путешествия", L"Modo viaje"},
    // DescTravelMode
    {L"Windows saat dilimi değişince konumu güncellemeniz için uyarır.", L"Warns you to update your location when the Windows time zone changes.", L"ينبهك لتحديث موقعك عند تغيير المنطقة الزمنية في ويندوز.", L"Erinnert ans Aktualisieren des Standorts, wenn sich die Zeitzone ändert.", L"Vous invite à mettre à jour le lieu quand le fuseau horaire change.", L"Mengingatkan untuk memperbarui lokasi saat zona waktu Windows berubah.", L"ونڈوز کا ٹائم زون بدلنے پر مقام اپ ڈیٹ کرنے کی یاد دلاتا ہے۔", L"উইন্ডোজের টাইম জোন বদলালে অবস্থান হালনাগাদ করতে মনে করায়।", L"Mengingatkan anda mengemas kini lokasi apabila zon waktu Windows berubah.", L"با تغییر منطقه زمانی ویندوز، برای به‌روزرسانی مکان هشدار می‌دهد.", L"Предлагает обновить место при смене часового пояса Windows.", L"Avisa para actualizar la ubicación cuando cambia la zona horaria de Windows."},
    // ChkTracking
    {L"Namaz takibi", L"Prayer tracking", L"متابعة الصلوات", L"Gebetsverlauf", L"Suivi des prières", L"Pelacak sholat", L"نماز کا ریکارڈ", L"নামাজ ট্র্যাকিং", L"Penjejak solat", L"پیگیری نماز", L"Учёт намазов", L"Seguimiento de oraciones"},
    // DescTracking
    {L"Açılır pencerede vakte tıklayarak \"kıldım\" işaretleyin; haftalık özet gösterilir.", L"Click a prayer in the flyout to mark it as prayed; a weekly summary is shown.", L"انقر على الصلاة في النافذة لتعليمها كمؤداة؛ يظهر ملخص أسبوعي.", L"Gebet im Panel anklicken, um es als verrichtet zu markieren; mit Wochenübersicht.", L"Cliquez sur une prière dans le panneau pour la marquer comme accomplie ; résumé hebdomadaire.", L"Klik sholat di panel untuk menandai sudah dikerjakan; ringkasan mingguan ditampilkan.", L"پینل میں نماز پر کلک کر کے ادا شدہ نشان لگائیں؛ ہفتہ وار خلاصہ دکھایا جاتا ہے۔", L"প্যানেলে নামাজে ক্লিক করে আদায় চিহ্নিত করুন; সাপ্তাহিক সারাংশ দেখানো হয়।", L"Klik solat dalam panel untuk tandakan sudah ditunaikan; ringkasan mingguan dipaparkan.", L"با کلیک روی نماز در پنجره آن را «خوانده شد» علامت بزنید؛ خلاصه هفتگی نمایش داده می‌شود.", L"Нажмите на намаз в панели, чтобы отметить его; показывается итог недели.", L"Haga clic en una oración del panel para marcarla como realizada; se muestra un resumen semanal."},
    // ChkModernToast
    {L"Modern Windows bildirimleri", L"Modern Windows notifications", L"إشعارات ويندوز الحديثة", L"Moderne Windows-Benachrichtigungen", L"Notifications Windows modernes", L"Notifikasi Windows modern", L"جدید ونڈوز اطلاعات", L"আধুনিক উইন্ডোজ বিজ্ঞপ্তি", L"Pemberitahuan Windows moden", L"اعلان‌های مدرن ویندوز", L"Современные уведомления Windows", L"Notificaciones modernas de Windows"},
    // DescModernToast
    {L"Ertele ve Kapat düğmeleri; Bildirim Merkezi'nde kalır. Kapalıyken klasik balon kullanılır.", L"Snooze and Dismiss buttons; stays in Notification Center. Off = classic balloon.", L"أزرار الغفوة والإغلاق؛ يبقى في مركز الإشعارات. عند الإيقاف تُستخدم الفقاعة التقليدية.", L"Schlummern- und Schließen-Knöpfe; bleibt im Infocenter. Aus = klassische Sprechblase.", L"Boutons Répéter et Ignorer ; reste dans le centre de notifications. Désactivé = bulle classique.", L"Tombol Tunda dan Tutup; tetap di Pusat Notifikasi. Mati = balon klasik.", L"اسنوز اور بند کرنے کے بٹن؛ نوٹیفکیشن سینٹر میں رہتی ہیں۔ بند ہو تو پرانا بیلون۔", L"স্নুজ ও বন্ধ বোতাম; নোটিফিকেশন সেন্টারে থাকে। বন্ধ থাকলে পুরনো বেলুন।", L"Butang Tunda dan Tutup; kekal di Pusat Pemberitahuan. Mati = belon klasik.", L"دکمه‌های تعویق و بستن؛ در مرکز اعلان می‌ماند. خاموش = حباب قدیمی.", L"Кнопки «Отложить» и «Закрыть»; остаются в центре уведомлений. Выкл. = старое всплывающее окно.", L"Botones Posponer y Descartar; se queda en el Centro de notificaciones. Desactivado = globo clásico."},
    // ChkHotkey
    {L"Klavye kısayolu", L"Keyboard shortcut", L"اختصار لوحة المفاتيح", L"Tastenkürzel", L"Raccourci clavier", L"Pintasan keyboard", L"کی بورڈ شارٹ کٹ", L"কীবোর্ড শর্টকাট", L"Pintasan papan kekunci", L"میان‌بر صفحه‌کلید", L"Сочетание клавиш", L"Atajo de teclado"},
    // DescHotkey
    {L"Vakit penceresini her yerden açıp kapatır.", L"Opens or closes the prayer panel from anywhere.", L"يفتح نافذة المواقيت أو يغلقها من أي مكان.", L"Öffnet oder schließt das Gebetspanel von überall.", L"Ouvre ou ferme le panneau des horaires depuis n'importe où.", L"Membuka atau menutup panel jadwal dari mana saja.", L"کہیں سے بھی اوقات کا پینل کھولتا یا بند کرتا ہے۔", L"যেকোনো জায়গা থেকে সময়ের প্যানেল খোলে বা বন্ধ করে।", L"Membuka atau menutup panel waktu dari mana-mana.", L"پنجره اوقات را از هر جا باز یا بسته می‌کند.", L"Открывает и закрывает панель из любого места.", L"Abre o cierra el panel de horarios desde cualquier lugar."},
    // ExtraDuha
    {L"Kuşluk", L"Duha", L"الضحى", L"Duha", L"Duha", L"Dhuha", L"چاشت", L"চাশত", L"Dhuha", L"چاشت", L"Духа", L"Duha"},
    // ExtraMidnight
    {L"Gece yarısı", L"Midnight", L"منتصف الليل", L"Mitternacht", L"Minuit", L"Tengah malam", L"آدھی رات", L"মধ্যরাত", L"Tengah malam", L"نیمه‌شب", L"Полночь", L"Medianoche"},
    // ExtraTahajjud
    {L"Teheccüd", L"Tahajjud", L"التهجد", L"Tahadschud", L"Tahajjud", L"Tahajud", L"تہجد", L"তাহাজ্জুদ", L"Tahajud", L"تهجد", L"Тахаджуд", L"Tahajjud"},
    // HolHijriNewYear
    {L"Hicri Yılbaşı", L"Islamic New Year", L"رأس السنة الهجرية", L"Islamisches Neujahr", L"Nouvel An hégirien", L"Tahun Baru Islam", L"اسلامی نیا سال", L"হিজরি নববর্ষ", L"Maal Hijrah", L"سال نو هجری قمری", L"Исламский Новый год", L"Año Nuevo islámico"},
    // HolAshura
    {L"Aşure Günü", L"Day of Ashura", L"يوم عاشوراء", L"Aschura-Tag", L"Jour d'Achoura", L"Hari Asyura", L"یوم عاشور", L"আশুরা", L"Hari Asyura", L"روز عاشورا", L"День Ашура", L"Día de Ashura"},
    // HolMawlid
    {L"Mevlid Kandili", L"Mawlid an-Nabi", L"المولد النبوي", L"Maulid-Nacht", L"Mawlid an-Nabi", L"Maulid Nabi", L"عید میلاد النبی", L"ঈদে মিলাদুন্নবী", L"Maulidur Rasul", L"شب میلاد پیامبر", L"Мавлид ан-Наби", L"Mawlid an-Nabi"},
    // HolRaghaib
    {L"Regaib Kandili", L"Night of Raghaib", L"ليلة الرغائب", L"Nacht von Regaib", L"Nuit de Raghaib", L"Malam Raghaib", L"شب رغائب", L"রাগায়েবের রাত", L"Malam Raghaib", L"شب رغائب", L"Ночь Рагаиб", L"Noche de Raghaib"},
    // HolMiraj
    {L"Miraç Kandili", L"Isra and Mi'raj", L"الإسراء والمعراج", L"Nacht der Himmelfahrt (Miradsch)", L"Nuit du Mi'raj", L"Isra Mi'raj", L"شب معراج", L"শবে মেরাজ", L"Israk Mikraj", L"شب معراج", L"Ночь Мирадж", L"Isra y Mi'raj"},
    // HolBarat
    {L"Berat Kandili", L"Laylat al-Bara'ah", L"ليلة النصف من شعبان", L"Nacht von Berat", L"Nuit du pardon (Bara'a)", L"Nisfu Syaban", L"شب برات", L"শবে বরাত", L"Nisfu Syaaban", L"شب نیمه شعبان", L"Ночь Бараат", L"Noche de Bara'a"},
    // HolRamadanStart
    {L"Ramazan'ın ilk günü", L"First day of Ramadan", L"أول أيام رمضان", L"Erster Tag des Ramadan", L"Premier jour du Ramadan", L"Hari pertama Ramadan", L"رمضان کا پہلا دن", L"রমজানের প্রথম দিন", L"Hari pertama Ramadan", L"نخستین روز رمضان", L"Первый день Рамадана", L"Primer día de Ramadán"},
    // HolQadr
    {L"Kadir Gecesi", L"Laylat al-Qadr", L"ليلة القدر", L"Nacht der Bestimmung", L"Nuit du Destin", L"Lailatul Qadar", L"شب قدر", L"শবে কদর", L"Lailatulqadar", L"شب قدر", L"Ночь Предопределения", L"Noche del Destino"},
    // HolEidFitr
    {L"Ramazan Bayramı", L"Eid al-Fitr", L"عيد الفطر", L"Ramadanfest", L"Aïd al-Fitr", L"Idul Fitri", L"عید الفطر", L"ঈদুল ফিতর", L"Hari Raya Aidilfitri", L"عید فطر", L"Ураза-байрам", L"Eid al-Fitr"},
    // HolArafah
    {L"Arefe Günü", L"Day of Arafah", L"يوم عرفة", L"Tag von Arafa", L"Jour d'Arafat", L"Hari Arafah", L"یوم عرفہ", L"আরাফার দিন", L"Hari Arafah", L"روز عرفه", L"День Арафа", L"Día de Arafat"},
    // HolEidAdha
    {L"Kurban Bayramı", L"Eid al-Adha", L"عيد الأضحى", L"Opferfest", L"Aïd al-Adha", L"Idul Adha", L"عید الاضحیٰ", L"ঈদুল আযহা", L"Hari Raya Aidiladha", L"عید قربان", L"Курбан-байрам", L"Eid al-Adha"},
    // HolidayTitle
    {L"Dini gün hatırlatması", L"Islamic holiday reminder", L"تذكير بمناسبة دينية", L"Erinnerung: islamischer Feiertag", L"Rappel de fête islamique", L"Pengingat hari besar Islam", L"اسلامی دن کی یاد دہانی", L"ইসলামী দিবসের রিমাইন্ডার", L"Peringatan hari kebesaran Islam", L"یادآوری مناسبت دینی", L"Напоминание о празднике", L"Recordatorio de fecha islámica"},
    // HolidayTonight
    {L"Bu gece: %ls", L"Tonight: %ls", L"الليلة: %ls", L"Heute Nacht: %ls", L"Ce soir : %ls", L"Malam ini: %ls", L"آج رات: %ls", L"আজ রাতে: %ls", L"Malam ini: %ls", L"امشب: %ls", L"Сегодня ночью: %ls", L"Esta noche: %ls"},
    // HolidayTomorrow
    {L"Yarın: %ls", L"Tomorrow: %ls", L"غدًا: %ls", L"Morgen: %ls", L"Demain : %ls", L"Besok: %ls", L"کل: %ls", L"আগামীকাল: %ls", L"Esok: %ls", L"فردا: %ls", L"Завтра: %ls", L"Mañana: %ls"},
    // TravelTitle
    {L"Saat diliminiz değişti", L"Your time zone changed", L"تغيّرت منطقتك الزمنية", L"Ihre Zeitzone hat sich geändert", L"Votre fuseau horaire a changé", L"Zona waktu Anda berubah", L"آپ کا ٹائم زون بدل گیا", L"আপনার টাইম জোন বদলেছে", L"Zon waktu anda berubah", L"منطقه زمانی شما تغییر کرد", L"Часовой пояс изменился", L"Su zona horaria cambió"},
    // TravelBody
    {L"Vakitlerin doğru olması için konumunuzu güncelleyin. Ayarları açmak için tıklayın.", L"Update your location so prayer times stay correct. Click to open settings.", L"حدّث موقعك لتبقى المواقيت صحيحة. انقر لفتح الإعدادات.", L"Aktualisieren Sie den Standort, damit die Zeiten stimmen. Klicken öffnet die Einstellungen.", L"Mettez à jour le lieu pour garder des horaires exacts. Cliquez pour ouvrir les paramètres.", L"Perbarui lokasi agar jadwal tetap tepat. Klik untuk membuka pengaturan.", L"اوقات درست رکھنے کے لیے مقام اپ ڈیٹ کریں۔ ترتیبات کھولنے کے لیے کلک کریں۔", L"সঠিক সময়ের জন্য অবস্থান হালনাগাদ করুন। সেটিংস খুলতে ক্লিক করুন।", L"Kemas kini lokasi supaya waktu solat tepat. Klik untuk membuka tetapan.", L"برای درست ماندن اوقات، مکان را به‌روز کنید. برای باز کردن تنظیمات کلیک کنید.", L"Обновите место, чтобы время было точным. Нажмите, чтобы открыть настройки.", L"Actualice la ubicación para que los horarios sean correctos. Haga clic para abrir la configuración."},
    // TrackWeek
    {L"Bu hafta: %ls", L"This week: %ls", L"هذا الأسبوع: %ls", L"Diese Woche: %ls", L"Cette semaine : %ls", L"Minggu ini: %ls", L"اس ہفتے: %ls", L"এই সপ্তাহে: %ls", L"Minggu ini: %ls", L"این هفته: %ls", L"За неделю: %ls", L"Esta semana: %ls"},
    // HotkeyPressKeys
    {L"Tuş kombinasyonuna basın…", L"Press a key combination…", L"اضغط مجموعة مفاتيح…", L"Tastenkombination drücken…", L"Appuyez sur une combinaison…", L"Tekan kombinasi tombol…", L"کیز کا مجموعہ دبائیں…", L"কী কম্বিনেশন চাপুন…", L"Tekan gabungan kekunci…", L"ترکیب کلید را فشار دهید…", L"Нажмите сочетание клавиш…", L"Pulse una combinación…"},
    // HotkeyInUse
    {L"Bu kısayol başka bir uygulamada kullanılıyor; başka bir kombinasyon seçin.", L"This shortcut is used by another app; pick another combination.", L"هذا الاختصار مستخدم في تطبيق آخر؛ اختر مجموعة أخرى.", L"Dieses Kürzel nutzt bereits eine andere App; wählen Sie ein anderes.", L"Ce raccourci est utilisé par une autre application ; choisissez-en un autre.", L"Pintasan ini dipakai aplikasi lain; pilih kombinasi lain.", L"یہ شارٹ کٹ کوئی اور ایپ استعمال کر رہی ہے؛ دوسرا مجموعہ منتخب کریں۔", L"এই শর্টকাট অন্য অ্যাপ ব্যবহার করছে; অন্য কম্বিনেশন বেছে নিন।", L"Pintasan ini digunakan oleh aplikasi lain; pilih gabungan lain.", L"این میان‌بر را برنامه دیگری استفاده می‌کند؛ ترکیب دیگری انتخاب کنید.", L"Это сочетание занято другим приложением; выберите другое.", L"Otra aplicación usa este atajo; elija otra combinación."},
    // HotkeyNeedsModifier
    {L"En az bir Ctrl, Alt, Shift veya Win tuşuyla birlikte basın.", L"Include at least one of Ctrl, Alt, Shift or Win.", L"أضف على الأقل Ctrl أو Alt أو Shift أو Win.", L"Mindestens Strg, Alt, Umschalt oder Win mitdrücken.", L"Incluez au moins Ctrl, Alt, Maj ou Win.", L"Sertakan minimal Ctrl, Alt, Shift, atau Win.", L"کم از کم Ctrl، Alt، Shift یا Win شامل کریں۔", L"অন্তত Ctrl, Alt, Shift বা Win যোগ করুন।", L"Sertakan sekurang-kurangnya Ctrl, Alt, Shift atau Win.", L"دست‌کم یکی از Ctrl، Alt، Shift یا Win را اضافه کنید.", L"Добавьте хотя бы Ctrl, Alt, Shift или Win.", L"Incluya al menos Ctrl, Alt, Mayús o Win."},
    // TabProLicense
    {L"Pro & Lisans", L"Pro & License", L"Pro والترخيص", L"Pro & Lizenz", L"Pro & Licence", L"Pro & Lisensi", L"Pro اور لائسنس", L"Pro ও লাইসেন্স", L"Pro & Lesen", L"Pro و مجوز", L"Pro и лицензия", L"Pro y licencia"},
    // ProBadgeFlexible
    {L"ESNEK", L"FLEXIBLE", L"مرن", L"FLEXIBEL", L"FLEXIBLE", L"FLEKSIBEL", L"لچکدار", L"নমনীয়", L"FLEKSIBEL", L"انعطاف‌پذیر", L"ГИБКИЙ", L"FLEXIBLE"},
    // ProAnnualSub
    {L"Her yıl yenilenir, istediğiniz zaman iptal", L"Renews yearly, cancel anytime", L"يتجدد سنوياً، إلغاء في أي وقت", L"Jährliche Verlängerung, jederzeit kündbar", L"Renouvellement annuel, annulable à tout moment", L"Diperpanjang tahunan, batalkan kapan saja", L"ہر سال تجدید، کسی بھی وقت منسوخ کریں", L"প্রতি বছর নবায়ন, যেকোনো সময় বাতিল", L"Diperbaharui setiap tahun, batal bila-bila masa", L"تمدید سالانه، لغو در هر زمان", L"Ежегодное продление, отмена в любое время", L"Se renueva al año, cancela cuando quieras"},
    // ProLifetimeSub
    {L"Bir kez öde, sonsuza kadar senin", L"Pay once, yours forever", L"ادفع مرة واحدة، ملكك للأبد", L"Einmal zahlen, für immer deins", L"Payez une fois, à vous pour toujours", L"Bayar sekali, milik Anda selamanya", L"ایک بار ادا کریں، ہمیشہ کے لیے آپ کا", L"একবার পরিশোধ করুন, চিরদিনের জন্য আপনার", L"Bayar sekali, milik anda selamanya", L"یک بار پرداخت کنید، برای همیشه از آن شماست", L"Оплатите один раз и пользуйтесь всегда", L"Paga una vez y es tuyo para siempre"},
    // ProBulletAllFeatures
    {L"Tüm Pro özellikleri", L"All Pro features", L"جميع ميزات Pro", L"Alle Pro-Funktionen", L"Toutes les fonctions Pro", L"Semua fitur Pro", L"تمام Pro خصوصیات", L"সব Pro সুবিধা", L"Semua ciri Pro", L"همه امکانات Pro", L"Все функции Pro", L"Todas las funciones Pro"},
    // ProBulletSubUpdates
    {L"Abonelik süresince güncellemeler", L"Updates during subscription", L"تحديثات طوال فترة الاشتراك", L"Updates während des Abos", L"Mises à jour pendant l'abonnement", L"Pembaruan selama berlangganan", L"سبسکرپشن کے دوران اپ ڈیٹس", L"সাবস্ক্রিপশন চলাকালীন আপডেট", L"Kemas kini sepanjang langganan", L"به‌روزرسانی در طول اشتراک", L"Обновления во время подписки", L"Actualizaciones durante la suscripción"},
    // ProBulletCancelAnytime
    {L"İstediğin zaman iptal et", L"Cancel anytime", L"إلغاء في أي وقت", L"Jederzeit kündbar", L"Annulation à tout moment", L"Batalkan kapan saja", L"کسی بھی وقت منسوخ کریں", L"যেকোনো সময় বাতিল করুন", L"Batal pada bila-bila masa", L"لغو در هر زمان", L"Отмена в любое время", L"Cancela en cualquier momento"},
    // ProBulletAllUpdates
    {L"Gelecekteki tüm güncellemeler dahil", L"All future updates included", L"يشمل جميع التحديثات المستقبلية", L"Alle künftigen Updates inklusive", L"Toutes les futures mises à jour incluses", L"Termasuk semua pembaruan mendatang", L"آئندہ تمام اپ ڈیٹس شامل ہیں", L"ভবিষ্যতের সব আপডেট অন্তর্ভুক্ত", L"Semua kemas kini akan datang disertakan", L"تمام به‌روزرسانی‌های آینده شامل است", L"Все будущие обновления включены", L"Todas las actualizaciones futuras incluidas"},
    // ProBulletOneTime
    {L"Tek ödeme, yenileme yok", L"One-time payment, no renewal", L"دفعة واحدة بدون تجديد", L"Einmalzahlung, keine Verlängerung", L"Paiement unique, sans renouvellement", L"Sekali bayar, tanpa perpanjangan", L"ایک بار ادائیگی، کوئی تجدید نہیں", L"এককালীন অর্থপ্রদান, কোনো নবায়ন নেই", L"Bayaran sekali, tiada pembaharuan", L"پرداخت یک‌باره، بدون تمدید", L"Разовый платёж, без продления", L"Pago único, sin renovación"},
    // ProBulletBestValue
    {L"Uzun vadede en ekonomik seçenek", L"Best long-term value", L"الخيار الأوفر على المدى الطويل", L"Langfristig die günstigste Wahl", L"Le choix le plus économique à long terme", L"Pilihan paling hemat jangka panjang", L"طویل مدت کے لیے سب سے کفایتی انتخاب", L"দীর্ঘমেয়াদে সবচেয়ে সাশ্রয়ী বিকল্প", L"Pilihan paling jimat jangka panjang", L"به‌صرفه‌ترین گزینه در بلندمدت", L"Самый выгодный вариант надолго", L"La opción más rentable a largo plazo"},
    // ProIncludedBoth
    {L"Her iki planda da:", L"Included in both plans:", L"متضمن في كلا الخطتين:", L"In beiden Tarifen enthalten:", L"Inclus dans les deux offres :", L"Termasuk dalam kedua paket:", L"دونوں منصوبوں میں شامل:", L"উভয় প্ল্যানেই অন্তর্ভুক্ত:", L"Disertakan dalam kedua-dua pelan:", L"در هر دو طرح شامل است:", L"Включено в оба тарифа:", L"Incluido en ambos planes:"},
    // LblPillColorMode
    {L"Görev Çubuğu Rengi:", L"Taskbar Pill Color:", L"لون شريط المهام:", L"Taskleisten-Farbe:", L"Couleur barre des tâches :", L"Warna Taskbar:", L"ٹاسک بار کا رنگ:", L"টাস্কবার রঙ:", L"Warna Bar Tugas:", L"رنگ نوار وظیفه:", L"Цвет на панели задач:", L"Color en barra de tareas:"},
    // PillColorEmerald
    {L"Zümrüt Yeşil (AMOLED)", L"Emerald Green (AMOLED)", L"أخضر زمردي (AMOLED)", L"Smaragdgrün (AMOLED)", L"Vert émeraude (AMOLED)", L"Hijau Zamrud (AMOLED)", L"زمرد سبز (AMOLED)", L"পান্না সবুজ (AMOLED)", L"Hijau Zamrud (AMOLED)", L"سبز زمردی (AMOLED)", L"Изумрудный (AMOLED)", L"Verde esmeralda (AMOLED)"},
    // PillColorTheme
    {L"Tema Rengine Uyumlu", L"Adaptive Theme Color", L"متوافق مع لون السمة", L"An Themenfarbe angepasst", L"Adapté au thème", L"Sesuai Warna Tema", L"تھیم کے رنگ کے مطابق", L"থিমের রঙ অনুযায়ী", L"Ikut Warna Tema", L"هماهنگ با رنگ پوسته", L"В цвет темы", L"Adaptado al tema"},
    // PillColorMinimal
    {L"Sade Şeffaf (Arka Plansız)", L"Minimal Transparent", L"شفاف بسيط", L"Minimal transparent", L"Minimal transparent", L"Minimal Transparan", L"سادہ شفاف", L"মিনিমাল স্বচ্ছ", L"Minimal Lutsinar", L"شفاف و ساده", L"Прозрачный", L"Mínimo transparente"},
    // LblFlyoutSize
    {L"Açılır Ekran Boyutu:", L"Flyout Panel Size:", L"حجم النافذة المنبثقة:", L"Flyout-Größe:", L"Taille du panneau :", L"Ukuran Panel Popup:", L"پاپ اپ پینل کا سائز:", L"পপআপ প্যানেলের আকার:", L"Saiz Panel:", L"اندازه پنجره بازشو:", L"Размер панели:", L"Tamaño del panel:"},
    // FlyoutSizeSmall
    {L"Küçük", L"Small", L"صغير", L"Klein", L"Petit", L"Kecil", L"چھوٹا", L"ছোট", L"Kecil", L"کوچک", L"Малый", L"Pequeño"},
    // FlyoutSizeNormal
    {L"Normal", L"Normal", L"عادي", L"Normal", L"Normal", L"Normal", L"نارمل", L"স্বাভাবিক", L"Biasa", L"معمولی", L"Обычный", L"Normal"},
    // FlyoutSizeLarge
    {L"Büyük", L"Large", L"كبير", L"Groß", L"Grand", L"Besar", L"بڑا", L"বড়", L"Besar", L"بزرگ", L"Большой", L"Grande"},
    // FlyoutSizeXLarge
    {L"Çok Büyük", L"Extra Large", L"كبير جداً", L"Sehr groß", L"Très grand", L"Sangat Besar", L"بہت بڑا", L"অতিরিক্ত বড়", L"Sangat Besar", L"خیلی بزرگ", L"Очень большой", L"Muy grande"},
    // ToastStopAdhan
    {L"Ezanı Sustur", L"Mute Adhan", L"إيقاف الأذان", L"Adhan stummschalten", L"Couper l'adhan", L"Hentikan Azan", L"اذان بند کریں", L"আজান বন্ধ করুন", L"Hentikan Azan", L"قطع اذان", L"Выключить азан", L"Silenciar adhan"},
    // BalloonStopHint
    {L"Sesi durdurmak için bildirime tıklayın.", L"Click notification to stop audio.", L"انقر على الإشعار لإيقاف الصوت.", L"Klicken, um den Ton zu stoppen.", L"Cliquez pour couper le son.", L"Klik notifikasi untuk menghentikan suara.", L"آواز بند کرنے کے لیے اطلاع پر کلک کریں۔", L"শব্দ বন্ধ করতে বিজ্ঞপ্তিতে ক্লিক করুন।", L"Klik pemberitahuan untuk hentikan bunyi.", L"برای قطع صدا روی اعلان کلیک کنید.", L"Нажмите на уведомление, чтобы остановить звук.", L"Haga clic en la notificación para detener el sonido."},
    // ChkTaskbarFrame
    {L"Görev çubuğu çerçevesini göster", L"Show taskbar frame", L"إظهار إطار شريط المهام", L"Taskleistenrahmen anzeigen", L"Afficher le cadre de la barre", L"Tampilkan bingkai bilah tugas", L"ٹاسک بار فریم دکھائیں", L"টাস্কবার ফ্রেম দেখান", L"Tunjukkan bingkai bar tugas", L"نمایش قاب نوار وظیفه", L"Показывать рамку на панели", L"Mostrar marco en la barra"},
    // ChkTaskbarLogo
    {L"Görev çubuğunda logo göster", L"Show logo in taskbar", L"إظهار الشعار في شريط المهام", L"Logo in Taskleiste anzeigen", L"Afficher le logo dans la barre", L"Tampilkan logo di bilah tugas", L"ٹاسک بار میں لوگو دکھائیں", L"টাস্কবারে লোগো দেখান", L"Tunjukkan logo di bar tugas", L"نمایش لوگو در نوار وظیفه", L"Показывать логотип на панели", L"Mostrar logotipo en la barra"}
};

static_assert(sizeof(kUiStrings) / sizeof(kUiStrings[0]) == static_cast<size_t>(StrId::ChkTaskbarLogo) + 1, "kUiStrings mismatch");

constexpr std::array<LangRow, 6> kPrayerNames = {{
    {L"İmsak",  L"Fajr",    L"الفجر",   L"Fadschr",  L"Fajr",    L"Subuh", L"فجر", L"ফজর", L"Subuh", L"صبح", L"Фаджр", L"Fajr"},
    {L"Güneş",  L"Sunrise", L"الشروق",  L"Sonne",    L"Lever",   L"Terbit", L"طلوع آفتاب", L"সূর্যোদয়", L"Syuruk", L"طلوع", L"Восход", L"Amanecer"},
    {L"Öğle",   L"Dhuhr",   L"الظهر",   L"Dhuhr",    L"Dhuhr",   L"Dzuhur", L"ظہر", L"জোহর", L"Zohor", L"ظهر", L"Зухр", L"Dhuhr"},
    {L"İkindi", L"Asr",     L"العصر",   L"Asr",      L"Asr",     L"Ashar", L"عصر", L"আসর", L"Asar", L"عصر", L"Аср", L"Asr"},
    {L"Akşam",  L"Maghrib", L"المغرب",  L"Maghrib",  L"Maghrib", L"Maghrib", L"مغرب", L"মাগরিব", L"Maghrib", L"مغرب", L"Магриб", L"Maghrib"},
    {L"Yatsı",  L"Isha",    L"العشاء",  L"Ischa",    L"Isha",    L"Isya", L"عشاء", L"এশা", L"Isyak", L"عشا", L"Иша", L"Isha"}
}};

constexpr std::array<LangRow, 6> kPrayerShortCodes = {{
    {L"İm", L"F", L"ف", L"F", L"F", L"Sb", L"ف", L"ফ", L"Sb", L"ص", L"Ф", L"F"},
    {L"G",  L"S", L"ش", L"S", L"L", L"Tb", L"ط", L"সূ", L"Sy", L"ط", L"В", L"A"},
    {L"Ö",  L"D", L"ظ", L"D", L"D", L"Dz", L"ظ", L"জো", L"Z", L"ظ", L"З", L"D"},
    {L"İ",  L"A", L"ع", L"A", L"A", L"As", L"ع", L"আ", L"As", L"ع", L"Ас", L"As"},
    {L"A",  L"M", L"م", L"M", L"M", L"Mg", L"م", L"মা", L"M", L"م", L"М", L"M"},
    {L"Y",  L"I", L"عش", L"I", L"I", L"Is", L"عش", L"এ", L"Is", L"عش", L"И", L"I"}
}};

} // namespace

AppLanguage ResolveEffectiveLanguage(AppLanguage configured) {
    if (configured != AppLanguage::Auto) {
        int val = static_cast<int>(configured);
        if (val >= 1 && val <= 12) return configured;
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
        case LANG_URDU:       return AppLanguage::Urdu;
        case LANG_BENGALI:    return AppLanguage::Bengali;
        case LANG_MALAY:      return AppLanguage::Malay;
        case LANG_PERSIAN:    return AppLanguage::Persian;
        case LANG_RUSSIAN:    return AppLanguage::Russian;
        case LANG_SPANISH:    return AppLanguage::Spanish;
        case LANG_ENGLISH:    return AppLanguage::English;
        default:              return AppLanguage::English;
    }
}

bool IsRtlLanguage(AppLanguage lang) {
    AppLanguage eff = ResolveEffectiveLanguage(lang);
    return eff == AppLanguage::Arabic || eff == AppLanguage::Urdu || eff == AppLanguage::Persian;
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
        case AppLanguage::Urdu:       return L"اردو (Urdu)";
        case AppLanguage::Bengali:    return L"বাংলা (Bengali)";
        case AppLanguage::Malay:      return L"Bahasa Melayu";
        case AppLanguage::Persian:    return L"فارسی (Persian)";
        case AppLanguage::Russian:    return L"Русский";
        case AppLanguage::Spanish:    return L"Español";
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
                case AppLanguage::Urdu:       return L"افطار تک باقی";
                case AppLanguage::Bengali:    return L"ইফতার পর্যন্ত";
                case AppLanguage::Malay:      return L"Menuju Berbuka";
                case AppLanguage::Persian:    return L"تا افطار";
                case AppLanguage::Russian:    return L"До ифтара";
                case AppLanguage::Spanish:    return L"Hasta el Iftar";
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
                case AppLanguage::Urdu:       return L"سحری تک باقی";
                case AppLanguage::Bengali:    return L"সাহরি পর্যন্ত";
                case AppLanguage::Malay:      return L"Menuju Sahur";
                case AppLanguage::Persian:    return L"تا سحر";
                case AppLanguage::Russian:    return L"До сухура";
                case AppLanguage::Spanish:    return L"Hasta el Suhur";
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
        case AppLanguage::Urdu:
            return GetEffectivePrayerName(kind, settings) + L" تک باقی";
        case AppLanguage::Bengali:
            return GetEffectivePrayerName(kind, settings) + L" পর্যন্ত";
        case AppLanguage::Malay:
            return L"Menuju " + GetEffectivePrayerName(kind, settings);
        case AppLanguage::Persian:
            return L"تا " + GetEffectivePrayerName(kind, settings);
        case AppLanguage::Russian:
            return L"До " + GetEffectivePrayerName(kind, settings);
        case AppLanguage::Spanish:
            return L"Hasta " + GetEffectivePrayerName(kind, settings);
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
                case AppLanguage::Urdu:       return L"افطار تک باقی وقت";
                case AppLanguage::Bengali:    return L"ইফতার পর্যন্ত বাকি";
                case AppLanguage::Malay:      return L"MENUJU BERBUKA";
                case AppLanguage::Persian:    return L"زمان باقی‌مانده تا افطار";
                case AppLanguage::Russian:    return L"ДО ИФТАРА";
                case AppLanguage::Spanish:    return L"HASTA EL IFTAR";
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
                case AppLanguage::Urdu:       return L"سحری تک باقی وقت";
                case AppLanguage::Bengali:    return L"সাহরি পর্যন্ত বাকি";
                case AppLanguage::Malay:      return L"MENUJU SAHUR";
                case AppLanguage::Persian:    return L"زمان باقی‌مانده تا سحر";
                case AppLanguage::Russian:    return L"ДО СУХУРА";
                case AppLanguage::Spanish:    return L"HASTA EL SUHUR";
                default: break;
            }
        }
    }
    std::wstring name = GetEffectivePrayerName(kind, settings);
    std::wstring up = name;
    if (!IsRtlLanguage(eff) && eff != AppLanguage::Bengali) {
        CharUpperW(up.data());
    }
    switch (eff) {
        case AppLanguage::Turkish:    return up + L" VAKTİNE";
        case AppLanguage::English:    return L"UNTIL " + up;
        case AppLanguage::Arabic:     return L"الوقت المتبقي · " + name;
        case AppLanguage::German:     return L"BIS " + up;
        case AppLanguage::French:     return L"AVANT " + up;
        case AppLanguage::Indonesian: return L"MENUJU " + up;
        case AppLanguage::Urdu:       return name + L" تک باقی وقت";
        case AppLanguage::Bengali:    return name + L" পর্যন্ত বাকি";
        case AppLanguage::Malay:      return L"MENUJU " + up;
        case AppLanguage::Persian:    return L"زمان باقی‌مانده تا " + name;
        case AppLanguage::Russian:    return L"ДО " + up;
        case AppLanguage::Spanish:    return L"HASTA " + up;
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
    static constexpr const wchar_t* kUr[12] = {L"جنوری", L"فروری", L"مارچ", L"اپریل", L"مئی", L"جون", L"جولائی", L"اگست", L"ستمبر", L"اکتوبر", L"نومبر", L"دسمبر"};
    static constexpr const wchar_t* kBn[12] = {L"জানু", L"ফেব", L"মার্চ", L"এপ্রি", L"মে", L"জুন", L"জুলা", L"আগ", L"সেপ্টে", L"অক্টো", L"নভে", L"ডিসে"};
    static constexpr const wchar_t* kMs[12] = {L"Jan", L"Feb", L"Mac", L"Apr", L"Mei", L"Jun", L"Jul", L"Ogo", L"Sep", L"Okt", L"Nov", L"Dis"};
    static constexpr const wchar_t* kFa[12] = {L"ژانویه", L"فوریه", L"مارس", L"آوریل", L"مه", L"ژوئن", L"ژوئیه", L"اوت", L"سپتامبر", L"اکتبر", L"نوامبر", L"دسامبر"};
    static constexpr const wchar_t* kRu[12] = {L"Янв", L"Фев", L"Мар", L"Апр", L"Май", L"Июн", L"Июл", L"Авг", L"Сен", L"Окт", L"Ноя", L"Дек"};
    static constexpr const wchar_t* kEs[12] = {L"Ene", L"Feb", L"Mar", L"Abr", L"May", L"Jun", L"Jul", L"Ago", L"Sep", L"Oct", L"Nov", L"Dic"};
    switch (LangIndex(lang)) {
        case 0: return kTr[m];
        case 1: return kEn[m];
        case 2: return kAr[m];
        case 3: return kDe[m];
        case 4: return kFr[m];
        case 5: return kId[m];
        case 6: return kUr[m];
        case 7: return kBn[m];
        case 8: return kMs[m];
        case 9: return kFa[m];
        case 10: return kRu[m];
        case 11: return kEs[m];
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
    static constexpr const wchar_t* kUr[12] = {L"جنوری", L"فروری", L"مارچ", L"اپریل", L"مئی", L"جون", L"جولائی", L"اگست", L"ستمبر", L"اکتوبر", L"نومبر", L"دسمبر"};
    static constexpr const wchar_t* kBn[12] = {L"জানুয়ারি", L"ফেব্রুয়ারি", L"মার্চ", L"এপ্রিল", L"মে", L"জুন", L"জুলাই", L"আগস্ট", L"সেপ্টেম্বর", L"অক্টোবর", L"নভেম্বর", L"ডিসেম্বর"};
    static constexpr const wchar_t* kMs[12] = {L"Januari", L"Februari", L"Mac", L"April", L"Mei", L"Jun", L"Julai", L"Ogos", L"September", L"Oktober", L"November", L"Disember"};
    static constexpr const wchar_t* kFa[12] = {L"ژانویه", L"فوریه", L"مارس", L"آوریل", L"مه", L"ژوئن", L"ژوئیه", L"اوت", L"سپتامبر", L"اکتبر", L"نوامبر", L"دسامبر"};
    static constexpr const wchar_t* kRu[12] = {L"Январь", L"Февраль", L"Март", L"Апрель", L"Май", L"Июнь", L"Июль", L"Август", L"Сентябрь", L"Октябрь", L"Ноябрь", L"Декабрь"};
    static constexpr const wchar_t* kEs[12] = {L"Enero", L"Febrero", L"Marzo", L"Abril", L"Mayo", L"Junio", L"Julio", L"Agosto", L"Septiembre", L"Octubre", L"Noviembre", L"Diciembre"};
    switch (LangIndex(lang)) {
        case 0: return kTr[m];
        case 1: return kEn[m];
        case 2: return kAr[m];
        case 3: return kDe[m];
        case 4: return kFr[m];
        case 5: return kId[m];
        case 6: return kUr[m];
        case 7: return kBn[m];
        case 8: return kMs[m];
        case 9: return kFa[m];
        case 10: return kRu[m];
        case 11: return kEs[m];
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
    static constexpr const wchar_t* kUr[7] = {L"اتوار", L"پیر", L"منگل", L"بدھ", L"جمعرات", L"جمعہ", L"ہفتہ"};
    static constexpr const wchar_t* kBn[7] = {L"রবি", L"সোম", L"মঙ্গল", L"বুধ", L"বৃহঃ", L"শুক্র", L"শনি"};
    static constexpr const wchar_t* kMs[7] = {L"Ahd", L"Isn", L"Sel", L"Rab", L"Kha", L"Jum", L"Sab"};
    static constexpr const wchar_t* kFa[7] = {L"یکشنبه", L"دوشنبه", L"سه‌شنبه", L"چهارشنبه", L"پنجشنبه", L"جمعه", L"شنبه"};
    static constexpr const wchar_t* kRu[7] = {L"Вс", L"Пн", L"Вт", L"Ср", L"Чт", L"Пт", L"Сб"};
    static constexpr const wchar_t* kEs[7] = {L"Dom", L"Lun", L"Mar", L"Mié", L"Jue", L"Vie", L"Sáb"};
    switch (LangIndex(lang)) {
        case 0: return kTr[d];
        case 1: return kEn[d];
        case 2: return kAr[d];
        case 3: return kDe[d];
        case 4: return kFr[d];
        case 5: return kId[d];
        case 6: return kUr[d];
        case 7: return kBn[d];
        case 8: return kMs[d];
        case 9: return kFa[d];
        case 10: return kRu[d];
        case 11: return kEs[d];
    }
    return kTr[d];
}

const wchar_t* GetHijriMonthNameLocalized(int month1To12, AppLanguage lang) {
    int m = std::clamp(month1To12, 1, 12) - 1;
    static constexpr const wchar_t* kTr[12] = {L"Muharrem", L"Safer", L"Rebiülevvel", L"Rebiülahir", L"Cemaziyelevvel", L"Cemaziyelahir", L"Recep", L"Şaban", L"Ramazan", L"Şevval", L"Zilkade", L"Zilhicce"};
    static constexpr const wchar_t* kEn[12] = {L"Muharram", L"Safar", L"Rabi I", L"Rabi II", L"Jumada I", L"Jumada II", L"Rajab", L"Sha'ban", L"Ramadan", L"Shawwal", L"Dhu al-Qi'dah", L"Dhu al-Hijjah"};
    static constexpr const wchar_t* kAr[12] = {L"محرم", L"صفر", L"ربيع الأول", L"ربيع الآخر", L"جمادى الأولى", L"جمادى الآخرة", L"رجب", L"شعبان", L"رمضان", L"شوال", L"ذو القعدة", L"ذو الحجة"};
    static constexpr const wchar_t* kId[12] = {L"Muharram", L"Safar", L"Rabiul Awal", L"Rabiul Akhir", L"Jumadil Awal", L"Jumadil Akhir", L"Rajab", L"Sya'ban", L"Ramadhan", L"Syawal", L"Dzulkaidah", L"Dzulhijjah"};
    static constexpr const wchar_t* kUr[12] = {L"محرم", L"صفر", L"ربیع الاول", L"ربیع الثانی", L"جمادی الاول", L"جمادی الثانی", L"رجب", L"شعبان", L"رمضان", L"شوال", L"ذوالقعدہ", L"ذوالحجہ"};
    static constexpr const wchar_t* kBn[12] = {L"মহররম", L"সফর", L"রবিউল আউয়াল", L"রবিউস সানি", L"জমাদিউল আউয়াল", L"জমাদিউস সানি", L"রজব", L"শাবান", L"রমজান", L"শাওয়াল", L"জিলকদ", L"জিলহজ"};
    static constexpr const wchar_t* kMs[12] = {L"Muharam", L"Safar", L"Rabiulawal", L"Rabiulakhir", L"Jamadilawal", L"Jamadilakhir", L"Rejab", L"Syaaban", L"Ramadan", L"Syawal", L"Zulkaedah", L"Zulhijah"};
    static constexpr const wchar_t* kFa[12] = {L"محرم", L"صفر", L"ربیع‌الاول", L"ربیع‌الثانی", L"جمادی‌الاول", L"جمادی‌الثانی", L"رجب", L"شعبان", L"رمضان", L"شوال", L"ذیقعده", L"ذیحجه"};
    static constexpr const wchar_t* kRu[12] = {L"Мухаррам", L"Сафар", L"Раби аль-авваль", L"Раби ас-сани", L"Джумада аль-уля", L"Джумада ас-сани", L"Раджаб", L"Шаабан", L"Рамадан", L"Шавваль", L"Зуль-када", L"Зуль-хиджа"};
    static constexpr const wchar_t* kEs[12] = {L"Muharram", L"Safar", L"Rabi al-Awwal", L"Rabi al-Thani", L"Yumada al-Ula", L"Yumada al-Thani", L"Rayab", L"Shaabán", L"Ramadán", L"Shawwal", L"Dhul Qa'da", L"Dhul Hiyya"};
    size_t li = LangIndex(lang);
    if (li == 0) return kTr[m];
    if (li == 2) return kAr[m];
    if (li == 5) return kId[m];
    if (li == 6) return kUr[m];
    if (li == 7) return kBn[m];
    if (li == 8) return kMs[m];
    if (li == 9) return kFa[m];
    if (li == 10) return kRu[m];
    if (li == 11) return kEs[m];
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
    static constexpr const wchar_t* kUr[8] = {L"شمال", L"شمال مشرق", L"مشرق", L"جنوب مشرق", L"جنوب", L"جنوب مغرب", L"مغرب", L"شمال مغرب"};
    static constexpr const wchar_t* kBn[8] = {L"উ", L"উপূ", L"পূ", L"দপূ", L"দ", L"দপ", L"প", L"উপ"};
    static constexpr const wchar_t* kFa[8] = {L"شمال", L"شمال‌شرق", L"شرق", L"جنوب‌شرق", L"جنوب", L"جنوب‌غرب", L"غرب", L"شمال‌غرب"};
    static constexpr const wchar_t* kRu[8] = {L"С", L"СВ", L"В", L"ЮВ", L"Ю", L"ЮЗ", L"З", L"СЗ"};
    static constexpr const wchar_t* kEs[8] = {L"N", L"NE", L"E", L"SE", L"S", L"SO", L"O", L"NO"};
    switch (LangIndex(lang)) {
        case 0: return kTr[idx];
        case 2: return kAr[idx];
        case 3: return kDe[idx];
        case 5: return kId[idx];
        case 6: return kUr[idx];
        case 7: return kBn[idx];
        case 8: return kId[idx];  // Malay uses the same U/T/S/B letters
        case 9: return kFa[idx];
        case 10: return kRu[idx];
        case 11: return kEs[idx];
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
        case 6: prefix = L"قبلہ"; break;
        case 7: prefix = L"কিবলা"; break;
        case 8: prefix = L"Kiblat"; break;
        case 9: prefix = L"قبله"; break;
        case 10: prefix = L"Кибла"; break;
        case 11: prefix = L"Qibla"; break;
    }
    wchar_t buf[64];
    swprintf_s(buf, L"%ls %d° %ls", prefix, rounded, dir);
    return std::wstring(buf);
}

const wchar_t* GetStyleFriendlyNameLocalized(TaskbarDisplayStyle style, AppLanguage lang) {
    static constexpr LangRow kNames[9] = {
        {L"İki Satır Klasik (İkindiye kalan / 23 dk)", L"Two-Line Classic (Until Asr / 23 min)", L"سطرين كلاسيكي", L"Zweizeilig Klassisch", L"Deux lignes classique", L"Dua Baris Klasik", L"دو لائن کلاسک", L"দুই লাইন ক্লাসিক", L"Dua Baris Klasik", L"دوخطی کلاسیک", L"Две строки, классика", L"Dos líneas clásico"},
        {L"Kompakt Ok (İ→23Dk)", L"Compact Arrow (A→23m)", L"سهم مضغوط", L"Kompakt Pfeil", L"Flèche compacte", L"Panah Ringkas", L"مختصر تیر", L"সংক্ষিপ্ত তীর", L"Anak Panah Ringkas", L"پیکان فشرده", L"Компактная стрелка", L"Flecha compacta"},
        {L"Aralıklı Ok (İ → 23 dk)", L"Spaced Arrow (A → 23 min)", L"سهم متباعد", L"Pfeil mit Abstand", L"Flèche espacée", L"Panah Berjarak", L"فاصلے والا تیر", L"ফাঁকা তীর", L"Anak Panah Berjarak", L"پیکان با فاصله", L"Стрелка с отступами", L"Flecha espaciada"},
        {L"Saatli İki Satır (İkindi · 15:25 / 23 dk)", L"Two-Line + Time (Asr · 15:25 / 23 min)", L"سطرين مع الوقت", L"Zweizeilig + Zeit", L"Deux lignes + Heure", L"Dua Baris + Jam", L"دو لائن + وقت", L"দুই লাইন + সময়", L"Dua Baris + Waktu", L"دوخطی + ساعت", L"Две строки + время", L"Dos líneas + hora"},
        {L"İkon + Sayaç (◐ 23 dk)", L"Icon + Countdown", L"أيقونة + عداد", L"Symbol + Zähler", L"Icône + Compte", L"Ikon + Hitung Mundur", L"آئیکن + الٹی گنتی", L"আইকন + কাউন্টডাউন", L"Ikon + Kiraan Detik", L"آیکون + شمارش", L"Значок + отсчёт", L"Icono + cuenta atrás"},
        {L"Kısa İki Satır (İkindi / 23dk)", L"Short Two-Line", L"سطرين قصير", L"Kurz Zweizeilig", L"Deux lignes court", L"Dua Baris Singkat", L"مختصر دو لائن", L"ছোট দুই লাইন", L"Dua Baris Pendek", L"دوخطی کوتاه", L"Короткие две строки", L"Dos líneas corto"},
        {L"Minimal Nokta Sayaç (● 00:23)", L"Minimal Dot Timer (● 00:23)", L"مؤقت بنقطة", L"Minimal Punkt-Timer", L"Point minimal", L"Timer Titik Minimal", L"سادہ نقطہ ٹائمر (● 00:23)", L"ন্যূনতম ডট টাইমার (● 00:23)", L"Pemasa Titik Minimal (● 00:23)", L"تایمر نقطه‌ای ساده (● 00:23)", L"Минимальный таймер (● 00:23)", L"Temporizador mínimo (● 00:23)"},
        {L"Vakit Saati (İkindi / 15:25)", L"Prayer + Time (Asr / 15:25)", L"الاسم + الوقت", L"Gebet + Zeit", L"Prière + Heure", L"Sholat + Jam", L"نماز + وقت", L"নামাজ + সময়", L"Solat + Waktu", L"نماز + ساعت", L"Намаз + время", L"Oración + hora"},
        {L"Özel Şablon", L"Custom Template", L"قالب مخصص", L"Eigene Vorlage", L"Modèle personnalisé", L"Templat Kustom", L"حسب ضرورت ٹیمپلیٹ", L"কাস্টম টেমপ্লেট", L"Templat Tersuai", L"قالب سفارشی", L"Свой шаблон", L"Plantilla personalizada"}
    };
    int idx = std::clamp(static_cast<int>(style), 0, 8);
    return kNames[idx].At(LangIndex(lang));
}

const wchar_t* GetFlyoutDesignFriendlyName(FlyoutDesignStyle design, AppLanguage lang) {
    static constexpr LangRow kNames[4] = {
        {L"Kompakt Fluent (Windows 11 Klasik)", L"Compact Fluent (Win11 Classic)", L"فلوينت المدمج (ويندوز 11)", L"Kompakt Fluent (Win11)", L"Fluent Compact (Win11)", L"Fluent Ringkas (Win11)", L"کمپیکٹ فلوئنٹ (ونڈوز 11)", L"কমপ্যাক্ট ফ্লুয়েন্ট (উইন১১)", L"Fluent Padat (Win11)", L"فلوئنت فشرده (ویندوز ۱۱)", L"Компактный Fluent (Win11)", L"Fluent compacto (Win11)"},
        {L"Minimal İnce Liste (Cam)", L"Minimal Sleek List", L"قائمة بسيطة فائقة النعومة", L"Minimalistisch Schlicht", L"Liste Épurée Minimal", L"Minimalis Elegan", L"سادہ پتلی فہرست", L"ন্যূনতম পাতলা তালিকা", L"Senarai Minimal Ramping", L"فهرست ساده و باریک", L"Минималистичный список", L"Lista minimalista"},
        {L"Modern 2×3 Kart Izgarası (Bento)", L"Modern 2×3 Bento Cards", L"بطاقات شبكية حديثة 2×3", L"2×3 Bento-Karten", L"Grille de cartes 2×3", L"Kartu Bento 2×3", L"جدید 2×3 بینٹو کارڈز", L"আধুনিক ২×৩ বেন্টো কার্ড", L"Kad Bento 2×3 Moden", L"کارت‌های بنتو ۲×۳", L"Карточки Bento 2×3", L"Tarjetas Bento 2×3"},
        {L"Zaman Çizelgesi + Kıble Pusulası", L"Timeline + Qibla Compass", L"الخط الزمني + بوصلة القبلة", L"Zeitstrahl + Qibla-Kompass", L"Chronologie + Boussole Qibla", L"Linimasa + Kompas Kiblat", L"ٹائم لائن + قبلہ کمپاس", L"টাইমলাইন + কিবলা কম্পাস", L"Garis Masa + Kompas Kiblat", L"خط زمانی + قطب‌نمای قبله", L"Хронология + компас киблы", L"Línea de tiempo + brújula de Qibla"}
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
        case 6: return name + L" کا وقت ہو گیا";
        case 7: return name + L"-এর সময় হয়েছে";
        case 8: return L"Waktu " + name + L" Telah Masuk";
        case 9: return L"وقت نماز " + name;
        case 10: return L"Время намаза: " + name;
        case 11: return L"Hora de " + name;
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
        case 6: return cityName + L" میں " + name + L" کا وقت (" + std::wstring(hhmm) + L")۔";
        case 7: return cityName + L"-এ " + name + L"-এর সময় (" + std::wstring(hhmm) + L")।";
        case 8: return L"Waktu " + name + L" (" + std::wstring(hhmm) + L") bagi " + cityName + L".";
        case 9: return L"وقت " + name + L" (" + std::wstring(hhmm) + L") در " + cityName + L".";
        case 10: return name + L" (" + std::wstring(hhmm) + L") — " + cityName + L".";
        case 11: return L"Es la hora de " + name + L" (" + std::wstring(hhmm) + L") en " + cityName + L".";
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
        case 6:
            swprintf_s(buf, L"%ls میں %d منٹ باقی", name.c_str(), minutes);
            break;
        case 7:
            swprintf_s(buf, L"%ls-এর %d মিনিট বাকি", name.c_str(), minutes);
            break;
        case 8:
            swprintf_s(buf, L"%d minit menuju %ls", minutes, name.c_str());
            break;
        case 9:
            swprintf_s(buf, L"%d دقیقه تا %ls", minutes, name.c_str());
            break;
        case 10:
            swprintf_s(buf, L"%d мин до %ls", minutes, name.c_str());
            break;
        case 11:
            swprintf_s(buf, L"%d minutos para %ls", minutes, name.c_str());
            break;
    }
    return std::wstring(buf);
}

std::wstring FormatNotificationPreBody(PrayerKind kind, std::wstring_view hhmm, const std::wstring& cityName, const AppSettings& settings) {
    std::wstring name = GetEffectivePrayerName(kind, settings);
    return name + L": " + std::wstring(hhmm) + L" · " + cityName;
}

} // namespace vakit
