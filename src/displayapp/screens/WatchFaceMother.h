#pragma once

#include <lvgl/src/lv_core/lv_obj.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include <displayapp/Controllers.h>
#include "displayapp/screens/Screen.h"
#include "components/datetime/DateTimeController.h"
#include "components/ble/SimpleWeatherService.h"
#include "utility/DirtyValue.h"

namespace Pinetime {
  namespace Controllers {
    class Settings;
    class Battery;
    class Ble;
  }

  namespace Applications {
    namespace Screens {

      // Cadran inspiré de l'interface MU/TH/UR (« Mother ») du Nostromo (film Alien).
      // Esthétique terminal CRT : tout en vert vif sur fond noir, mise en page centrée.
      class WatchFaceMother : public Screen {
      public:
        WatchFaceMother(Controllers::DateTime& dateTimeController,
                        const Controllers::Battery& batteryController,
                        const Controllers::Ble& bleController,
                        Controllers::Settings& settingsController,
                        Controllers::SimpleWeatherService& weatherService);
        ~WatchFaceMother() override;

        void Refresh() override;

      private:
        Utility::DirtyValue<int> batteryPercentRemaining {};
        Utility::DirtyValue<bool> powerPresent {};
        Utility::DirtyValue<bool> bleState {};
        Utility::DirtyValue<bool> bleRadioEnabled {};
        Utility::DirtyValue<std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>> currentDateTime {};
        Utility::DirtyValue<std::chrono::time_point<std::chrono::system_clock, std::chrono::days>> currentDate;
        Utility::DirtyValue<std::optional<Controllers::SimpleWeatherService::CurrentWeather>> currentWeather {};

        lv_obj_t* labelHeader;
        lv_obj_t* labelLogo;
        lv_obj_t* labelTime;
        lv_obj_t* labelShip;
        lv_obj_t* labelMfr;
        lv_obj_t* labelMdl;
        lv_obj_t* labelCls;
        lv_obj_t* labelAffil;
        lv_obj_t* batteryWeather;
        lv_obj_t* connectState;
        lv_obj_t* labelDate;

        Controllers::DateTime& dateTimeController;
        const Controllers::Battery& batteryController;
        const Controllers::Ble& bleController;
        Controllers::Settings& settingsController;
        Controllers::SimpleWeatherService& weatherService;

        lv_task_t* taskRefresh;
      };
    }

    template <>
    struct WatchFaceTraits<WatchFace::Mother> {
      static constexpr WatchFace watchFace = WatchFace::Mother;
      static constexpr const char* name = "MU/TH/UR";

      static Screens::Screen* Create(AppControllers& controllers) {
        return new Screens::WatchFaceMother(controllers.dateTimeController,
                                            controllers.batteryController,
                                            controllers.bleController,
                                            controllers.settingsController,
                                            *controllers.weatherController);
      };

      static bool IsAvailable(Pinetime::Controllers::FS& /*filesystem*/) {
        return true;
      }
    };
  }
}
