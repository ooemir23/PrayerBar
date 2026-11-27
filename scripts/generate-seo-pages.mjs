import fs from "node:fs";
import path from "node:path";
import { createRequire } from "node:module";
import { fileURLToPath } from "node:url";

const require = createRequire(import.meta.url);
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const ROOT = path.resolve(__dirname, "..");
const WEBSITE_DIR = path.join(ROOT, "website");
const CATALOG_PATH = path.join(ROOT, "src", "core", "city_catalog.cpp");
const ENGINE = require(path.join(WEBSITE_DIR, "js", "prayer-engine.js"));

const BASE_URL = "https://prayerbar.ooemir.com";
const STORE_URL = "https://apps.microsoft.com/detail/9MSQBZTPGWX6";
const GITHUB_URL = "https://github.com/ooemir23/PrayerBar";

const WIN_TO_IANA_TZ = {
  "Turkey Standard Time": "Europe/Istanbul",
  "GTB Standard Time": "Asia/Nicosia",
  "Azerbaijan Standard Time": "Asia/Baku",
  "Central European Standard Time": "Europe/Sarajevo",
  "Arab Standard Time": "Asia/Riyadh",
  "W. Europe Standard Time": "Europe/Berlin",
  "GMT Standard Time": "Europe/London",
  "Romance Standard Time": "Europe/Paris",
  "Eastern Standard Time": "America/New_York",
  "Central Standard Time": "America/Chicago",
  "Egypt Standard Time": "Africa/Cairo",
  "Arabian Standard Time": "Asia/Dubai",
  "Jordan Standard Time": "Asia/Amman",
  "Iran Standard Time": "Asia/Tehran",
  "Pakistan Standard Time": "Asia/Karachi",
  "Singapore Standard Time": "Asia/Singapore",
  "SE Asia Standard Time": "Asia/Jakarta",
  "W. Central Africa Standard Time": "Africa/Tunis",
  "Morocco Standard Time": "Africa/Casablanca",
  "Russian Standard Time": "Europe/Moscow",
  "Tokyo Standard Time": "Asia/Tokyo",
  "AUS Eastern Standard Time": "Australia/Sydney",
  "Cen. Australia Standard Time": "Australia/Adelaide",
  "E. Australia Standard Time": "Australia/Brisbane",
  "W. Australia Standard Time": "Australia/Perth",
  "AUS Central Standard Time": "Australia/Darwin"
};

const EN_NAME_MAP = {
  istanbul: { name: "Istanbul", country: "Turkey" },
  ankara: { name: "Ankara", country: "Turkey" },
  izmir: { name: "Izmir", country: "Turkey" },
  lefkosa: { name: "Nicosia", country: "Northern Cyprus" },
  baku: { name: "Baku", country: "Azerbaijan" },
  sarajevo: { name: "Sarajevo", country: "Bosnia and Herzegovina" },
  makkah: { name: "Makkah", country: "Saudi Arabia" },
  madinah: { name: "Madinah", country: "Saudi Arabia" },
  riyadh: { name: "Riyadh", country: "Saudi Arabia" },
  berlin: { name: "Berlin", country: "Germany" },
  munich: { name: "Munich", country: "Germany" },
  cologne: { name: "Cologne", country: "Germany" },
  frankfurt: { name: "Frankfurt", country: "Germany" },
  london: { name: "London", country: "United Kingdom" },
  paris: { name: "Paris", country: "France" },
  amsterdam: { name: "Amsterdam", country: "Netherlands" },
  brussels: { name: "Brussels", country: "Belgium" },
  vienna: { name: "Vienna", country: "Austria" },
  newyork: { name: "New York", country: "United States" },
  chicago: { name: "Chicago", country: "United States" },
  toronto: { name: "Toronto", country: "Canada" },
  cairo: { name: "Cairo", country: "Egypt" },
  doha: { name: "Doha", country: "Qatar" },
  dubai: { name: "Dubai", country: "UAE" },
  kuwait: { name: "Kuwait City", country: "Kuwait" },
  amman: { name: "Amman", country: "Jordan" },
  tehran: { name: "Tehran", country: "Iran" },
  karachi: { name: "Karachi", country: "Pakistan" },
  islamabad: { name: "Islamabad", country: "Pakistan" },
  kualalumpur: { name: "Kuala Lumpur", country: "Malaysia" },
  jakarta: { name: "Jakarta", country: "Indonesia" },
  singapore: { name: "Singapore", country: "Singapore" },
  tunis: { name: "Tunis", country: "Tunisia" },
  algiers: { name: "Algiers", country: "Algeria" },
  casablanca: { name: "Casablanca", country: "Morocco" },
  lisbon: { name: "Lisbon", country: "Portugal" },
  moscow: { name: "Moscow", country: "Russia" },
  tokyo: { name: "Tokyo", country: "Japan" },
  sydney: { name: "Sydney", country: "Australia" },
  adelaide: { name: "Adelaide", country: "Australia" },
  melbourne: { name: "Melbourne", country: "Australia" },
  brisbane: { name: "Brisbane", country: "Australia" },
  perth: { name: "Perth", country: "Australia" },
  darwin: { name: "Darwin", country: "Australia" },
  canberra: { name: "Canberra", country: "Australia" }
};

function escapeHtml(str) {
  return String(str)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

function safeJson(obj) {
  return JSON.stringify(obj).replace(/</g, "\\u003c");
}

function parseCityCatalog() {
  const src = fs.readFileSync(CATALOG_PATH, "utf8");
  const lines = src.split(/\r?\n/);
  let section = "province";
  const cities = [];
  const entryRegex = /\{\s*L"([^"]+)"\s*,\s*L"([^"]+)"\s*,\s*L"([^"]+)"\s*,\s*L"([^"]+)"\s*,\s*(-?\d+(?:\.\d+)?)\s*,\s*(-?\d+(?:\.\d+)?)\s*,\s*(?:L"([^"]+)"|(kTrTz))\s*,\s*L"([^"]*)"\s*\}/;

  for (const line of lines) {
    if (line.includes("// Popüler İlçeler")) {
      section = "district";
      continue;
    }
    if (line.includes("// Dünya Şehirleri")) {
      section = "world";
      continue;
    }
    const m = line.match(entryRegex);
    if (!m) continue;
    const winTz = m[8] === "kTrTz" ? "Turkey Standard Time" : m[7];
    const ianaTz = WIN_TO_IANA_TZ[winTz];
    if (!ianaTz) {
      throw new Error(`Unmapped Windows timezone "${winTz}" for city "${m[1]}"`);
    }
    const id = m[1];
    const enMeta = EN_NAME_MAP[id];
    const method = ENGINE.getRecommendedMethodForCountry(m[4]);
    const methodMeta = ENGINE.METHOD_PARAMS[method] || ENGINE.METHOD_PARAMS.Diyanet;

    cities.push({
      id,
      name: m[2],
      country: m[3],
      countryCode: m[4],
      lat: parseFloat(m[5]),
      lon: parseFloat(m[6]),
      winTz,
      ianaTz,
      aliases: m[9],
      group: section,
      nameEn: enMeta ? enMeta.name : m[2],
      countryEn: enMeta ? enMeta.country : m[3],
      method,
      methodLabelTr: methodMeta.labelTr,
      methodLabelEn: methodMeta.labelEn
    });
  }

  if (cities.length < 120) {
    throw new Error(`Parsed only ${cities.length} cities from city_catalog.cpp (expected >= 120).`);
  }
  return cities;
}

function formatDateTr(ymd) {
  const months = ["Ocak", "Şubat", "Mart", "Nisan", "Mayıs", "Haziran", "Temmuz", "Ağustos", "Eylül", "Ekim", "Kasım", "Aralık"];
  return `${ymd.day} ${months[ymd.month - 1]} ${ymd.year}`;
}

function formatDateEn(ymd) {
  const months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
  return `${months[ymd.month - 1]} ${ymd.day}, ${ymd.year}`;
}

function renderTopbar(lang, activeNav) {
  const links = lang === "tr"
    ? [
        { key: "home", href: "/", label: "Ana Sayfa" },
        { key: "namaz", href: "/namaz-vakitleri", label: "Namaz Vakitleri" },
        { key: "imsakiye", href: "/imsakiye", label: "İmsakiye" },
        { key: "program", href: "/ezan-vakti-programi", label: "Ezan Programı" },
        { key: "global", href: "/prayer-times", label: "World Times" }
      ]
    : [
        { key: "home", href: "/", label: "Home" },
        { key: "global", href: "/prayer-times", label: "Prayer Times" },
        { key: "namaz", href: "/namaz-vakitleri", label: "Namaz Vakitleri (TR)" },
        { key: "imsakiye", href: "/imsakiye", label: "Imsakiye" }
      ];

  const linksHtml = links
    .map(l => `<a href="${l.href}"${l.key === activeNav ? ' class="active"' : ""}>${escapeHtml(l.label)}</a>`)
    .join("");

  const storeLabel = lang === "tr" ? "Windows İçin İndir" : "Get Windows App";

  return `<header class="topbar">
    <div class="container nav-inner">
      <a class="brand" href="/">
        <img src="/images/app-icon.png" alt="PrayerBar" />
        <span>PrayerBar</span>
      </a>
      <nav class="nav-links" aria-label="Main">${linksHtml}</nav>
      <div class="nav-actions">
        <a href="${STORE_URL}" target="_blank" rel="noopener" class="btn btn-primary btn-sm">${escapeHtml(storeLabel)}</a>
      </div>
    </div>
  </header>`;
}

function renderFooter(lang) {
  const links = lang === "tr"
    ? `<a href="/">Ana Sayfa</a>
       <a href="/namaz-vakitleri">Namaz Vakitleri</a>
       <a href="/imsakiye">İmsakiye</a>
       <a href="/ezan-vakti-programi">Bilgisayar Ezan Programı</a>
       <a href="/prayer-times">Prayer Times</a>
       <a href="/privacy">Gizlilik Politikası</a>
       <a href="/terms">Kullanım Koşulları</a>
       <a href="${STORE_URL}" target="_blank" rel="noopener">Microsoft Store</a>`
    : `<a href="/">Home</a>
       <a href="/prayer-times">Prayer Times</a>
       <a href="/namaz-vakitleri">Namaz Vakitleri</a>
       <a href="/imsakiye">Imsakiye</a>
       <a href="/privacy">Privacy Policy</a>
       <a href="/terms">Terms</a>
       <a href="${GITHUB_URL}" target="_blank" rel="noopener">GitHub</a>
       <a href="${STORE_URL}" target="_blank" rel="noopener">Microsoft Store</a>`;

  return `<footer>
    <span>© 2026 PrayerBar · OOE CO</span>
    <div class="footer-links">${links}</div>
  </footer>`;
}

function renderDownloadBanner(lang, cityLabel) {
  if (lang === "tr") {
    return `<section class="app-banner" aria-label="PrayerBar Windows uygulaması">
      <div class="app-banner-left">
        <img src="/images/app-icon.png" alt="PrayerBar" />
        <div>
          <h2>${escapeHtml(cityLabel)} namaz vakitlerini Windows görev çubuğundan takip edin</h2>
          <p>İftar, sahur ve tüm vakitler için geri sayım, ezan sesiyle vaktinde uyarı, vakit öncesi hatırlatma ve aylık imsakiye. Reklamsız ve internetsiz de çalışır.</p>
        </div>
      </div>
      <div class="app-banner-actions">
        <a href="${STORE_URL}" target="_blank" rel="noopener" class="btn btn-primary">Microsoft Store'dan İndir</a>
        <a href="/ezan-vakti-programi" class="btn btn-secondary">Programı İncele</a>
      </div>
    </section>`;
  }

  return `<section class="app-banner" aria-label="PrayerBar for Windows">
    <div class="app-banner-left">
      <img src="/images/app-icon.png" alt="PrayerBar" />
      <div>
        <h2>Follow ${escapeHtml(cityLabel)} prayer times right on your Windows taskbar</h2>
        <p>Live countdown to the next Adhan, customizable prayer alerts, Qibla compass, and monthly/Ramadan schedule. Ad-free and works offline.</p>
      </div>
    </div>
    <div class="app-banner-actions">
      <a href="${STORE_URL}" target="_blank" rel="noopener" class="btn btn-primary">Get on Microsoft Store</a>
      <a href="/" class="btn btn-secondary">See Live Preview</a>
    </div>
  </section>`;
}

function renderHtmlShell({ lang, title, description, canonicalUrl, activeNav, jsonLdList, bodyHtml, extraScripts = "" }) {
  const jsonLdScripts = jsonLdList
    .map(obj => `<script type="application/ld+json">${safeJson(obj)}</script>`)
    .join("\n  ");

  return `<!DOCTYPE html>
<html lang="${escapeHtml(lang)}">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>${escapeHtml(title)}</title>
  <meta name="description" content="${escapeHtml(description)}" />
  <link rel="canonical" href="${escapeHtml(canonicalUrl)}" />
  <link rel="icon" type="image/png" href="/favicon.png" />
  <meta property="og:type" content="website" />
  <meta property="og:url" content="${escapeHtml(canonicalUrl)}" />
  <meta property="og:title" content="${escapeHtml(title)}" />
  <meta property="og:description" content="${escapeHtml(description)}" />
  <meta property="og:image" content="${BASE_URL}/images/og-card.png" />
  <meta name="twitter:card" content="summary_large_image" />
  <meta name="twitter:image" content="${BASE_URL}/images/og-card.png" />
  <link rel="stylesheet" href="/css/seo.css" />
  ${jsonLdScripts}
</head>
<body>
  ${renderTopbar(lang, activeNav)}
  <main class="container">
    ${bodyHtml}
    ${renderFooter(lang)}
  </main>
  ${extraScripts}
</body>
</html>
`;
}

function buildSoftwareJsonLd(lang) {
  return {
    "@context": "https://schema.org",
    "@type": "SoftwareApplication",
    "name": "PrayerBar",
    "operatingSystem": "Windows 10, Windows 11",
    "applicationCategory": "LifestyleApplication",
    "url": BASE_URL + "/",
    "downloadUrl": STORE_URL,
    "description": lang === "tr"
      ? "Windows görev çubuğunda canlı namaz vakti geri sayımı, ezan bildirimi, aylık imsakiye ve kıble pusulası."
      : "Windows taskbar prayer times countdown, Adhan notifications, Qibla compass, and monthly Ramadan schedule.",
    "offers": {
      "@type": "Offer",
      "price": "0",
      "priceCurrency": "USD"
    }
  };
}

function buildBreadcrumbJsonLd(items) {
  return {
    "@context": "https://schema.org",
    "@type": "BreadcrumbList",
    "itemListElement": items.map((item, idx) => ({
      "@type": "ListItem",
      "position": idx + 1,
      "name": item.name,
      "item": item.url
    }))
  };
}

function buildFaqJsonLd(faqs) {
  return {
    "@context": "https://schema.org",
    "@type": "FAQPage",
    "mainEntity": faqs.map(f => ({
      "@type": "Question",
      "name": f.q,
      "acceptedAnswer": {
        "@type": "Answer",
        "text": f.a
      }
    }))
  };
}

function generateTrCityPage(city, mode, allTrProvinces, allTrDistricts, nowDate) {
  const isImsakiye = mode === "imsakiye";
  const local = ENGINE.getCityNowParts(city.ianaTz, nowDate);
  const schedule = ENGINE.calculateMonthlySchedule(local.year, local.month, local.day, 30, city.lat, city.lon, city.ianaTz, city.method, "standard");
  const today = schedule[0];
  const tomorrowImsak = schedule[1] ? schedule[1].imsak : today.imsak;
  const nowSec = local.hour * 3600 + local.minute * 60 + local.second;
  const nextInfo = ENGINE.getNextPrayerInfo(today, tomorrowImsak, nowSec);
  const qibla = ENGINE.calculateQibla(city.lat, city.lon);

  const prayerLabels = ["İmsak", "Güneş", "Öğle", "İkindi", "Akşam", "Yatsı"];
  const canonicalUrl = isImsakiye
    ? `${BASE_URL}/imsakiye/${city.id}`
    : `${BASE_URL}/namaz-vakitleri/${city.id}`;

  const title = isImsakiye
    ? `${city.name} İmsakiye — 30 Günlük İftar, Sahur ve Namaz Vakitleri | PrayerBar`
    : `${city.name} Namaz Vakitleri — Bugünkü Ezan Vakti ve İmsakiye | PrayerBar`;

  const description = isImsakiye
    ? `${city.name} için 30 günlük imsakiye, sahur (imsak) ve iftar (akşam) saatleri, öğle, ikindi, yatsı vakitleri ve yazdırılabilir namaz takvimi.`
    : `${city.name} için bugünkü imsak, güneş, öğle, ikindi, akşam ve yatsı namaz vakitleri, canlı ezan geri sayımı, kıble yönü ve aylık imsakiye.`;

  const h1 = isImsakiye
    ? `${city.name} İmsakiye (30 Günlük)`
    : `${city.name} Namaz Vakitleri`;

  const lead = isImsakiye
    ? `${city.name} için günlük sahur (imsak), güneş, öğle, ikindi, iftar (akşam) ve yatsı vakitleri. ${city.methodLabelTr} esas alınarak hesaplanır.`
    : `${city.name} için bugünkü ezan saatleri, sıradaki vakte canlı geri sayım, kıble yönü ve 30 günlük imsakiye takvimi.`;

  const prayerCardsHtml = today.times.map((timeStr, idx) => {
    const isNext = (idx === nextInfo.nextIndex && !nextInfo.isTomorrow) || (idx === 0 && nextInfo.isTomorrow);
    const isActive = idx === nextInfo.activeIndex;
    const tagText = isNext ? "Sıradaki" : (isActive ? "Şu an" : "");
    return `<div class="prayer-card${isNext ? " is-next" : ""}${isActive ? " is-active" : ""}" id="prayer-card-${idx}">
      <div class="prayer-name">
        <span>${prayerLabels[idx]}</span>
        <span class="prayer-tag" id="prayer-tag-${idx}"${tagText ? "" : " hidden"}>${tagText}</span>
      </div>
      <div class="prayer-time" id="prayer-time-${idx}">${timeStr}</div>
    </div>`;
  }).join("");

  const rowsHtml = schedule.map((row, idx) => `<tr${idx === 0 ? ' class="today-row"' : ""}>
    <td>${formatDateTr(row)}</td>
    <td>${row.imsak}</td>
    <td>${row.gunes}</td>
    <td>${row.ogle}</td>
    <td>${row.ikindi}</td>
    <td class="ims-iftar">${row.aksam}</td>
    <td>${row.yatsi}</td>
  </tr>`).join("");

  const remH = String(Math.floor(nextInfo.remainingSeconds / 3600)).padStart(2, "0");
  const remM = String(Math.floor((nextInfo.remainingSeconds % 3600) / 60)).padStart(2, "0");
  const remS = String(nextInfo.remainingSeconds % 60).padStart(2, "0");

  const faqs = isImsakiye
    ? [
        {
          q: `${city.name} için sahur (imsak) ve iftar (akşam) vakitleri nasıl hesaplanıyor?`,
          a: `${city.name} imsakiye tablosu, şehrin enlem ve boylamına göre ${city.methodLabelTr} hesaplama metodu kullanılarak hazırlanır. İmsak sahur vaktinin bitişini, Akşam ise iftar vaktini gösterir.`
        },
        {
          q: `${city.name} imsakiyesini yazdırabilir veya bilgisayarımda takip edebilir miyim?`,
          a: `Evet. Bu sayfadaki "İmsakiyeyi Yazdır" düğmesiyle 30 günlük tabloyu yazdırabilir veya Windows 10/11 için ücretsiz PrayerBar uygulamasını kurarak görev çubuğundan iftar ve sahur geri sayımını canlı takip edebilirsiniz.`
        },
        {
          q: `Bilgisayarda ezan vaktini sesli hatırlatan ücretsiz program var mı?`,
          a: `PrayerBar, Windows görev çubuğunda ${city.name} vakitlerini gösteren, her vakit için ayrı ezan veya uyarı sesi çalan, reklamsız bir masaüstü namaz vakti programıdır.`
        }
      ]
    : [
        {
          q: `${city.name} için bugünkü ezan vakitleri hangi metoda göre veriliyor?`,
          a: `${city.name} vakitleri ${city.methodLabelTr} parametreleri ve şehrin koordinatları (${city.lat.toFixed(2)}, ${city.lon.toFixed(2)}) baz alınarak hesaplanır.`
        },
        {
          q: `${city.name} kıble yönü kaç derecedir?`,
          a: `${city.name} için kıble açısı kuzeyden saat yönünde ${qibla.bearing}° (${qibla.dirTr}) yönündedir.`
        },
        {
          q: `${city.name} namaz vakitlerini Windows bilgisayarımda sürekli görebilir miyim?`,
          a: `Evet. PrayerBar'ı Microsoft Store'dan indirerek Windows 10 ve 11 görev çubuğunun sağ veya orta kısmında ${city.name} için sıradaki ezana kalan süreyi anlık görebilir, vakit öncesi hatırlatma alabilirsiniz.`
        }
      ];

  const faqHtml = faqs.map(f => `<details>
    <summary>${escapeHtml(f.q)}</summary>
    <p>${escapeHtml(f.a)}</p>
  </details>`).join("");

  const switchHref = isImsakiye
    ? `/namaz-vakitleri/${city.id}`
    : (city.group === "province" ? `/imsakiye/${city.id}` : `/imsakiye`);
  const switchLabel = isImsakiye
    ? `${city.name} Günlük Namaz Vakitleri →`
    : (city.group === "province" ? `${city.name} 30 Günlük İmsakiye →` : `81 İl İmsakiye Sayfası →`);

  const relatedList = allTrProvinces
    .filter(c => c.id !== city.id)
    .slice(0, 18)
    .map(c => `<a class="chip-link" href="/${isImsakiye ? "imsakiye" : "namaz-vakitleri"}/${c.id}">${escapeHtml(c.name)}</a>`)
    .join("");

  const districtChips = allTrDistricts
    .filter(c => c.id !== city.id)
    .map(c => `<a class="chip-link" href="/namaz-vakitleri/${c.id}">${escapeHtml(c.name)}</a>`)
    .join("");

  const breadcrumbItems = isImsakiye
    ? [
        { name: "Ana Sayfa", url: `${BASE_URL}/` },
        { name: "İmsakiye", url: `${BASE_URL}/imsakiye` },
        { name: `${city.name} İmsakiye`, url: canonicalUrl }
      ]
    : [
        { name: "Ana Sayfa", url: `${BASE_URL}/` },
        { name: "Namaz Vakitleri", url: `${BASE_URL}/namaz-vakitleri` },
        { name: `${city.name} Namaz Vakitleri`, url: canonicalUrl }
      ];

  const pageConfig = {
    id: city.id,
    name: city.name,
    nameEn: city.nameEn,
    country: city.country,
    countryCode: city.countryCode,
    lat: city.lat,
    lon: city.lon,
    ianaTz: city.ianaTz,
    method: city.method,
    methodLabel: city.methodLabelTr,
    lang: "tr",
    mode
  };

  const bodyHtml = `
    <nav class="breadcrumbs" aria-label="Breadcrumb">
      <a href="/">Ana Sayfa</a> <span>/</span>
      <a href="/${isImsakiye ? "imsakiye" : "namaz-vakitleri"}">${isImsakiye ? "İmsakiye" : "Namaz Vakitleri"}</a> <span>/</span>
      <span>${escapeHtml(city.name)}</span>
    </nav>

    <section class="page-hero">
      <div>
        <h1>${escapeHtml(h1)}</h1>
        <p class="page-lead">${escapeHtml(lead)}</p>
        <div class="page-meta-row">
          <span class="meta-badge" id="liveDateLabel">${escapeHtml(formatDateTr(local))}</span>
          <span class="meta-badge">${escapeHtml(city.methodLabelTr)}</span>
          <span class="meta-badge">Kıble: ${qibla.bearing}° ${escapeHtml(qibla.dirTr)}</span>
          <a class="chip-link" href="${switchHref}">${escapeHtml(switchLabel)}</a>
        </div>
      </div>
      <div class="countdown-pill">
        <div class="countdown-label" id="liveCountdownLabel">${escapeHtml(prayerLabels[nextInfo.nextIndex])} vaktine kalan</div>
        <div class="countdown-value num" id="liveCountdownValue">${remH}:${remM}:${remS}</div>
        <div class="countdown-sub num" id="liveCountdownSub">Vakit saati: ${nextInfo.targetTime}</div>
      </div>
    </section>

    <section aria-label="Bugünkü namaz vakitleri">
      <div class="prayer-grid">${prayerCardsHtml}</div>
    </section>

    ${renderDownloadBanner("tr", city.name)}

    <section class="section" id="imsakiye-tablosu">
      <div class="section-head">
        <div>
          <h2>${escapeHtml(city.name)} 30 Günlük İmsakiye</h2>
          <p>İmsak (sahur), güneş, öğle, ikindi, akşam (iftar) ve yatsı saatleri.</p>
        </div>
        <button type="button" class="btn btn-secondary btn-sm no-print" data-print-imsakiye>İmsakiyeyi Yazdır</button>
      </div>
      <div class="table-card">
        <table class="ims-table">
          <thead>
            <tr>
              <th>Tarih</th>
              <th>İmsak (Sahur)</th>
              <th>Güneş</th>
              <th>Öğle</th>
              <th>İkindi</th>
              <th>Akşam (İftar)</th>
              <th>Yatsı</th>
            </tr>
          </thead>
          <tbody id="imsakiyeBody">${rowsHtml}</tbody>
        </table>
      </div>
    </section>

    <section class="section no-print">
      <div class="section-head">
        <div>
          <h2>Sık Sorulan Sorular</h2>
        </div>
      </div>
      <div class="faq-list">${faqHtml}</div>
    </section>

    <section class="section no-print">
      <div class="section-head">
        <div>
          <h2>Diğer İller ve İlçeler</h2>
          <p><a href="/${isImsakiye ? "imsakiye" : "namaz-vakitleri"}">Tüm şehirleri gör →</a></p>
        </div>
      </div>
      <div class="quick-chips">${relatedList}${isImsakiye ? "" : districtChips}</div>
    </section>

    <script type="application/json" id="page-city-data">${safeJson(pageConfig)}</script>
  `;

  return {
    canonicalUrl,
    html: renderHtmlShell({
      lang: "tr",
      title,
      description,
      canonicalUrl,
      activeNav: isImsakiye ? "imsakiye" : "namaz",
      jsonLdList: [
        buildBreadcrumbJsonLd(breadcrumbItems),
        buildFaqJsonLd(faqs),
        buildSoftwareJsonLd("tr")
      ],
      bodyHtml,
      extraScripts: `<script src="/js/prayer-engine.js"></script>\n  <script src="/js/city-page.js"></script>`
    })
  };
}

function generateEnCityPage(city, allWorldTargets, nowDate) {
  const local = ENGINE.getCityNowParts(city.ianaTz, nowDate);
  const schedule = ENGINE.calculateMonthlySchedule(local.year, local.month, local.day, 30, city.lat, city.lon, city.ianaTz, city.method, "standard");
  const today = schedule[0];
  const tomorrowImsak = schedule[1] ? schedule[1].imsak : today.imsak;
  const nowSec = local.hour * 3600 + local.minute * 60 + local.second;
  const nextInfo = ENGINE.getNextPrayerInfo(today, tomorrowImsak, nowSec);
  const qibla = ENGINE.calculateQibla(city.lat, city.lon);

  const prayerLabels = ["Fajr", "Sunrise", "Dhuhr", "Asr", "Maghrib", "Isha"];
  const canonicalUrl = `${BASE_URL}/prayer-times/${city.id}`;
  const title = `${city.nameEn} Prayer Times Today — Adhan & Monthly Schedule | PrayerBar`;
  const description = `Today's Fajr, Sunrise, Dhuhr, Asr, Maghrib, and Isha prayer times in ${city.nameEn}, ${city.countryEn} with live countdown, Qibla direction, and 30-day timetable.`;

  const prayerCardsHtml = today.times.map((timeStr, idx) => {
    const isNext = (idx === nextInfo.nextIndex && !nextInfo.isTomorrow) || (idx === 0 && nextInfo.isTomorrow);
    const isActive = idx === nextInfo.activeIndex;
    const tagText = isNext ? "Next" : (isActive ? "Now" : "");
    return `<div class="prayer-card${isNext ? " is-next" : ""}${isActive ? " is-active" : ""}" id="prayer-card-${idx}">
      <div class="prayer-name">
        <span>${prayerLabels[idx]}</span>
        <span class="prayer-tag" id="prayer-tag-${idx}"${tagText ? "" : " hidden"}>${tagText}</span>
      </div>
      <div class="prayer-time" id="prayer-time-${idx}">${timeStr}</div>
    </div>`;
  }).join("");

  const rowsHtml = schedule.map((row, idx) => `<tr${idx === 0 ? ' class="today-row"' : ""}>
    <td>${formatDateEn(row)}</td>
    <td>${row.imsak}</td>
    <td>${row.gunes}</td>
    <td>${row.ogle}</td>
    <td>${row.ikindi}</td>
    <td class="ims-iftar">${row.aksam}</td>
    <td>${row.yatsi}</td>
  </tr>`).join("");

  const remH = String(Math.floor(nextInfo.remainingSeconds / 3600)).padStart(2, "0");
  const remM = String(Math.floor((nextInfo.remainingSeconds % 3600) / 60)).padStart(2, "0");
  const remS = String(nextInfo.remainingSeconds % 60).padStart(2, "0");

  const faqs = [
    {
      q: `Which calculation method is used for ${city.nameEn} prayer times?`,
      a: `${city.nameEn} prayer times on this page use ${city.methodLabelEn} based on ${city.nameEn}'s geographic coordinates (${city.lat.toFixed(2)}, ${city.lon.toFixed(2)}).`
    },
    {
      q: `What is the Qibla direction in ${city.nameEn}?`,
      a: `From ${city.nameEn}, the Qibla bearing is ${qibla.bearing}° (${qibla.dirEn}) clockwise from true North.`
    },
    {
      q: `How can I keep ${city.nameEn} Adhan times on my Windows desktop?`,
      a: `Install PrayerBar from the Microsoft Store to see a live countdown for ${city.nameEn} right on your Windows 10 or 11 taskbar, with customizable Adhan alerts and offline support.`
    }
  ];

  const faqHtml = faqs.map(f => `<details>
    <summary>${escapeHtml(f.q)}</summary>
    <p>${escapeHtml(f.a)}</p>
  </details>`).join("");

  const relatedChips = allWorldTargets
    .filter(c => c.id !== city.id)
    .slice(0, 20)
    .map(c => `<a class="chip-link" href="/prayer-times/${c.id}">${escapeHtml(c.nameEn)}</a>`)
    .join("");

  const breadcrumbItems = [
    { name: "Home", url: `${BASE_URL}/` },
    { name: "Prayer Times", url: `${BASE_URL}/prayer-times` },
    { name: `${city.nameEn} Prayer Times`, url: canonicalUrl }
  ];

  const pageConfig = {
    id: city.id,
    name: city.name,
    nameEn: city.nameEn,
    country: city.countryEn,
    countryCode: city.countryCode,
    lat: city.lat,
    lon: city.lon,
    ianaTz: city.ianaTz,
    method: city.method,
    methodLabel: city.methodLabelEn,
    lang: "en",
    mode: "prayer-times"
  };

  const bodyHtml = `
    <nav class="breadcrumbs" aria-label="Breadcrumb">
      <a href="/">Home</a> <span>/</span>
      <a href="/prayer-times">Prayer Times</a> <span>/</span>
      <span>${escapeHtml(city.nameEn)}</span>
    </nav>

    <section class="page-hero">
      <div>
        <h1>${escapeHtml(city.nameEn)} Prayer Times</h1>
        <p class="page-lead">Today's Fajr, Sunrise, Dhuhr, Asr, Maghrib, and Isha times in ${escapeHtml(city.nameEn)}, ${escapeHtml(city.countryEn)}, with live Adhan countdown and 30-day timetable.</p>
        <div class="page-meta-row">
          <span class="meta-badge" id="liveDateLabel">${escapeHtml(formatDateEn(local))}</span>
          <span class="meta-badge">${escapeHtml(city.methodLabelEn)}</span>
          <span class="meta-badge">Qibla: ${qibla.bearing}° ${escapeHtml(qibla.dirEn)}</span>
        </div>
      </div>
      <div class="countdown-pill">
        <div class="countdown-label" id="liveCountdownLabel">Time until ${prayerLabels[nextInfo.nextIndex]}</div>
        <div class="countdown-value num" id="liveCountdownValue">${remH}:${remM}:${remS}</div>
        <div class="countdown-sub num" id="liveCountdownSub">Prayer time: ${nextInfo.targetTime}</div>
      </div>
    </section>

    <section aria-label="Today's prayer times">
      <div class="prayer-grid">${prayerCardsHtml}</div>
    </section>

    ${renderDownloadBanner("en", city.nameEn)}

    <section class="section">
      <div class="section-head">
        <div>
          <h2>${escapeHtml(city.nameEn)} 30-Day Prayer Schedule</h2>
          <p>Fajr, Sunrise, Dhuhr, Asr, Maghrib (Iftar), and Isha timetable.</p>
        </div>
        <button type="button" class="btn btn-secondary btn-sm no-print" data-print-imsakiye>Print Schedule</button>
      </div>
      <div class="table-card">
        <table class="ims-table">
          <thead>
            <tr>
              <th>Date</th>
              <th>Fajr</th>
              <th>Sunrise</th>
              <th>Dhuhr</th>
              <th>Asr</th>
              <th>Maghrib</th>
              <th>Isha</th>
            </tr>
          </thead>
          <tbody id="imsakiyeBody">${rowsHtml}</tbody>
        </table>
      </div>
    </section>

    <section class="section no-print">
      <div class="section-head"><div><h2>Frequently Asked Questions</h2></div></div>
      <div class="faq-list">${faqHtml}</div>
    </section>

    <section class="section no-print">
      <div class="section-head">
        <div>
          <h2>Other World Cities</h2>
          <p><a href="/prayer-times">Browse all cities →</a></p>
        </div>
      </div>
      <div class="quick-chips">${relatedChips}</div>
    </section>

    <script type="application/json" id="page-city-data">${safeJson(pageConfig)}</script>
  `;

  return {
    canonicalUrl,
    html: renderHtmlShell({
      lang: "en",
      title,
      description,
      canonicalUrl,
      activeNav: "global",
      jsonLdList: [
        buildBreadcrumbJsonLd(breadcrumbItems),
        buildFaqJsonLd(faqs),
        buildSoftwareJsonLd("en")
      ],
      bodyHtml,
      extraScripts: `<script src="/js/prayer-engine.js"></script>\n  <script src="/js/city-page.js"></script>`
    })
  };
}

function generateTrHub(mode, provinces, districts, worldCities, nowDate) {
  const isImsakiye = mode === "imsakiye";
  const canonicalUrl = `${BASE_URL}/${isImsakiye ? "imsakiye" : "namaz-vakitleri"}`;
  const title = isImsakiye
    ? "İmsakiye — 81 İl Aylık ve Ramazan İmsakiyesi, İftar ve Sahur Vakitleri | PrayerBar"
    : "Namaz Vakitleri — 81 İl ve Dünya Şehirleri Ezan Vakti | PrayerBar";
  const description = isImsakiye
    ? "Türkiye'deki 81 il için 30 günlük aylık ve Ramazan imsakiyesi. İmsak (sahur), güneş, öğle, ikindi, akşam (iftar) ve yatsı saatleri."
    : "81 il ve dünya şehirleri için bugünkü namaz vakitleri, ezan saatleri, kıble yönü ve 30 günlük imsakiye. Bilgisayarınız için ücretsiz namaz vakti takip aracı.";

  const featuredIds = ["istanbul", "ankara", "izmir", "bursa", "antalya", "adana", "konya", "gaziantep", "sanliurfa", "kocaeli"];
  const featuredRows = featuredIds.map(id => provinces.find(c => c.id === id)).filter(Boolean).map(city => {
    const local = ENGINE.getCityNowParts(city.ianaTz, nowDate);
    const t = ENGINE.calculatePrayerTimes(local.year, local.month, local.day, city.lat, city.lon, city.ianaTz, city.method);
    return `<tr>
      <td><a href="/${isImsakiye ? "imsakiye" : "namaz-vakitleri"}/${city.id}" style="color:var(--accent);font-weight:600;text-decoration:none">${escapeHtml(city.name)}</a></td>
      <td>${t.imsak}</td>
      <td>${t.gunes}</td>
      <td>${t.ogle}</td>
      <td>${t.ikindi}</td>
      <td class="ims-iftar">${t.aksam}</td>
      <td>${t.yatsi}</td>
    </tr>`;
  }).join("");

  const provinceCards = provinces.map(c => {
    const filterKey = `${c.name} ${c.id} ${c.aliases}`;
    return `<a class="city-link-card" href="/${isImsakiye ? "imsakiye" : "namaz-vakitleri"}/${c.id}" data-city-filter="${escapeHtml(filterKey)}">
      <span>${escapeHtml(c.name)}</span>
      <small>${isImsakiye ? "İmsakiye" : "Vakitler"}</small>
    </a>`;
  }).join("");

  const districtCards = !isImsakiye
    ? districts.map(c => `<a class="city-link-card" href="/namaz-vakitleri/${c.id}" data-city-filter="${escapeHtml(c.name + " " + c.aliases)}">
        <span>${escapeHtml(c.name)}</span>
        <small>İlçe</small>
      </a>`).join("")
    : "";

  const worldChips = worldCities.slice(0, 14).map(c => `<a class="chip-link" href="/prayer-times/${c.id}">${escapeHtml(c.name)}</a>`).join("");

  const bodyHtml = `
    <nav class="breadcrumbs" aria-label="Breadcrumb">
      <a href="/">Ana Sayfa</a> <span>/</span>
      <span>${isImsakiye ? "İmsakiye" : "Namaz Vakitleri"}</span>
    </nav>

    <section class="page-hero">
      <div>
        <h1>${isImsakiye ? "81 İl İmsakiye — İftar ve Sahur Vakitleri" : "Namaz Vakitleri — 81 İl Ezan Saatleri"}</h1>
        <p class="page-lead">${isImsakiye
          ? "Şehrinizi seçerek 30 günlük imsak (sahur), güneş, öğle, ikindi, akşam (iftar) ve yatsı saatlerini görüntüleyin veya yazdırın."
          : "Türkiye'deki 81 il, popüler ilçeler ve dünya şehirleri için bugünkü namaz vakitleri, kıble yönü ve aylık imsakiye."}</p>
      </div>
      <a href="/${isImsakiye ? "namaz-vakitleri" : "imsakiye"}" class="btn btn-secondary">${isImsakiye ? "Günlük Namaz Vakitleri →" : "81 İl İmsakiye →"}</a>
    </section>

    <section class="search-panel">
      <div class="search-row">
        <input type="search" id="citySearchInput" class="search-input" placeholder="Şehir veya ilçe ara (ör. İstanbul, Ankara, Kadıköy, Konya)..." aria-label="Şehir ara" />
      </div>
      <div class="quick-chips">
        <a class="chip-link" href="/${isImsakiye ? "imsakiye" : "namaz-vakitleri"}/istanbul">İstanbul</a>
        <a class="chip-link" href="/${isImsakiye ? "imsakiye" : "namaz-vakitleri"}/ankara">Ankara</a>
        <a class="chip-link" href="/${isImsakiye ? "imsakiye" : "namaz-vakitleri"}/izmir">İzmir</a>
        <a class="chip-link" href="/${isImsakiye ? "imsakiye" : "namaz-vakitleri"}/bursa">Bursa</a>
        <a class="chip-link" href="/${isImsakiye ? "imsakiye" : "namaz-vakitleri"}/antalya">Antalya</a>
        <a class="chip-link" href="/${isImsakiye ? "imsakiye" : "namaz-vakitleri"}/konya">Konya</a>
      </div>
    </section>

    <section aria-label="Büyükşehirler bugünkü vakitler" style="margin-bottom:28px">
      <div class="table-card">
        <table class="ims-table">
          <thead>
            <tr>
              <th>Şehir</th>
              <th>İmsak</th>
              <th>Güneş</th>
              <th>Öğle</th>
              <th>İkindi</th>
              <th>Akşam (İftar)</th>
              <th>Yatsı</th>
            </tr>
          </thead>
          <tbody>${featuredRows}</tbody>
        </table>
      </div>
    </section>

    ${renderDownloadBanner("tr", "Türkiye")}

    <section class="section">
      <div class="section-head">
        <div>
          <h2>Tüm Şehirler (81 İl${isImsakiye ? "" : " ve İlçeler"})</h2>
          <p>Detaylı vakit tablosu ve 30 günlük imsakiye için şehrinizi seçin.</p>
        </div>
      </div>
      <div class="city-links-grid">${provinceCards}${districtCards}</div>
      <p id="citySearchEmpty" hidden style="color:var(--ink-muted);margin-top:14px">Aramanızla eşleşen şehir bulunamadı.</p>
    </section>

    <section class="section">
      <div class="section-head">
        <div>
          <h2>Dünya Şehirleri</h2>
          <p><a href="/prayer-times">Tüm dünya şehirlerini gör →</a></p>
        </div>
      </div>
      <div class="quick-chips">${worldChips}</div>
    </section>
  `;

  return {
    canonicalUrl,
    html: renderHtmlShell({
      lang: "tr",
      title,
      description,
      canonicalUrl,
      activeNav: isImsakiye ? "imsakiye" : "namaz",
      jsonLdList: [
        buildBreadcrumbJsonLd([
          { name: "Ana Sayfa", url: `${BASE_URL}/` },
          { name: isImsakiye ? "İmsakiye" : "Namaz Vakitleri", url: canonicalUrl }
        ]),
        buildSoftwareJsonLd("tr")
      ],
      bodyHtml,
      extraScripts: `<script src="/js/city-page.js"></script>`
    })
  };
}

function generateEnHub(worldTargets, nowDate) {
  const canonicalUrl = `${BASE_URL}/prayer-times`;
  const title = "Prayer Times & Adhan Schedule — World Cities & Windows App | PrayerBar";
  const description = "Accurate Fajr, Sunrise, Dhuhr, Asr, Maghrib, and Isha prayer times for major cities worldwide, plus a quiet Windows taskbar Adhan countdown app.";

  const featuredIds = ["london", "berlin", "paris", "newyork", "makkah", "dubai", "jakarta", "kualalumpur", "cairo", "istanbul"];
  const featuredRows = featuredIds.map(id => worldTargets.find(c => c.id === id)).filter(Boolean).map(city => {
    const local = ENGINE.getCityNowParts(city.ianaTz, nowDate);
    const t = ENGINE.calculatePrayerTimes(local.year, local.month, local.day, city.lat, city.lon, city.ianaTz, city.method);
    return `<tr>
      <td><a href="/prayer-times/${city.id}" style="color:var(--accent);font-weight:600;text-decoration:none">${escapeHtml(city.nameEn)}, ${escapeHtml(city.countryEn)}</a></td>
      <td>${t.imsak}</td>
      <td>${t.gunes}</td>
      <td>${t.ogle}</td>
      <td>${t.ikindi}</td>
      <td class="ims-iftar">${t.aksam}</td>
      <td>${t.yatsi}</td>
    </tr>`;
  }).join("");

  const cards = worldTargets.map(c => {
    const filterKey = `${c.nameEn} ${c.name} ${c.countryEn} ${c.aliases}`;
    return `<a class="city-link-card" href="/prayer-times/${c.id}" data-city-filter="${escapeHtml(filterKey)}">
      <span>${escapeHtml(c.nameEn)}</span>
      <small>${escapeHtml(c.countryEn)}</small>
    </a>`;
  }).join("");

  const bodyHtml = `
    <nav class="breadcrumbs" aria-label="Breadcrumb">
      <a href="/">Home</a> <span>/</span>
      <span>Prayer Times</span>
    </nav>

    <section class="page-hero">
      <div>
        <h1>World Prayer Times &amp; Adhan Schedule</h1>
        <p class="page-lead">Select your city for today's Fajr, Dhuhr, Asr, Maghrib, and Isha prayer times, Qibla direction, and 30-day timetable.</p>
      </div>
      <a href="/namaz-vakitleri" class="btn btn-secondary">Türkçe: 81 İl Namaz Vakitleri →</a>
    </section>

    <section class="search-panel">
      <div class="search-row">
        <input type="search" id="citySearchInput" class="search-input" placeholder="Search city or country (e.g. London, Berlin, Jakarta, New York)..." aria-label="Search city" />
      </div>
      <div class="quick-chips">
        <a class="chip-link" href="/prayer-times/london">London</a>
        <a class="chip-link" href="/prayer-times/berlin">Berlin</a>
        <a class="chip-link" href="/prayer-times/paris">Paris</a>
        <a class="chip-link" href="/prayer-times/newyork">New York</a>
        <a class="chip-link" href="/prayer-times/makkah">Makkah</a>
        <a class="chip-link" href="/prayer-times/jakarta">Jakarta</a>
      </div>
    </section>

    <section style="margin-bottom:28px">
      <div class="table-card">
        <table class="ims-table">
          <thead>
            <tr>
              <th>City</th>
              <th>Fajr</th>
              <th>Sunrise</th>
              <th>Dhuhr</th>
              <th>Asr</th>
              <th>Maghrib</th>
              <th>Isha</th>
            </tr>
          </thead>
          <tbody>${featuredRows}</tbody>
        </table>
      </div>
    </section>

    ${renderDownloadBanner("en", "your city")}

    <section class="section">
      <div class="section-head">
        <div>
          <h2>All World Cities</h2>
          <p>Click any city for a live Adhan countdown and 30-day schedule.</p>
        </div>
      </div>
      <div class="city-links-grid">${cards}</div>
      <p id="citySearchEmpty" hidden style="color:var(--ink-muted);margin-top:14px">No matching cities found.</p>
    </section>
  `;

  return {
    canonicalUrl,
    html: renderHtmlShell({
      lang: "en",
      title,
      description,
      canonicalUrl,
      activeNav: "global",
      jsonLdList: [
        buildBreadcrumbJsonLd([
          { name: "Home", url: `${BASE_URL}/` },
          { name: "Prayer Times", url: canonicalUrl }
        ]),
        buildSoftwareJsonLd("en")
      ],
      bodyHtml,
      extraScripts: `<script src="/js/city-page.js"></script>`
    })
  };
}

function generateTrLandingPage() {
  const canonicalUrl = `${BASE_URL}/ezan-vakti-programi`;
  const title = "Bilgisayar İçin Ezan Vakti ve Namaz Vakitleri Programı | PrayerBar";
  const description = "Windows 10 ve 11 görev çubuğunda çalışan ücretsiz, reklamsız ve Diyanet uyumlu bilgisayar ezan vakti programı. İftar/sahur geri sayımı, ezan sesiyle bildirim ve aylık imsakiye.";

  const faqs = [
    {
      q: "Bilgisayar için en sade ve reklamsız ezan vakti programı hangisidir?",
      a: "PrayerBar, Windows 10 ve 11 görev çubuğuna yerleşen, reklam veya takip kodu içermeyen bir namaz vakti uygulamasıdır. Çekirdek vakit takibi, bildirimler, dahili ezan sesleri ve aylık imsakiye tamamen ücretsizdir."
    },
    {
      q: "Diyanet namaz vakitleriyle uyumlu mu ve internetsiz çalışıyor mu?",
      a: "Evet. Türkiye için Diyanet İşleri Başkanlığı hesaplama metodunu kullanır; internet bağlantısı kesildiğinde dahi bulunduğunuz şehir için vakitleri doğru şekilde hesaplamaya devam eder."
    },
    {
      q: "Vakit girmeden 15–30 dakika önce hatırlatma alabilir miyim?",
      a: "Evet. İmsak, güneş, öğle, ikindi, akşam ve yatsı için ayrı ayrı vakit öncesi (5–60 dakika) hatırlatma ve dilediğiniz ezan veya yumuşak zil sesini seçebilirsiniz."
    },
    {
      q: "Oyun oynarken veya toplantı sırasında ezan sesi rahatsız eder mi?",
      a: "Hayır. Tam ekran / oyun sessiz modu açıkken oyun oynadığınızda, video izlediğinizde veya sunum yaptığınızda ezan sesini otomatik olarak susturur."
    }
  ];

  const faqHtml = faqs.map(f => `<details>
    <summary>${escapeHtml(f.q)}</summary>
    <p>${escapeHtml(f.a)}</p>
  </details>`).join("");

  const bodyHtml = `
    <nav class="breadcrumbs" aria-label="Breadcrumb">
      <a href="/">Ana Sayfa</a> <span>/</span>
      <span>Bilgisayar Ezan Vakti Programı</span>
    </nav>

    <section class="page-hero">
      <div>
        <h1>Bilgisayar İçin Ezan Vakti ve Namaz Vakitleri Programı</h1>
        <p class="page-lead">Windows 10 ve Windows 11 görev çubuğunun sağ veya orta kısmında bir sonraki ezana kalan süreyi her an görün. Reklamsız, sade ve internetsiz de çalışır.</p>
        <div class="page-meta-row" style="margin-top:14px">
          <a href="${STORE_URL}" target="_blank" rel="noopener" class="btn btn-primary">Microsoft Store'dan Ücretsiz İndir</a>
          <a href="/#playground" class="btn btn-secondary">Canlı Önizlemeyi Dene</a>
        </div>
      </div>
    </section>

    <section class="section">
      <div class="section-head">
        <div>
          <h2>Günlük Kullanımda Ne İşe Yarar?</h2>
          <p>Masaüstünüzü kalabalıklaştırmadan namaz vakitlerini ve Ramazan imsakiyesini elinizin altında tutar.</p>
        </div>
      </div>
      <div class="feature-grid">
        <div class="feature-card">
          <h3>Görev Çubuğunda Canlı Geri Sayım</h3>
          <p>Bilgisayarda çalışırken veya ders dinlerken sıradaki vakte kaç dakika kaldığını tek bakışta görün. Tıklayınca günlük tüm vakitler açılır.</p>
        </div>
        <div class="feature-card">
          <h3>Vaktinde ve Öncesinde Ezan Uyarısı</h3>
          <p>Her vakit için ayrı ayrı vaktinde bildirim ve 5–60 dakika önce hatırlatma ayarlayın. Kısa/uzun ezan veya yumuşak zil sesleri dahildir.</p>
        </div>
        <div class="feature-card">
          <h3>Ramazan İftar/Sahur &amp; Aylık İmsakiye</h3>
          <p>Ramazan ayında otomatik İftar ve Sahur geri sayımına geçer. 30 günlük imsakiyeyi görüntüleyebilir, CSV veya yazdırılabilir HTML olarak alabilirsiniz.</p>
        </div>
        <div class="feature-card">
          <h3>Diyanet ve 23 Dünya Metodu</h3>
          <p>Türkiye'nin 81 ili dahil tüm dünyada şehir seçimi ve otomatik konum algılama. İnternet yokken bile hesaplamalar aksamaz.</p>
        </div>
        <div class="feature-card">
          <h3>Canlı Kıble Pusulası</h3>
          <p>Bulunduğunuz şehrin Kıble açısını derece ve yön olarak açılır pencereden anında görün.</p>
        </div>
        <div class="feature-card">
          <h3>Bilgisayarı Yormaz &amp; Tamamen Reklamsız</h3>
          <p>Reklam, abonelik zorunluluğu veya takip kodu yoktur. Oyun oynarken veya sunumdayken sesi otomatik susturur.</p>
        </div>
      </div>
    </section>

    <section class="section">
      <div class="section-head">
        <div>
          <h2>Şehrinizin Vakitlerini Şimdi Görün</h2>
          <p>Programı kurmadan önce web üzerinden de günlük namaz vakitlerine ve 30 günlük imsakiyeye bakabilirsiniz.</p>
        </div>
      </div>
      <div class="quick-chips">
        <a class="chip-link" href="/namaz-vakitleri/istanbul">İstanbul Namaz Vakitleri</a>
        <a class="chip-link" href="/namaz-vakitleri/ankara">Ankara Namaz Vakitleri</a>
        <a class="chip-link" href="/namaz-vakitleri/izmir">İzmir Namaz Vakitleri</a>
        <a class="chip-link" href="/namaz-vakitleri/bursa">Bursa Namaz Vakitleri</a>
        <a class="chip-link" href="/namaz-vakitleri/konya">Konya Namaz Vakitleri</a>
        <a class="chip-link" href="/imsakiye">81 İl İmsakiye</a>
        <a class="chip-link" href="/namaz-vakitleri">Tüm Şehirler →</a>
      </div>
    </section>

    <section class="section">
      <div class="section-head"><div><h2>Sık Sorulan Sorular</h2></div></div>
      <div class="faq-list">${faqHtml}</div>
    </section>
  `;

  return {
    canonicalUrl,
    html: renderHtmlShell({
      lang: "tr",
      title,
      description,
      canonicalUrl,
      activeNav: "program",
      jsonLdList: [
        buildBreadcrumbJsonLd([
          { name: "Ana Sayfa", url: `${BASE_URL}/` },
          { name: "Bilgisayar Ezan Vakti Programı", url: canonicalUrl }
        ]),
        buildFaqJsonLd(faqs),
        buildSoftwareJsonLd("tr")
      ],
      bodyHtml
    })
  };
}

function writeOut(relPath, content) {
  const fullPath = path.join(WEBSITE_DIR, relPath);
  fs.mkdirSync(path.dirname(fullPath), { recursive: true });
  fs.writeFileSync(fullPath, content, "utf8");
}

function main() {
  const cities = parseCityCatalog();
  const trProvinces = cities.filter(c => c.group === "province");
  const trDistricts = cities.filter(c => c.group === "district");
  const trAll = [...trProvinces, ...trDistricts];
  const worldCities = cities.filter(c => c.group === "world");
  const extraWorldIds = ["istanbul", "ankara", "izmir"];
  const worldTargets = [
    ...worldCities,
    ...extraWorldIds.map(id => cities.find(c => c.id === id)).filter(Boolean)
  ];

  const nowDate = new Date();
  const sitemapUrls = [
    `${BASE_URL}/`,
    `${BASE_URL}/ezan-vakti-programi`,
    `${BASE_URL}/namaz-vakitleri`,
    `${BASE_URL}/imsakiye`,
    `${BASE_URL}/prayer-times`
  ];

  let countNamaz = 0;
  let countImsakiye = 0;
  let countPrayerTimes = 0;

  // Hub pages
  writeOut("namaz-vakitleri/index.html", generateTrHub("namaz", trProvinces, trDistricts, worldCities, nowDate).html);
  writeOut("imsakiye/index.html", generateTrHub("imsakiye", trProvinces, trDistricts, worldCities, nowDate).html);
  writeOut("prayer-times/index.html", generateEnHub(worldTargets, nowDate).html);
  writeOut("ezan-vakti-programi/index.html", generateTrLandingPage().html);

  // 91 TR namaz-vakitleri city/district pages
  for (const city of trAll) {
    const res = generateTrCityPage(city, "namaz", trProvinces, trDistricts, nowDate);
    writeOut(`namaz-vakitleri/${city.id}.html`, res.html);
    sitemapUrls.push(res.canonicalUrl);
    countNamaz++;
  }

  // 81 TR imsakiye province pages
  for (const city of trProvinces) {
    const res = generateTrCityPage(city, "imsakiye", trProvinces, trDistricts, nowDate);
    writeOut(`imsakiye/${city.id}.html`, res.html);
    sitemapUrls.push(res.canonicalUrl);
    countImsakiye++;
  }

  // 45 global prayer-times pages
  for (const city of worldTargets) {
    const res = generateEnCityPage(city, worldTargets, nowDate);
    writeOut(`prayer-times/${city.id}.html`, res.html);
    sitemapUrls.push(res.canonicalUrl);
    countPrayerTimes++;
  }

  sitemapUrls.push(`${BASE_URL}/privacy`, `${BASE_URL}/terms`);

  // robots.txt
  writeOut("robots.txt", `User-agent: *\nAllow: /\n\nSitemap: ${BASE_URL}/sitemap.xml\n`);

  // sitemap.xml
  const todayIso = nowDate.toISOString().slice(0, 10);
  const sitemapXml = `<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n` +
    sitemapUrls.map(url => `  <url><loc>${escapeHtml(url)}</loc><lastmod>${todayIso}</lastmod></url>`).join("\n") +
    `\n</urlset>\n`;
  writeOut("sitemap.xml", sitemapXml);

  console.log(JSON.stringify({
    parsedCities: cities.length,
    trProvinces: trProvinces.length,
    trDistricts: trDistricts.length,
    worldCities: worldCities.length,
    generated: {
      namazVakitleriPages: countNamaz,
      imsakiyePages: countImsakiye,
      prayerTimesPages: countPrayerTimes,
      hubAndLandingPages: 4,
      totalSitemapUrls: sitemapUrls.length
    }
  }, null, 2));
}

main();
