#include <lvgl/lvgl.h>
#include "displayapp/screens/WatchFaceMother.h"
#include "components/battery/BatteryController.h"
#include "components/ble/BleController.h"
#include "components/settings/Settings.h"
#include "components/ble/SimpleWeatherService.h"
#include "displayapp/screens/WeatherSymbols.h"

using namespace Pinetime::Applications::Screens;

namespace {
  // Vert phosphore vif, sur fond noir (esthétique MU/TH/UR).
  constexpr lv_color_t phosphorGreen = LV_COLOR_MAKE(0x00, 0xff, 0x00);

  // Crée un label vert, centré, ajouté au conteneur centré.
  lv_obj_t* MakeLabel(lv_obj_t* parent) {
    lv_obj_t* label = lv_label_create(parent, nullptr);
    lv_obj_set_style_local_text_color(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, phosphorGreen);
    lv_label_set_align(label, LV_LABEL_ALIGN_CENTER);
    return label;
  }
}

WatchFaceMother::WatchFaceMother(Controllers::DateTime& dateTimeController,
                                 const Controllers::Battery& batteryController,
                                 const Controllers::Ble& bleController,
                                 Controllers::Settings& settingsController,
                                 Controllers::SimpleWeatherService& weatherService)
  : currentDateTime {{}},
    dateTimeController {dateTimeController},
    batteryController {batteryController},
    bleController {bleController},
    settingsController {settingsController},
    weatherService {weatherService} {

  // Fond noir intégral.
  lv_obj_set_style_local_bg_color(lv_scr_act(), LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);

  // Conteneur en colonne centrée : il se dimensionne sur son contenu (FIT_TIGHT)
  // et les labels sont centrés les uns sous les autres (COLUMN_MID). Le bloc entier
  // est ensuite aligné au centre de l'écran.
  lv_obj_t* container = lv_cont_create(lv_scr_act(), nullptr);
  lv_cont_set_layout(container, LV_LAYOUT_COLUMN_MID);
  lv_cont_set_fit(container, LV_FIT_TIGHT);
  lv_obj_set_style_local_pad_inner(container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, -1);
  lv_obj_set_style_local_bg_opa(container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);

  // En-tête : MU / TH / UR  6000
  labelHeader = MakeLabel(container);
  lv_label_set_text_static(labelHeader, "MU / TH / UR  6000");

  // Emblème Weyland-Yutani (logo « texte stylisé », ailes approximées en ASCII).
  labelLogo = MakeLabel(container);
  lv_label_set_text_static(labelLogo, ">>--==[ W-Y ]==--<<");

  // Heure en gros.
  labelTime = MakeLabel(container);
  lv_obj_set_style_local_text_font(labelTime, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &jetbrains_mono_42);

  // Nom du vaisseau.
  labelShip = MakeLabel(container);
  lv_label_set_text_static(labelShip, "N O S T R O M O");

  // Plaque constructeur.
  labelMfr = MakeLabel(container);
  lv_label_set_text_static(labelMfr, "MANUFACTURER: LOCKMART");

  labelMdl = MakeLabel(container);
  lv_label_set_text_static(labelMdl, "MODEL: CM-998 BISON");

  labelCls = MakeLabel(container);
  lv_label_set_text_static(labelCls, "CLASS: M-CLASS");

  labelAffil = MakeLabel(container);
  lv_label_set_text_static(labelAffil, "AFFIL: WEYLAND-YUTANI CORP");

  // Batterie + météo sur une ligne (ajout demandé : météo).
  batteryWeather = MakeLabel(container);

  // Statut de connexion au téléphone (ajout demandé).
  connectState = MakeLabel(container);

  // Date + jour de la semaine.
  labelDate = MakeLabel(container);

  lv_obj_align(container, nullptr, LV_ALIGN_CENTER, 0, 0);

  taskRefresh = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);
  Refresh();
}

WatchFaceMother::~WatchFaceMother() {
  lv_task_del(taskRefresh);
  lv_obj_clean(lv_scr_act());
}

void WatchFaceMother::Refresh() {
  currentDateTime = std::chrono::time_point_cast<std::chrono::seconds>(dateTimeController.CurrentDateTime());
  if (currentDateTime.IsUpdated()) {
    uint8_t hour = dateTimeController.Hours();
    uint8_t minute = dateTimeController.Minutes();

    if (settingsController.GetClockType() == Controllers::Settings::ClockType::H12) {
      char ampmChar[3] = "AM";
      if (hour == 0) {
        hour = 12;
      } else if (hour == 12) {
        ampmChar[0] = 'P';
      } else if (hour > 12) {
        hour = hour - 12;
        ampmChar[0] = 'P';
      }
      lv_label_set_text_fmt(labelTime, "%02d:%02d %s", hour, minute, ampmChar);
    } else {
      lv_label_set_text_fmt(labelTime, "%02d:%02d", hour, minute);
    }

    currentDate = std::chrono::time_point_cast<std::chrono::days>(currentDateTime.Get());
    if (currentDate.IsUpdated()) {
      uint16_t year = dateTimeController.Year();
      uint8_t month = static_cast<uint8_t>(dateTimeController.Month());
      uint8_t day = dateTimeController.Day();
      // Date jj/mm/aaaa (année réelle) + jour de la semaine.
      lv_label_set_text_fmt(labelDate, "%02d/%02d/%04d  %s", day, month, year, dateTimeController.DayOfWeekToString());
    }
  }

  powerPresent = batteryController.IsPowerPresent();
  batteryPercentRemaining = batteryController.PercentRemaining();
  currentWeather = weatherService.Current();
  if (batteryPercentRemaining.IsUpdated() || powerPresent.IsUpdated() || currentWeather.IsUpdated()) {
    int batt = batteryPercentRemaining.Get();
    auto optCurrentWeather = currentWeather.Get();
    if (optCurrentWeather) {
      int16_t temp = optCurrentWeather->temperature.Celsius();
      char tempUnit = 'C';
      if (settingsController.GetWeatherFormat() == Controllers::Settings::WeatherFormat::Imperial) {
        temp = optCurrentWeather->temperature.Fahrenheit();
        tempUnit = 'F';
      }
      lv_label_set_text_fmt(batteryWeather,
                            "BAT %d%%   %d%c %s",
                            batt,
                            temp,
                            tempUnit,
                            Symbols::GetSimpleCondition(optCurrentWeather->iconId));
    } else {
      lv_label_set_text_fmt(batteryWeather, "BAT %d%%   --- ", batt);
    }
  }

  bleState = bleController.IsConnected();
  bleRadioEnabled = bleController.IsRadioEnabled();
  if (bleState.IsUpdated() || bleRadioEnabled.IsUpdated()) {
    if (!bleRadioEnabled.Get()) {
      lv_label_set_text_static(connectState, "LINK: OFFLINE");
    } else if (bleState.Get()) {
      lv_label_set_text_static(connectState, "LINK: ONLINE");
    } else {
      lv_label_set_text_static(connectState, "LINK: NO SIGNAL");
    }
  }
}
