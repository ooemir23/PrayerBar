(function () {
  "use strict";

  function pad2(n) {
    return n < 10 ? "0" + n : String(n);
  }

  function formatDuration(sec) {
    var s = Math.max(0, Math.floor(sec));
    var h = Math.floor(s / 3600);
    var m = Math.floor((s % 3600) / 60);
    var r = s % 60;
    return pad2(h) + ":" + pad2(m) + ":" + pad2(r);
  }

  function formatDateDisplay(ymd, lang) {
    var monthsTr = ["Ocak", "Şubat", "Mart", "Nisan", "Mayıs", "Haziran", "Temmuz", "Ağustos", "Eylül", "Ekim", "Kasım", "Aralık"];
    var monthsEn = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
    if (lang === "tr") {
      return ymd.day + " " + monthsTr[ymd.month - 1] + " " + ymd.year;
    }
    return monthsEn[ymd.month - 1] + " " + ymd.day + ", " + ymd.year;
  }

  function initSearchFilter() {
    var input = document.getElementById("citySearchInput");
    if (!input) return;
    var cards = Array.prototype.slice.call(document.querySelectorAll("[data-city-filter]"));
    var emptyNotice = document.getElementById("citySearchEmpty");

    function normalizeStr(s) {
      return String(s || "")
        .toLowerCase()
        .replace(/ı/g, "i")
        .replace(/İ/g, "i")
        .replace(/ğ/g, "g")
        .replace(/ü/g, "u")
        .replace(/ş/g, "s")
        .replace(/ö/g, "o")
        .replace(/ç/g, "c");
    }

    input.addEventListener("input", function () {
      var q = normalizeStr(input.value.trim());
      var visibleCount = 0;
      for (var i = 0; i < cards.length; i++) {
        var hay = normalizeStr(cards[i].getAttribute("data-city-filter"));
        var match = !q || hay.indexOf(q) !== -1;
        cards[i].style.display = match ? "" : "none";
        if (match) visibleCount++;
      }
      if (emptyNotice) {
        emptyNotice.hidden = visibleCount > 0;
      }
    });
  }

  function initCityHydration() {
    var dataEl = document.getElementById("page-city-data");
    if (!dataEl || !window.PrayerEngine) return;

    var cfg;
    try {
      cfg = JSON.parse(dataEl.textContent);
    } catch (e) {
      return;
    }

    var Engine = window.PrayerEngine;
    var prayerNamesTr = ["İmsak", "Güneş", "Öğle", "İkindi", "Akşam", "Yatsı"];
    var prayerNamesEn = ["Fajr", "Sunrise", "Dhuhr", "Asr", "Maghrib", "Isha"];
    var prayerNames = cfg.lang === "tr" ? prayerNamesTr : prayerNamesEn;

    var lastRenderedDayKey = "";
    var cachedSchedule = null;
    var cachedToday = null;
    var cachedTomorrowImsak = null;

    function recomputeForNow(nowDate) {
      var local = Engine.getCityNowParts(cfg.ianaTz, nowDate);
      var dayKey = local.year + "-" + pad2(local.month) + "-" + pad2(local.day);

      if (dayKey !== lastRenderedDayKey) {
        lastRenderedDayKey = dayKey;
        cachedSchedule = Engine.calculateMonthlySchedule(
          local.year, local.month, local.day, 30,
          cfg.lat, cfg.lon, cfg.ianaTz, cfg.method, "standard"
        );
        cachedToday = cachedSchedule[0];
        cachedTomorrowImsak = cachedSchedule[1] ? cachedSchedule[1].imsak : cachedToday.imsak;

        var dateEl = document.getElementById("liveDateLabel");
        if (dateEl) {
          dateEl.textContent = formatDateDisplay(local, cfg.lang);
        }

        // Update 6 today prayer time elements
        for (var i = 0; i < 6; i++) {
          var tEl = document.getElementById("prayer-time-" + i);
          if (tEl) tEl.textContent = cachedToday.times[i];
        }

        // Update 30-day Imsakiye rows
        var tbody = document.getElementById("imsakiyeBody");
        if (tbody && cachedSchedule) {
          var rowsHtml = "";
          for (var r = 0; r < cachedSchedule.length; r++) {
            var row = cachedSchedule[r];
            var cls = r === 0 ? ' class="today-row"' : "";
            rowsHtml += "<tr" + cls + ">" +
              "<td>" + formatDateDisplay(row, cfg.lang) + "</td>" +
              "<td>" + row.imsak + "</td>" +
              "<td>" + row.gunes + "</td>" +
              "<td>" + row.ogle + "</td>" +
              "<td>" + row.ikindi + "</td>" +
              '<td class="ims-iftar">' + row.aksam + "</td>" +
              "<td>" + row.yatsi + "</td>" +
              "</tr>";
          }
          tbody.innerHTML = rowsHtml;
        }
      }

      var nowSec = local.hour * 3600 + local.minute * 60 + local.second;
      var info = Engine.getNextPrayerInfo(cachedToday, cachedTomorrowImsak, nowSec);

      for (var k = 0; k < 6; k++) {
        var card = document.getElementById("prayer-card-" + k);
        var tag = document.getElementById("prayer-tag-" + k);
        if (!card) continue;
        card.classList.toggle("is-next", k === info.nextIndex && !info.isTomorrow ? true : (k === 0 && info.isTomorrow));
        card.classList.toggle("is-active", k === info.activeIndex);
        if (tag) {
          if ((k === info.nextIndex && !info.isTomorrow) || (k === 0 && info.isTomorrow)) {
            tag.textContent = cfg.lang === "tr" ? "Sıradaki" : "Next";
            tag.hidden = false;
          } else if (k === info.activeIndex) {
            tag.textContent = cfg.lang === "tr" ? "Şu an" : "Now";
            tag.hidden = false;
          } else {
            tag.hidden = true;
          }
        }
      }

      var cdLabel = document.getElementById("liveCountdownLabel");
      var cdVal = document.getElementById("liveCountdownValue");
      var cdSub = document.getElementById("liveCountdownSub");
      var targetName = prayerNames[info.nextIndex];
      if (cdLabel) {
        cdLabel.textContent = cfg.lang === "tr"
          ? (targetName + " vaktine kalan")
          : ("Time until " + targetName);
      }
      if (cdVal) {
        cdVal.textContent = formatDuration(info.remainingSeconds);
      }
      if (cdSub) {
        cdSub.textContent = (cfg.lang === "tr" ? "Vakit saati: " : "Prayer time: ") +
          info.targetTime + " · " + pad2(local.hour) + ":" + pad2(local.minute);
      }
    }

    recomputeForNow(new Date());
    setInterval(function () {
      recomputeForNow(new Date());
    }, 1000);
  }

  function initPrintButtons() {
    var btns = document.querySelectorAll("[data-print-imsakiye]");
    for (var i = 0; i < btns.length; i++) {
      btns[i].addEventListener("click", function () {
        window.print();
      });
    }
  }

  document.addEventListener("DOMContentLoaded", function () {
    initSearchFilter();
    initCityHydration();
    initPrintButtons();
  });
})();
