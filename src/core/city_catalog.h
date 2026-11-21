#pragma once

#include "models.h"
#include <optional>
#include <string_view>
#include <vector>

namespace vakit {

const std::vector<CityLocation>& GetAllCities();
const CityLocation& GetDefaultCity();
std::optional<CityLocation> FindCityById(std::wstring_view id);
std::optional<CityLocation> FindNearestCity(double latitude, double longitude, double maxDistanceKm = 30.0);
std::vector<CityLocation> SearchCities(std::wstring_view query, size_t maxResults = 60);
std::wstring ResolveCountryCodeFromName(std::wstring_view countryName);

} // namespace vakit
