# Location Detection Handoff

Read this note before changing location detection, city search, timezone handling, or location privacy disclosures.

## Behavior

- Automatic location detection is enabled for fresh installs and runs asynchronously after startup. Windows Location is tried first; IP geolocation (`ipwho.is`, then `get.geojs.io`) is the fallback and may be approximate.
- Existing 2.4.0.0 settings without an auto-detection preference are migrated conservatively: the default Istanbul location can opt into detection, while a saved non-default city or custom coordinates remain manual.
- Settings allow automatic detection to be turned off and on. Selecting a city or applying custom coordinates turns it off; a late detection result must not overwrite a newer manual choice.
- City search always starts with the built-in offline catalog. After a short debounce, it adds worldwide results from Open-Meteo Geocoding; network failure must not remove or disable offline results.
- Coordinates and timezone feed the same prayer calculation, online month sync, and Imsakiye views. Keep IANA-to-Windows timezone mappings aligned with search results and regional cities.
- The Australian offline catalog includes Adelaide, Sydney, Melbourne, Brisbane, Perth, Darwin, and Canberra. Adelaide uses `Cen. Australia Standard Time` and observes daylight-saving transitions.

## Privacy

- Location detection is enabled by default on fresh installs. Users can disable it or choose a city/coordinates manually.
- IP providers may receive the request IP. Worldwide city search sends the search query and request IP to `geocoding-api.open-meteo.com`.
- Monthly prayer-time sync sends configured coordinates and calculation method to `api.aladhan.com` only when online sync is enabled.
- Do not claim that these requests never occur. Keep both `docs/PRIVACY_POLICY.md` and `website/privacy.html` accurate when behavior or providers change.

## Key files and validation

- `src/core/schedule_service.cpp`: Windows/IP detection, IANA timezone mapping, worldwide geocoding.
- `src/core/city_catalog.cpp`: offline city list and nearest-city lookup.
- `src/ui/app_host.cpp`: startup detection and applying results.
- `src/ui/settings_window.cpp`: manual detection, debounced search, and manual override.
- `src/core/models.h` and `src/core/settings_store.cpp`: persisted auto-detection preference and global coordinate bounds.
- `tests/vakit_tests.cpp`: settings migration, global latitude, and Adelaide timezone regression coverage.
- Run `.\scripts\build.ps1` to build the Windows application and run native tests.
