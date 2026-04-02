#ifndef TRANSITSCREENSAVERPLUGIN_H
#define TRANSITSCREENSAVERPLUGIN_H

#include "../core/Plugin.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

class Controller;
class PluginManager;

class TransitScreensaverPlugin : public Plugin {
  public:
    void setup(Controller *controller, PluginManager *pluginManager) override;
    void loop() override;

  private:
    struct Config {
        struct BusRoute {
            String route = "";
            String stopIdOverride = "";
            String destinationFilter = "";
            String label = "";
        };

        bool enabled = false;
        String appId = "";
        String appKey = "";
        String busStopId = "";
        std::vector<String> tubeLines;
        std::vector<BusRoute> busRoutes;
    };

    void reloadConfig();
    void fetchAndPublish();
    void publishState(bool enabled, bool loading, const String &tubeSummary, const String &busSummary, const String &footer,
                      const String &error);

    bool fetchLineStatuses(String &summary, String &error) const;
    bool fetchBusArrivals(String &summary, String &error) const;
    bool getJson(const String &url, JsonDocument &doc, String &error) const;

    String appendAuth(const String &url) const;
    String buildUpdatedLabel() const;
    String formatEta(int seconds) const;
    String combineErrors(const String &left, const String &right) const;
    String lineRequestId(const String &lineId) const;
    String lineColorHex(const String &lineId) const;
    static Config::BusRoute parseBusRoute(const String &value);
    static std::vector<String> splitCsv(const String &value);
    static String normalizeToken(const String &value);

    Controller *controller = nullptr;
    PluginManager *pluginManager = nullptr;
    Config config;

    bool apMode = true;
    bool refreshRequested = true;
    bool lastFetchSuccessful = false;
    unsigned long lastFetchAttempt = 0;

    bool publishedEnabled = false;
    bool publishedLoading = false;
    String publishedTubeSummary = "";
    String publishedBusSummary = "";
    String publishedFooter = "";
    String publishedError = "";
    String cachedTubeSummary = "";
    String cachedBusSummary = "";
};

#endif // TRANSITSCREENSAVERPLUGIN_H
