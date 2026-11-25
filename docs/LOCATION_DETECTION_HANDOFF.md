# Location Detection Handoff

This note describes PrayerBar's location-detection and worldwide city-search implementation for future coding agents.

## Behavior

- Location auto-detection is enabled by default and runs in the background when the app starts.
- Detection tries the Windows Location API first. When that is unavailable, it falls back to IP geolocation providers (`ipwho.is`, then `get.geojs.io`).
- Settings let users disable automatic detection. Choosing a city or applying custom coordinates turns it off so startup detection will not overwrite the user's manual choice.
- Settings city search keeps the built-in catalog and asynchronously adds global results from the Open-Meteo Geocoding API after a short typing debounce. Network failure leaves built-in search results usable.
- Detected coordinates update prayer-time calculations and the country-recommended calculation method when that option is enabled. Keep timezone conversion in `schedule_service.cpp` aligned with any new cities or regions.

## Key files

- `src/core/schedule_service.cpp`: Windows/IP detection, IANA-to-Windows timezone mapping, global geocoding.
- `src/core/city_catalog.cpp`: offline city catalog, including Australian cities such as Adelaide.
- `src/ui/app_host.cpp`: startup detection and applying its result.
- `src/ui/settings_window.cpp`: manual detection, global search, and manual-choice behavior.
- `src/core/models.h` and `src/core/settings_store.cpp`: persisted `autoDetectLocation` setting and coordinate bounds.
- `docs/PRIVACY_POLICY.md` and `website/privacy.html`: disclose location and geocoding network behavior.

## Important invariants

- Preserve the manual override: a manually selected city or custom coordinate must not be silently replaced at the next startup.
- Keep city searching usable offline; global results are an addition to, not a replacement for, `SearchCities`.
- Windows location may be unavailable due to OS permissions. IP geolocation is approximate, so do not present it as precise.
- City search sends the query and request IP to Open-Meteo. Keep both privacy-policy versions accurate when changing providers or behavior.
- Settings support latitudes from -90 to +90. Preserve this for locations at high latitudes.

## Validation

Run `.\scripts\build.ps1` to build the Windows app and execute the native C++ test suites. The script links the Windows Location API for both the app and tests.
