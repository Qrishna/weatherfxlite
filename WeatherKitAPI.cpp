/*
    weatherfxlite
    A lite replacement for the venerable Bushnell WeatherFX station. RIP.
    Copyright (C) 2022 iAchieved.it LLC

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "WeatherKitAPI.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <jwt-cpp/jwt.h>

#include <icons/icons.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
using std::cout;
using std::endl;

#define WEATHERKIT_API_URL "https://weatherkit.apple.com/api/v1/weather/"

#include "config.h"

static std::map<QString, QString> conditionCodeMap = {
  {"Clear",                  "Clear"},
  {"Cloudy",                 "Cloudy"},
  {"Dust",                   "Dust"},
  {"Fog",                    "Fog"},
  {"Haze",                   "Haze"},
  {"MostlyClear",            "Mostly Clear"},
  {"MostlyCloudy",           "Mostly Cloudy"},
  {"PartlyCloudy",           "Partly Cloudy"},
  {"ScatteredThunderstorms", "Scattered Thunderstorms"},
  {"Smoke",                  "Smoke"},
  {"Breezy",                 "Breezy"},
  {"Windy",                  "Windy"},
  {"Drizzle",                "Drizzle"},
  {"HeavyRain",              "Heavy Rain"},
  {"Rain",                   "Rain"},
  {"Showers",                "Showers"},
  {"Flurries",               "Flurries"},
  {"HeavySnow",              "Heavy Snow"},
  {"MixedRainAndSleet",      "Mixed Rain and Sleet"},
  {"MixedRainAndSnow",       "Mixed Rain and Snow"},
  {"MixedRainfall",          "Mixed Rainfall"},
  {"MixedSnowAndSleet",      "Mixed Snow and Sleet"},
  {"ScatteredShowers",       "Scattered Showers"},
  {"ScatteredSnowShowers",   "Scattered Snow Showers"},
  {"Sleet",                  "Sleet"},
  {"Snow",                   "Snow"},
  {"SnowShowers",            "Snow Showers"},
  {"Blizzard",               "Blizzard"},
  {"BlowingSnow",            "Blowing Snow"},
  {"FreezingDrizzle",        "Freezing Drizzle"},
  {"FreezingRain",           "Freezing Rain"},
  {"Frigid",                 "Frigid"},
  {"Hail",                   "Hail"},
  {"Hot",                    "Hot"},
  {"Hurricane",              "Hurricane"},
  {"IsolatedThunderstorms",  "Isolated Thunderstorms"},
  {"SevereThunderstorm",     "Severe Thunderstorm"},
  {"Thunderstorms",          "Thunderstorms"},
  {"Tornado",                "Tornado"},
  {"TropicalStorm",          "Tropical Storm"},
};

static std::map<QString, WeatherIcon> nightIcons = {
  {"Clear",       {icons_night_Clear_png,       icons_night_Clear_png_len}},
  {"MostlyClear", {icons_night_MostlyClear_png, icons_night_MostlyClear_png_len}},
};

static std::map<QString, WeatherIcon> dayIcons = {
  {"Clear", {icons_day_Clear_png,icons_day_Clear_png_len}},
  {"MostlyClear", {icons_day_Clear_png, icons_day_Clear_png_len}},
  {"MostlyCloudy", {icons_day_MostlyCloudy_png, icons_day_MostlyCloudy_png_len}},
  {"PartlyCloudy", {icons_day_PartlyCloudy_png, icons_day_PartlyCloudy_png_len}},
  {"Rain",         {icons_day_Rain_png, icons_day_Rain_png_len}},
  {"Thunderstorms", {icons_day_Thunderstorms_png, icons_day_Thunderstorms_png_len}},
};

// Constructor
WeatherKitAPI::WeatherKitAPI() {
}

// Destructor
WeatherKitAPI::~WeatherKitAPI() {
}

void WeatherKitAPI::updateCurrentConditions(void) {

  QString url = WEATHERKIT_API_URL;
  url += "en_US/";
  url += LATLNG;
  url += "?dataSets=currentWeather";

  QString bearer(makeJWT().c_str());

  QUrl current(url);

  RequestHeaders headers;
  headers.append(qMakePair(QByteArray("Authorization"), QByteArray("Bearer " + bearer.toUtf8())));

  fDownloader = new FileDownloader(current, this, &headers);

  connect(fDownloader, SIGNAL(downloaded()), this, SLOT(parseCurrentConditions()));
}

void WeatherKitAPI::updateCurrentForecast(void) {

  qDebug() << "Update current forecast";
  
  QString url = WEATHERKIT_API_URL;
  url += "en_US/";
  url += LATLNG;
  url += "?dataSets=forecastDaily";

  QString bearer(makeJWT().c_str());

  QUrl current(url);

  RequestHeaders headers;
  headers.append(qMakePair(QByteArray("Authorization"), QByteArray("Bearer " + bearer.toUtf8())));

  fcastDownloader = new FileDownloader(current, this, &headers);

  connect(fcastDownloader, SIGNAL(downloaded()), this, SLOT(parseCurrentForecast()));

}

void WeatherKitAPI::parseCurrentConditions(void) {
  QString current(fDownloader->downloadedData());

  cout << current.toStdString() << endl;

  QJsonDocument doc = QJsonDocument::fromJson(current.toUtf8());
  QJsonObject json = doc.object();

  if (json.contains("currentWeather")) {
    QJsonObject w = json["currentWeather"].toObject();
    QJsonValue t = w["temperature"];

    currentConditions.temperature = floor((t.toDouble() * 9.0) / 5.0 + 32); // Celsius to Fahrenheit

    QJsonValue c = w["conditionCode"];

    currentConditions.condition = conditionCodeMap[c.toString()];

    QJsonValue d = w["daylight"];

    #if 0
    if (d.toBool()) {
      currentConditions.iconPath = "icons/day/" + iconPathMap[c.toString()];
    } else {
      currentConditions.iconPath = "icons/night/" + iconPathMap[c.toString()];
    }
    #endif

    if (d.toBool()) {
      currentConditions.icon = dayIcons[c.toString()];
    } else {
      currentConditions.icon = nightIcons[c.toString()];
    }

  }

  fDownloader->deleteLater();

  emit currentConditionsUpdate();

}

void WeatherKitAPI::parseCurrentForecast(void) {

  qDebug() << "Parse current forecast"; 

  QDateTime now = QDateTime().currentDateTime();

  QString forecast(fcastDownloader->downloadedData());

  QJsonDocument doc = QJsonDocument::fromJson(forecast.toUtf8());
  QJsonObject json = doc.object();

  if (json.contains("forecastDaily")) {

    QJsonObject forecastDaily = json["forecastDaily"].toObject();

    if (forecastDaily.contains("days")) {

      QJsonArray forecasts = forecastDaily["days"].toArray(); // Daily forecasts

      for (auto i = forecasts.begin(); i != forecasts.end(); i++) {

        //qDebug() << (*i);

        QJsonObject f = QJsonValue(*i).toObject();

        if (f.contains("forecastStart")) {

          QJsonValue dt = f["forecastStart"];
          QDateTime qdt = QDateTime::fromString(dt.toString(), "yyyy-MM-ddTHH:mm:ssZ");

          //qDebug() << qdt;

          // Is the forecast for today?
          if (qdt.date() == now.date()) {

            double hi = floor((f["temperatureMax"].toDouble()  * 9.0) / 5.0 + 32);
            double lo = floor((f["temperatureMin"].toDouble() * 9.0) / 5.0 + 32);

            currentConditions.low = lo;
            currentConditions.high = hi;

          } 
        }
      }
    }

    fcastDownloader->deleteLater();
    emit currentForecastUpdate();
  }
}


CurrentConditions WeatherKitAPI::getCurrentConditions(void) {
  return currentConditions;
}

CurrentConditions WeatherKitAPI::getCurrentForecast(void) {
  return currentConditions;
}

std::string WeatherKitAPI::makeJWT(void) {

  // Compile key directly into binary
  const char* priv_key(
  #include "weatherkit/priv.key"
  );

  const char* pub_key = (
  #include "weatherkit/pub.key"
  );

  // Load key from file
#if 0
  std::ifstream t("weatherkit/priv.key");
  std::stringstream buffer;
  buffer << t.rdbuf();

  std::string priv_key = buffer.str();

  t.open("weatherkit/pub.key");
  buffer << t.rdbuf();
  std::string pub_key = buffer.str();
#endif

  auto token = jwt::create()
                   .set_issuer(APPLE_DEVELOPER_TEAM_ID)
                   .set_type("JWT")
                   .set_key_id(WEATHERKIT_KEY_ID)
                   .set_header_claim("id", jwt::claim(std::string(WEATHERKIT_APP_ID)))
                   .set_subject(WEATHERKIT_APP)
                   .set_issued_at(std::chrono::system_clock::now())
                   .set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds{120})
                   .sign(jwt::algorithm::es256(pub_key, priv_key));

  return token;
}
