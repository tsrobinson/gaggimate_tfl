#ifndef TRANSITSCREENRENDERER_H
#define TRANSITSCREENRENDERER_H

#include <Arduino.h>
#include "./lvgl/ui.h"

class TransitScreenRenderer {
  public:
    void onTransitUpdate(int enabled, int loading, const String &tube, const String &bus, const String &footer,
                         const String &error);
    void setStyle(int style);
    bool isEnabled() const;

    void ensureWidgets(lv_obj_t *screen);
    void resetWidgets();

    // Updates transit overlay. Returns true if overlay is active (caller should
    // hide logo/statusContainer and skip icon visibility logic).
    bool update(lv_obj_t *screen, lv_obj_t *logo, lv_obj_t *statusContainer);

    // Clock management. updateClock returns true if it handled the clock display.
    bool updateClock(const char *hours, const char *minutes);
    void hideClock();
    lv_obj_t *getClockPanel() const;

    uint32_t backgroundColorHex() const;

  private:
    // Event state
    int enabled_ = false;
    int loading_ = false;
    int style_ = 0;
    String tubeSummary_;
    String busSummary_;
    String footer_;
    String error_;

    // Rendered-state cache
    String renderedTubeSummary_;
    String renderedBusSummary_;
    String renderedFooter_;
    String renderedError_;
    int renderedLoading_ = -1;
    int renderedStyle_ = -1;

    // LVGL widget pointers
    lv_obj_t *panel_ = nullptr;
    lv_obj_t *clockPanel_ = nullptr;
    lv_obj_t *clockHours_ = nullptr;
    lv_obj_t *clockMinutes_ = nullptr;
    lv_obj_t *tubeHeader_ = nullptr;
    lv_obj_t *tubeRows_ = nullptr;
    lv_obj_t *busHeader_ = nullptr;
    lv_obj_t *busRows_ = nullptr;
    lv_obj_t *footerLabel_ = nullptr;
};

#endif // TRANSITSCREENRENDERER_H
