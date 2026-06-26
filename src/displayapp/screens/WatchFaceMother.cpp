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

  void SetGreen(lv_obj_t* label) {
    lv_obj_set_style_local_text_color(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, phosphorGreen);
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

  container = lv_cont_create(lv_scr_act(), nullptr);
  lv_cont_set_layout(container, LV_LAYOUT_COLUMN_LEFT);
  lv_cont_set_fit(container, LV_FIT_TIGHT);
  lv_obj_set_style_local_pad_inner(container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, -2);
  lv_obj_set_style_local_bg_opa(container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);

  // En-tête : constructeur + slogan Weyland-Yutani (logo "texte stylisé").
  labelHeader = lv_label_create(container, nullptr);
  SetGreen(labelHeader);
  lv_label_set_text_static(labelHeader, "/W-Y/ WEYLAND-YUTANI");

  labelMuthur = lv_label_create(container, nullptr);
  SetGreen(labelMuthur);
  lv_label_set_text_static(labelMuthur, "MU/TH/UR 6000  INTERFACE");

  // Heure en gros.
  labelTime = lv_label_create(container, nullptr);
  SetGreen(labelTime);
  lv_obj_set_style_local_text_font(labelTime, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &jetbrains_mono_42);

  labelDate = lv_label_create(container, nullptr);
  SetGreen(labelDate);

  batteryValue = lv_label_create(container, nullptr);
  SetGreen(batteryValue);

  weather = lv_label_create(container, nullptr);
  SetGreen(weather);

  connectState = lv_label_create(container, nullptr);
  SetGreen(connectState);

  // Bloc « plaque constructeur ».
  labelMfr = lv_label_create(container, nullptr);
  SetGreen(labelMfr);
  lv_label_set_text_static(labelMfr, "MFR.. LOCKMART");

  labelMdl = lv_label_create(container, nullptr);
  SetGreen(labelMdl);
  lv_label_set_text_static(labelMdl, "MDL.. CM-998 BISON");

  labelCls = lv_label_create(container, nullptr);
  SetGreen(labelCls);
  lv_label_set_text_static(labelCls, "CLS.. M-CLASS");

  // Invite avec curseur clignotant.
  labelPrompt = lv_label_create(container, nullptr);
  SetGreen(labelPrompt);
  lv_label_set_text_static(labelPrompt, "> READY_");

  lv_obj_align(container, nullptr, LV_ALIGN_IN_TOP_LEFT, 4, 4);

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
    uint8_t second = dateTimeController.Seconds();

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

    // Curseur de l'invite : clignote une seconde sur deux.
    lv_label_set_text_static(labelPrompt, (second % 2 == 0) ? "> READY_" : "> READY");

    currentDate = std::chrono::time_point_cast<std::chrono::days>(currentDateTime.Get());
    if (currentDate.IsUpdated()) {
      uint16_t year = dateTimeController.Year();
      Controllers::DateTime::Months month = dateTimeController.Month();
      uint8_t day = dateTimeController.Day();
      // Année réelle (pas 2037).
      lv_label_set_text_fmt(labelDate, "DATE. %04d-%02d-%02d", year, month, day);
    }
  }

  powerPresent = batteryController.IsPowerPresent();
  batteryPercentRemaining = batteryController.PercentRemaining();
  if (batteryPercentRemaining.IsUpdated() || powerPresent.IsUpdated()) {
    if (batteryController.IsCharging()) {
      lv_label_set_text_fmt(batteryValue, "POWR. %d%% CHG", batteryPercentRemaining.Get());
    } else {
      lv_label_set_text_fmt(batteryValue, "POWR. %d%%", batteryPercentRemaining.Get());
    }
  }

  currentWeather = weatherService.Current();
  if (currentWeather.IsUpdated()) {
    auto optCurrentWeather = currentWeather.Get();
    if (optCurrentWeather) {
      int16_t temp = optCurrentWeather->temperature.Celsius();
      char tempUnit = 'C';
      if (settingsController.GetWeatherFormat() == Controllers::Settings::WeatherFormat::Imperial) {
        temp = optCurrentWeather->temperature.Fahrenheit();
        tempUnit = 'F';
      }
      lv_label_set_text_fmt(weather, "TEMP. %d%c %s", temp, tempUnit, Symbols::GetSimpleCondition(optCurrentWeather->iconId));
    } else {
      lv_label_set_text_static(weather, "TEMP. ---");
    }
  }

  bleState = bleController.IsConnected();
  bleRadioEnabled = bleController.IsRadioEnabled();
  if (bleState.IsUpdated() || bleRadioEnabled.IsUpdated()) {
    if (!bleRadioEnabled.Get()) {
      lv_label_set_text_static(connectState, "LINK. OFFLINE");
    } else if (bleState.Get()) {
      lv_label_set_text_static(connectState, "LINK. ONLINE");
    } else {
      lv_label_set_text_static(connectState, "LINK. NO SIGNAL");
    }
  }
}
