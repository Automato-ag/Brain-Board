# Agri Data Sidebar

## Overview

The Agri Data sidebar provides real-time external environmental data alongside the onboard sensor readings. All API calls are routed through the board's `/proxy` endpoint — this ensures data loads correctly when accessing the board via its SoftAP in the field (no direct internet from the browser required). All sources are free; no API keys are required.

---

## Location

Enter any of the following in the location field:

- City name: `Winnipeg`
- City + country: `Saskatoon, CA`
- ZIP/postal code: `R3C 0V8`
- Latitude, Longitude: `49.8954, -97.1385`

Location is resolved via the **Open-Meteo Geocoding API** and persisted to the board's `/prefs` endpoint. External data refreshes every **60 seconds** after a location is set.

---

## Available Parameters

### Weather & Forecast
*Source: [Open-Meteo](https://open-meteo.com)*

| Parameter | Unit (Metric) | Unit (Imperial) |
|---|---|---|
| Outdoor Temperature | °C | °F |
| Precipitation | mm | in |
| Wind Speed | km/h | mph |
| Wind Direction | Cardinal + degrees | Cardinal + degrees |
| Cloud Cover | % | % |
| Outdoor Humidity | % | % |

### Solar & Light
*Source: Open-Meteo + [Sunrise-Sunset.org](https://sunrise-sunset.org/api)*

| Parameter | Notes |
|---|---|
| First Light / Civil Dawn | Start of civil twilight — earliest usable light |
| Sunrise | |
| Solar Noon | Peak sun angle |
| Sunset | |
| Last Light / Civil Dusk | End of civil twilight |
| Day Length | Hours and minutes |
| UV Index | |
| Shortwave Radiation | W/m² |
| Photoperiod | Day length classification (Short / Neutral / Long) + seasonal trend |

### Soil & Agriculture
*Source: Open-Meteo*

| Parameter | Unit (Metric) | Unit (Imperial) |
|---|---|---|
| Soil Temperature (0–7 cm) | °C | °F |
| Soil Temperature (7–28 cm) | °C | °F |
| Soil Moisture | m³/m³ | m³/m³ |
| Evapotranspiration (ET₀) | mm | in |
| Vapor Pressure Deficit (VPD) | kPa | kPa |
| Frost Risk (derived) | Low / Moderate / High / Severe — radiative cooling model from outdoor temp, cloud cover, wind speed | |
| Growing Degree Days | °C·day or °F·day — season-to-date accumulation since Jan 1 (base 10°C / 50°F) | |
| Chill Hours | hrs — season-to-date accumulation since Nov 1 (N) / May 1 (S), at or below 7.2°C (45°F); ±5% estimate | |

### Moon
*Source: USNO + Sunrise-Sunset.org*

| Parameter | Notes |
|---|---|
| Moon Phase | New / Waxing Crescent / First Quarter / Waxing Gibbous / Full / Waning Gibbous / Last Quarter / Waning Crescent |
| Moonrise | |
| Moonset | |

### Weather Alerts
*Source: [NOAA National Weather Service](https://api.weather.gov)*

| Parameter | Notes |
|---|---|
| NWS Frost Alerts | Active frost / freeze warnings and advisories for the selected location |

### Air Quality
*Source: [Open-Meteo Air Quality API](https://air-quality-api.open-meteo.com)*

| Parameter | Unit |
|---|---|
| PM2.5 Particulates | μg/m³ |
| Pollen Count | Low / Moderate / High / Very High |

### DLI Accumulator (onboard sensor)
*Source: TSL2591 lux sensor — accumulated in firmware*

| Parameter | Notes |
|---|---|
| Daily Light Integral | mol/m² — accumulated photosynthetically active light since midnight. Resets at midnight. Set a crop target in Settings → Board Settings to track progress as a percentage. |

---

## External APIs Used

All free, no authentication required, no API keys. All requests routed through the board's `/proxy` endpoint.

| API | Domain | Used For |
|---|---|---|
| Open-Meteo Geocoding | `geocoding-api.open-meteo.com` | Location search |
| Open-Meteo Forecast | `api.open-meteo.com` | Weather, soil, solar, UV, ET₀, VPD, frost risk, GDD bridge |
| Open-Meteo ERA5 Archive | `archive-api.open-meteo.com` | Season-to-date GDD and Chill Hours accumulation |
| Sunrise-Sunset.org | `api.sunrise-sunset.org` | Sun/moon times, day length, moon phase, photoperiod |
| USNO | `aa.usno.navy.mil` | Moon phase confirmation |
| Open-Meteo AQI | `air-quality-api.open-meteo.com` | PM2.5, pollen |
| NOAA NWS | `api.weather.gov` | Frost/freeze alerts |

---

## Units Toggle

Switch between **Metric** and **Imperial** at any time using the toggle in **Settings → Display & Behavior**. The change applies immediately to all active parameter chips and to the board sensor readings. Unit preference is persisted to the board.

---

## Preferences Persistence

Location, active parameters, units, and source enable/disable state are all persisted to the board's `/prefs` endpoint (LittleFS + NVS backup). Settings survive browser/device changes and LittleFS firmware uploads.
