# Agri Data Sidebar

## Overview

The Agri Data sidebar provides real-time external environmental data alongside the onboard sensor readings. All API calls are made **browser-side in JavaScript** — the Brain Board itself only serves the dashboard HTML and the `/data` endpoint. No API keys are required; all sources are free and open.

---

## Location

Enter any of the following in the location field:

- City name: `Winnipeg`
- City + country: `Saskatoon, CA`
- ZIP/postal code: `R3C 0V8`
- Latitude, Longitude: `49.8954, -97.1385`

Location is resolved via the **Open-Meteo Geocoding API** and stored in the browser session. External data refreshes every **60 seconds** after a location is set.

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

### Soil & Agriculture
*Source: Open-Meteo*

| Parameter | Unit (Metric) | Unit (Imperial) |
|---|---|---|
| Soil Temperature (0–7 cm) | °C | °F |
| Soil Temperature (7–28 cm) | °C | °F |
| Soil Moisture | m³/m³ | m³/m³ |
| Evapotranspiration (ET₀) | mm | in |
| Vapor Pressure Deficit (VPD) | kPa | kPa |

### Moon
*Source: Sunrise-Sunset.org*

| Parameter | Notes |
|---|---|
| Moon Phase | New / Waxing Crescent / First Quarter / Waxing Gibbous / Full |
| Moonrise | |
| Moonset | |

### Air Quality
*Source: [Open-Meteo Air Quality API](https://air-quality-api.open-meteo.com)*

| Parameter | Unit |
|---|---|
| PM2.5 Particulates | μg/m³ |
| Pollen Count | Low / Moderate / High / Very High |

---

## External APIs Used

All free, no authentication required, no API keys.

| API | URL | Used For |
|---|---|---|
| Open-Meteo Geocoding | `geocoding-api.open-meteo.com` | Location search |
| Open-Meteo Forecast | `api.open-meteo.com` | Weather, soil, solar, UV, ET₀, VPD |
| Sunrise-Sunset.org | `api.sunrise-sunset.org` | Sun/moon times, day length, moon phase |
| Open-Meteo AQI | `air-quality-api.open-meteo.com` | PM2.5, pollen |

---

## Units Toggle

Switch between **Metric** and **Imperial** at any time using the toggle at the top of the sidebar. The change applies immediately to all active parameter chips and to the board sensor readings.
