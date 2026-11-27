(function (root) {
  "use strict";

  var DEG_TO_RAD = Math.PI / 180.0;
  var RAD_TO_DEG = 180.0 / Math.PI;

  function sinD(d) { return Math.sin(d * DEG_TO_RAD); }
  function cosD(d) { return Math.cos(d * DEG_TO_RAD); }
  function tanD(d) { return Math.tan(d * DEG_TO_RAD); }
  function clamp(v, lo, hi) { return Math.min(hi, Math.max(lo, v)); }
  function asinD(x) { return Math.asin(clamp(x, -1.0, 1.0)) * RAD_TO_DEG; }
  function acosD(x) { return Math.acos(clamp(x, -1.0, 1.0)) * RAD_TO_DEG; }
  function atanD(x) { return Math.atan(x) * RAD_TO_DEG; }
  function atan2D(y, x) { return Math.atan2(y, x) * RAD_TO_DEG; }

  function fixAngle(deg) {
    var r = deg % 360.0;
    return r < 0.0 ? r + 360.0 : r;
  }

  function fixHour(hour) {
    var r = hour % 24.0;
    return r < 0.0 ? r + 24.0 : r;
  }

  function toJulianDate(year, month, day) {
    var y = year;
    var m = month;
    if (m <= 2) {
      y -= 1;
      m += 12;
    }
    var a = Math.floor(y / 100);
    var b = 2 - a + Math.floor(a / 4);
    return Math.floor(365.25 * (y + 4716)) +
           Math.floor(30.6001 * (m + 1)) +
           day + b - 1524.5;
  }

  function sunDeclinationAndEqT(jd) {
    var d = jd - 2451545.0;
    var g = fixAngle(357.529 + 0.98560028 * d);
    var q = fixAngle(280.459 + 0.98564736 * d);
    var l = fixAngle(q + 1.915 * sinD(g) + 0.020 * sinD(2.0 * g));
    var e = 23.439 - 0.00000036 * d;
    var raHours = atan2D(cosD(e) * sinD(l), cosD(l)) / 15.0;
    var eqt = q / 15.0 - fixHour(raHours);
    var decl = asinD(sinD(e) * sinD(l));
    return { decl: decl, eqt: eqt };
  }

  function solarNoonCore(jd, longitude, tzHours) {
    var se = sunDeclinationAndEqT(jd);
    return 12.0 + tzHours - longitude / 15.0 - se.eqt;
  }

  function computeSolarNoon(julianBase, dayFraction, longitude, tzHours) {
    var t = solarNoonCore(julianBase + dayFraction, longitude, tzHours);
    return solarNoonCore(julianBase + t / 24.0, longitude, tzHours);
  }

  function sunAngleCore(jd, latitude, longitude, tzHours, depressionDeg, isMorning) {
    var se = sunDeclinationAndEqT(jd);
    var noon = 12.0 + tzHours - longitude / 15.0 - se.eqt;
    var cosH = (-sinD(depressionDeg) - sinD(latitude) * sinD(se.decl)) / (cosD(latitude) * cosD(se.decl));
    if (cosH < -0.9999 || cosH > 0.9999) {
      return NaN;
    }
    var hourAngle = acosD(cosH) / 15.0;
    return isMorning ? (noon - hourAngle) : (noon + hourAngle);
  }

  function computeSunAngleTime(julianBase, dayFraction, latitude, longitude, tzHours, depressionDeg, isMorning) {
    var t = sunAngleCore(julianBase + dayFraction, latitude, longitude, tzHours, depressionDeg, isMorning);
    if (Number.isNaN(t)) return t;
    return sunAngleCore(julianBase + t / 24.0, latitude, longitude, tzHours, depressionDeg, isMorning);
  }

  function asrCore(jd, latitude, longitude, tzHours, shadowFactor) {
    var se = sunDeclinationAndEqT(jd);
    var noon = 12.0 + tzHours - longitude / 15.0 - se.eqt;
    var altitude = atanD(1.0 / (shadowFactor + tanD(Math.abs(latitude - se.decl))));
    var cosH = (sinD(altitude) - sinD(latitude) * sinD(se.decl)) / (cosD(latitude) * cosD(se.decl));
    if (cosH < -0.9999 || cosH > 0.9999) {
      return noon + 3.0;
    }
    var hourAngle = acosD(cosH) / 15.0;
    return noon + hourAngle;
  }

  function computeAsrTime(julianBase, dayFraction, latitude, longitude, tzHours, shadowFactor) {
    var t = asrCore(julianBase + dayFraction, latitude, longitude, tzHours, shadowFactor);
    if (Number.isNaN(t)) return t;
    return asrCore(julianBase + t / 24.0, latitude, longitude, tzHours, shadowFactor);
  }

  function hoursToTotalMinutes(hours) {
    if (Number.isNaN(hours) || !Number.isFinite(hours)) {
      return 12 * 60;
    }
    var fixedHours = fixHour(hours);
    var totalMinutes = Math.round(fixedHours * 60.0);
    return clamp(totalMinutes, 0, 23 * 60 + 59);
  }

  function formatMinutes(totalMinutes) {
    var h = Math.floor(totalMinutes / 60);
    var m = totalMinutes % 60;
    return (h < 10 ? "0" + h : String(h)) + ":" + (m < 10 ? "0" + m : String(m));
  }

  var METHOD_PARAMS = {
    Jafari:           { fajrAngle: 16.0, ishaAngle: 14.0, ishaMinutesAfterMaghrib: -1, maghribAngle: 4.0,   applyDiyanetTemkin: false, labelTr: "Caferi (Kum)", labelEn: "Shia Ithna-Ashari (Jafari)" },
    Karachi:          { fajrAngle: 18.0, ishaAngle: 18.0, ishaMinutesAfterMaghrib: -1, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "Karaçi İslam Bilimleri", labelEn: "University of Islamic Sciences, Karachi" },
    ISNA:             { fajrAngle: 15.0, ishaAngle: 15.0, ishaMinutesAfterMaghrib: -1, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "ISNA (Kuzey Amerika)", labelEn: "ISNA (North America)" },
    MWL:              { fajrAngle: 18.0, ishaAngle: 17.0, ishaMinutesAfterMaghrib: -1, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "Muslim World League (MWL)", labelEn: "Muslim World League (MWL)" },
    UmmAlQura:        { fajrAngle: 18.5, ishaAngle: 0.0,  ishaMinutesAfterMaghrib: 90, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "Ümmü'l-Kura (Mekke)", labelEn: "Umm al-Qura University, Makkah" },
    Egypt:            { fajrAngle: 19.5, ishaAngle: 17.5, ishaMinutesAfterMaghrib: -1, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "Mısır Genel Kurumu", labelEn: "Egyptian General Authority of Survey" },
    Tehran:           { fajrAngle: 17.7, ishaAngle: 14.0, ishaMinutesAfterMaghrib: -1, maghribAngle: 4.5,   applyDiyanetTemkin: false, labelTr: "Tahran Üniversitesi", labelEn: "Institute of Geophysics, Tehran" },
    Gulf:             { fajrAngle: 19.5, ishaAngle: 0.0,  ishaMinutesAfterMaghrib: 90, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "Körfez Bölgesi", labelEn: "Gulf Region" },
    Kuwait:           { fajrAngle: 18.0, ishaAngle: 17.5, ishaMinutesAfterMaghrib: -1, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "Kuveyt", labelEn: "Kuwait" },
    Qatar:            { fajrAngle: 18.0, ishaAngle: 0.0,  ishaMinutesAfterMaghrib: 90, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "Katar", labelEn: "Qatar" },
    Singapore:        { fajrAngle: 20.0, ishaAngle: 18.0, ishaMinutesAfterMaghrib: -1, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "Singapur (MUIS)", labelEn: "MUIS Singapore" },
    FranceUOIF:       { fajrAngle: 12.0, ishaAngle: 12.0, ishaMinutesAfterMaghrib: -1, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "UOIF (Fransa)", labelEn: "UOIF France" },
    Diyanet:          { fajrAngle: 18.0, ishaAngle: 17.0, ishaMinutesAfterMaghrib: -1, maghribAngle: 0.833, applyDiyanetTemkin: true,  labelTr: "Diyanet İşleri Başkanlığı", labelEn: "Diyanet (Turkey)" },
    Russia:           { fajrAngle: 16.0, ishaAngle: 15.0, ishaMinutesAfterMaghrib: -1, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "Rusya Müslümanları Dini İdaresi", labelEn: "Spiritual Administration of Muslims of Russia" },
    Moonsighting:     { fajrAngle: 18.0, ishaAngle: 18.0, ishaMinutesAfterMaghrib: -1, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "Moonsighting Committee", labelEn: "Moonsighting Committee Worldwide" },
    Dubai:            { fajrAngle: 18.2, ishaAngle: 18.2, ishaMinutesAfterMaghrib: -1, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "Dubai (BAE)", labelEn: "Dubai (UAE)" },
    MalaysiaJAKIM:    { fajrAngle: 20.0, ishaAngle: 18.0, ishaMinutesAfterMaghrib: -1, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "Malezya (JAKIM)", labelEn: "JAKIM Malaysia" },
    Tunisia:          { fajrAngle: 18.0, ishaAngle: 18.0, ishaMinutesAfterMaghrib: -1, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "Tunus", labelEn: "Tunisia" },
    Algeria:          { fajrAngle: 18.0, ishaAngle: 17.0, ishaMinutesAfterMaghrib: -1, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "Cezayir", labelEn: "Algeria" },
    IndonesiaKEMENAG: { fajrAngle: 20.0, ishaAngle: 18.0, ishaMinutesAfterMaghrib: -1, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "Endonezya (KEMENAG)", labelEn: "KEMENAG Indonesia" },
    Morocco:          { fajrAngle: 19.0, ishaAngle: 17.0, ishaMinutesAfterMaghrib: -1, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "Fas", labelEn: "Morocco" },
    Portugal:         { fajrAngle: 18.0, ishaAngle: 17.0, ishaMinutesAfterMaghrib: -1, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "Portekiz (Lizbon)", labelEn: "Islamic Community of Lisbon" },
    Jordan:           { fajrAngle: 18.0, ishaAngle: 18.0, ishaMinutesAfterMaghrib: -1, maghribAngle: 0.833, applyDiyanetTemkin: false, labelTr: "Ürdün Evkaf Bakanlığı", labelEn: "Ministry of Awqaf, Jordan" }
  };

  function getRecommendedMethodForCountry(countryCode) {
    var cc = String(countryCode || "").trim().toLowerCase();
    if (cc === "tr" || cc === "cy") return "Diyanet";
    if (cc === "us" || cc === "ca") return "ISNA";
    if (cc === "sa") return "UmmAlQura";
    if (cc === "eg") return "Egypt";
    if (cc === "pk" || cc === "in" || cc === "bd" || cc === "af") return "Karachi";
    if (cc === "ir") return "Tehran";
    if (cc === "kw") return "Kuwait";
    if (cc === "qa") return "Qatar";
    if (cc === "ae") return "Dubai";
    if (cc === "bh" || cc === "om") return "Gulf";
    if (cc === "sg") return "Singapore";
    if (cc === "fr") return "FranceUOIF";
    if (cc === "ru") return "Russia";
    if (cc === "my" || cc === "bn") return "MalaysiaJAKIM";
    if (cc === "tn") return "Tunisia";
    if (cc === "dz") return "Algeria";
    if (cc === "id") return "IndonesiaKEMENAG";
    if (cc === "ma") return "Morocco";
    if (cc === "pt") return "Portugal";
    if (cc === "jo") return "Jordan";
    return "MWL";
  }

  function extractFormatterParts(dateObj, ianaTz) {
    var dtf = new Intl.DateTimeFormat("en-US", {
      timeZone: ianaTz,
      year: "numeric",
      month: "2-digit",
      day: "2-digit",
      hour: "2-digit",
      minute: "2-digit",
      second: "2-digit",
      hourCycle: "h23"
    });
    var parts = dtf.formatToParts(dateObj);
    var map = {};
    for (var i = 0; i < parts.length; i++) {
      map[parts[i].type] = parts[i].value;
    }
    var h = parseInt(map.hour, 10);
    if (h === 24) h = 0;
    return {
      year: parseInt(map.year, 10),
      month: parseInt(map.month, 10),
      day: parseInt(map.day, 10),
      hour: h,
      minute: parseInt(map.minute, 10),
      second: parseInt(map.second, 10)
    };
  }

  function getTimeZoneOffsetHours(year, month, day, ianaTz) {
    var utcNoonMs = Date.UTC(year, month - 1, day, 12, 0, 0);
    var local = extractFormatterParts(new Date(utcNoonMs), ianaTz);
    var localAsUtcMs = Date.UTC(local.year, local.month - 1, local.day, local.hour, local.minute, local.second);
    return (localAsUtcMs - utcNoonMs) / 3600000.0;
  }

  function getCityNowParts(ianaTz, nowDate) {
    return extractFormatterParts(nowDate || new Date(), ianaTz);
  }

  function calculatePrayerTimes(year, month, day, latitude, longitude, ianaTz, methodKey, asrMethod) {
    var p = METHOD_PARAMS[methodKey] || METHOD_PARAMS.Diyanet;
    var tzHours = getTimeZoneOffsetHours(year, month, day, ianaTz);
    var julianDate = toJulianDate(year, month, day) - longitude / 360.0;
    var asrShadowFactor = (asrMethod === "hanafi") ? 2.0 : 1.0;

    var imsak = computeSunAngleTime(julianDate, 5.0 / 24.0, latitude, longitude, tzHours, p.fajrAngle, true);
    var gunes = computeSunAngleTime(julianDate, 6.0 / 24.0, latitude, longitude, tzHours, 0.833, true);
    var ogle = computeSolarNoon(julianDate, 12.0 / 24.0, longitude, tzHours);
    var ikindi = computeAsrTime(julianDate, 15.0 / 24.0, latitude, longitude, tzHours, asrShadowFactor);
    var aksam = computeSunAngleTime(julianDate, 18.0 / 24.0, latitude, longitude, tzHours, p.maghribAngle, false);
    if (Number.isNaN(aksam)) {
      aksam = computeSunAngleTime(julianDate, 18.0 / 24.0, latitude, longitude, tzHours, 0.833, false);
    }

    var nightHours = fixHour(gunes - aksam);
    if (Number.isNaN(imsak) || imsak < 0.0 || imsak >= gunes) {
      imsak = gunes - (p.fajrAngle / 60.0) * nightHours;
      if (imsak < 0.02) imsak = Math.max(0.02, gunes * 0.5);
    }

    var yatsi = 0.0;
    if (p.ishaMinutesAfterMaghrib > 0) {
      yatsi = aksam + p.ishaMinutesAfterMaghrib / 60.0;
    } else {
      yatsi = computeSunAngleTime(julianDate, 19.5 / 24.0, latitude, longitude, tzHours, p.ishaAngle, false);
      if (Number.isNaN(yatsi) || yatsi >= 23.99 || yatsi <= aksam) {
        yatsi = aksam + (p.ishaAngle / 60.0) * nightHours;
      }
    }

    if (p.applyDiyanetTemkin) {
      imsak -= 2.0 / 60.0;
      gunes -= 7.0 / 60.0;
      ogle += 5.0 / 60.0;
      ikindi += 4.0 / 60.0;
      aksam += 7.0 / 60.0;
      yatsi += 2.0 / 60.0;
    }

    gunes = clamp(gunes, 0.05, 23.90);
    imsak = clamp(imsak, 0.01, Math.max(0.01, gunes - 1.0 / 60.0));
    aksam = clamp(aksam, ogle + 1.0 / 60.0, 23.95);
    yatsi = clamp(yatsi, Math.min(23.98, aksam + 1.0 / 60.0), 23.98);

    var mins = [
      hoursToTotalMinutes(imsak),
      hoursToTotalMinutes(gunes),
      hoursToTotalMinutes(ogle),
      hoursToTotalMinutes(ikindi),
      hoursToTotalMinutes(aksam),
      hoursToTotalMinutes(yatsi)
    ];
    var strs = mins.map(formatMinutes);

    return {
      year: year,
      month: month,
      day: day,
      imsak: strs[0],
      gunes: strs[1],
      ogle: strs[2],
      ikindi: strs[3],
      aksam: strs[4],
      yatsi: strs[5],
      minutes: mins,
      times: strs
    };
  }

  function addDaysYMD(year, month, day, offsetDays) {
    var d = new Date(Date.UTC(year, month - 1, day + offsetDays));
    return {
      year: d.getUTCFullYear(),
      month: d.getUTCMonth() + 1,
      day: d.getUTCDate()
    };
  }

  function calculateMonthlySchedule(startYear, startMonth, startDay, daysCount, latitude, longitude, ianaTz, methodKey, asrMethod) {
    var out = [];
    for (var i = 0; i < daysCount; i++) {
      var ymd = addDaysYMD(startYear, startMonth, startDay, i);
      out.push(calculatePrayerTimes(ymd.year, ymd.month, ymd.day, latitude, longitude, ianaTz, methodKey, asrMethod));
    }
    return out;
  }

  function calculateQibla(latitude, longitude) {
    var kaabaLat = 21.422487;
    var kaabaLon = 39.826206;
    var lat1 = latitude * DEG_TO_RAD;
    var lat2 = kaabaLat * DEG_TO_RAD;
    var dLon = (kaabaLon - longitude) * DEG_TO_RAD;
    var y = Math.sin(dLon) * Math.cos(lat2);
    var x = Math.cos(lat1) * Math.sin(lat2) - Math.sin(lat1) * Math.cos(lat2) * Math.cos(dLon);
    var bearing = (Math.atan2(y, x) * RAD_TO_DEG + 360.0) % 360.0;
    var dirsTr = ["K", "KD", "D", "GD", "G", "GB", "B", "KB"];
    var dirsEn = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"];
    var idx = Math.round(bearing / 45.0) % 8;
    return {
      bearing: Math.round(bearing) % 360,
      dirTr: dirsTr[idx],
      dirEn: dirsEn[idx]
    };
  }

  function getNextPrayerInfo(todayTimes, tomorrowImsakStr, nowSeconds) {
    var mins = todayTimes.minutes;
    for (var i = 0; i < mins.length; i++) {
      var targetSec = mins[i] * 60;
      if (nowSeconds < targetSec) {
        return {
          activeIndex: i - 1,
          nextIndex: i,
          remainingSeconds: targetSec - nowSeconds,
          targetTime: todayTimes.times[i],
          isTomorrow: false
        };
      }
    }
    var parts = tomorrowImsakStr.split(":");
    var tomImsakSec = (parseInt(parts[0], 10) * 60 + parseInt(parts[1], 10)) * 60;
    return {
      activeIndex: 5,
      nextIndex: 0,
      remainingSeconds: (86400 - nowSeconds) + tomImsakSec,
      targetTime: tomorrowImsakStr,
      isTomorrow: true
    };
  }

  var api = {
    METHOD_PARAMS: METHOD_PARAMS,
    toJulianDate: toJulianDate,
    sunDeclinationAndEqT: sunDeclinationAndEqT,
    getRecommendedMethodForCountry: getRecommendedMethodForCountry,
    getTimeZoneOffsetHours: getTimeZoneOffsetHours,
    getCityNowParts: getCityNowParts,
    calculatePrayerTimes: calculatePrayerTimes,
    calculateMonthlySchedule: calculateMonthlySchedule,
    addDaysYMD: addDaysYMD,
    calculateQibla: calculateQibla,
    getNextPrayerInfo: getNextPrayerInfo,
    formatMinutes: formatMinutes
  };

  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  }
  if (root && typeof window !== "undefined") {
    root.PrayerEngine = api;
  }
})(typeof globalThis !== "undefined" ? globalThis : this);
