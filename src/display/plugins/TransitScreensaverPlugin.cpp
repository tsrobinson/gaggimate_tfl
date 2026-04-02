#include "TransitScreensaverPlugin.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <map>

#include <display/core/Controller.h>
#include <display/core/Event.h>

extern const uint8_t x509_crt_imported_bundle_bin_start[] asm("_binary_x509_crt_bundle_start");

namespace {
constexpr unsigned long REFRESH_INTERVAL_MS = 15000;
constexpr unsigned long RETRY_INTERVAL_MS = 15000;
constexpr int HTTP_TIMEOUT_MS = 5000;
constexpr char TFL_LINE_URL_PREFIX[] = "https://api.tfl.gov.uk/Line/";
constexpr char TFL_LINE_URL_SUFFIX[] = "/Status";
constexpr char TFL_STOPPOINT_URL_PREFIX[] = "https://api.tfl.gov.uk/StopPoint/";
constexpr char TFL_STOPPOINT_URL_SUFFIX[] = "/Arrivals";
} // namespace

void TransitScreensaverPlugin::setup(Controller *controller, PluginManager *pluginManager) {
    this->controller = controller;
    this->pluginManager = pluginManager;

    pluginManager->on("settings:changed", [this](const Event &) {
        reloadConfig();
        refreshRequested = true;
    });
    pluginManager->on("controller:wifi:connect", [this](const Event &event) {
        apMode = event.getInt("AP") == 1;
        refreshRequested = true;
    });
    pluginManager->on("controller:wifi:disconnect", [this](const Event &) {
        apMode = true;
        refreshRequested = true;
    });
    pluginManager->on("controller:mode:change", [this](const Event &event) {
        if (event.getInt("value") == MODE_STANDBY) {
            refreshRequested = true;
        }
    });

    reloadConfig();
    publishState(false, false, "", "", "", "");
}

void TransitScreensaverPlugin::loop() {
    if (!config.enabled) {
        publishState(false, false, "", "", "", "");
        return;
    }

    if (controller->getMode() != MODE_STANDBY) {
        return;
    }

    if (apMode || WiFi.status() != WL_CONNECTED) {
        publishState(true, false, cachedTubeSummary, cachedBusSummary, "Waiting for WiFi", "Waiting for WiFi");
        return;
    }

    const unsigned long now = millis();
    const unsigned long interval = lastFetchSuccessful ? REFRESH_INTERVAL_MS : RETRY_INTERVAL_MS;
    if (!refreshRequested && now - lastFetchAttempt < interval) {
        return;
    }

    fetchAndPublish();
}

void TransitScreensaverPlugin::reloadConfig() {
    const Settings &settings = controller->getSettings();

    config.enabled = settings.isTflScreensaverEnabled();
    config.appId = settings.getTflAppId();
    config.appKey = settings.getTflAppKey();
    config.busStopId = settings.getTflBusStopId();
    config.tubeLines = splitCsv(settings.getTflTubeLines());
    config.busRoutes.clear();
    for (const String &value : splitCsv(settings.getTflBusRoutes())) {
        const Config::BusRoute route = parseBusRoute(value);
        if (!route.route.isEmpty()) {
            config.busRoutes.push_back(route);
        }
    }

    bool hasBusConfig = false;
    for (const auto &route : config.busRoutes) {
        if (!route.route.isEmpty() && (!route.stopIdOverride.isEmpty() || !config.busStopId.isEmpty())) {
            hasBusConfig = true;
            break;
        }
    }
    const bool hasTubeConfig = !config.tubeLines.empty();
    config.enabled = config.enabled && (hasTubeConfig || hasBusConfig);

    if (!config.enabled) {
        cachedTubeSummary = "";
        cachedBusSummary = "";
        lastFetchSuccessful = false;
    }
}

void TransitScreensaverPlugin::fetchAndPublish() {
    lastFetchAttempt = millis();
    refreshRequested = false;

    publishState(true, true, cachedTubeSummary, cachedBusSummary, "Loading TfL...", "");

    String tubeSummary = cachedTubeSummary;
    String busSummary = cachedBusSummary;
    String tubeError = "";
    String busError = "";

    const bool tubeOk = fetchLineStatuses(tubeSummary, tubeError);
    const bool busOk = fetchBusArrivals(busSummary, busError);

    if (tubeOk) {
        cachedTubeSummary = tubeSummary;
    }
    if (busOk) {
        cachedBusSummary = busSummary;
    }

    lastFetchSuccessful = tubeOk && busOk;

    String footer = buildUpdatedLabel();
    const String error = combineErrors(tubeError, busError);

    if (!lastFetchSuccessful) {
        if (!error.isEmpty()) {
            footer = error;
        } else {
            footer = "TfL unavailable";
        }
    }

    publishState(true, false, cachedTubeSummary, cachedBusSummary, footer, error);
}

void TransitScreensaverPlugin::publishState(bool enabled, bool loading, const String &tubeSummary, const String &busSummary,
                                            const String &footer, const String &error) {
    if (publishedEnabled == enabled && publishedLoading == loading && publishedTubeSummary == tubeSummary &&
        publishedBusSummary == busSummary && publishedFooter == footer && publishedError == error) {
        return;
    }

    publishedEnabled = enabled;
    publishedLoading = loading;
    publishedTubeSummary = tubeSummary;
    publishedBusSummary = busSummary;
    publishedFooter = footer;
    publishedError = error;

    Event event;
    event.id = "transit:update";
    event.setInt("enabled", enabled ? 1 : 0);
    event.setInt("loading", loading ? 1 : 0);
    event.setString("tube", tubeSummary);
    event.setString("bus", busSummary);
    event.setString("footer", footer);
    event.setString("error", error);
    pluginManager->trigger(event);
}

bool TransitScreensaverPlugin::fetchLineStatuses(String &summary, String &error) const {
    if (config.tubeLines.empty()) {
        summary = "";
        return true;
    }

    String requestedLines = "";
    std::vector<String> requestIds;
    for (const String &configuredLine : config.tubeLines) {
        const String requestId = lineRequestId(configuredLine);
        if (requestId.isEmpty()) {
            continue;
        }
        bool exists = false;
        for (const String &existing : requestIds) {
            if (existing == requestId) {
                exists = true;
                break;
            }
        }
        if (exists) {
            continue;
        }
        requestIds.push_back(requestId);
        if (!requestedLines.isEmpty()) {
            requestedLines += ",";
        }
        requestedLines += requestId;
    }

    if (requestedLines.isEmpty()) {
        error = "Tube lines not found";
        return false;
    }

    JsonDocument doc;
    if (!getJson(String(TFL_LINE_URL_PREFIX) + requestedLines + TFL_LINE_URL_SUFFIX, doc, error)) {
        return false;
    }
    if (!doc.is<JsonArray>()) {
        error = "Tube status unavailable";
        return false;
    }

    struct LineDisplay {
        String id;
        String name;
        String status;
    };

    std::map<std::string, LineDisplay> statuses;
    for (JsonObject lineObj : doc.as<JsonArray>()) {
        const String lineId = lineObj["id"] | "";
        const String lineName = lineObj["name"] | lineId;

        String status = "Unknown";
        JsonArray lineStatuses = lineObj["lineStatuses"].as<JsonArray>();
        if (!lineStatuses.isNull() && lineStatuses.size() > 0) {
            status = lineStatuses[0]["statusSeverityDescription"] | status;
        }

        const LineDisplay entry{
            .id = lineId,
            .name = lineName,
            .status = status,
        };
        statuses[normalizeToken(lineId).c_str()] = entry;
        statuses[normalizeToken(lineName).c_str()] = entry;
    }

    String formatted = "";
    for (const String &configuredLine : config.tubeLines) {
        const std::string key = normalizeToken(configuredLine).c_str();
        if (!statuses.count(key)) {
            continue;
        }
        if (!formatted.isEmpty()) {
            formatted += "\n";
        }
        const LineDisplay &entry = statuses.at(key);
        formatted += entry.name;
        formatted += "|";
        formatted += entry.status;
        formatted += "|";
        formatted += lineColorHex(entry.id);
    }

    if (formatted.isEmpty()) {
        error = "Tube lines not found";
        return false;
    }

    summary = formatted;
    return true;
}

bool TransitScreensaverPlugin::fetchBusArrivals(String &summary, String &error) const {
    if (config.busRoutes.empty()) {
        summary = "";
        return true;
    }

    bool hasAnyStop = false;
    for (const auto &route : config.busRoutes) {
        if (!route.stopIdOverride.isEmpty() || !config.busStopId.isEmpty()) {
            hasAnyStop = true;
            break;
        }
    }
    if (!hasAnyStop) {
        summary = "";
        return false;
    }

    std::vector<std::vector<int>> arrivalsByRoute(config.busRoutes.size());
    std::vector<std::string> normalizedRoutes;
    normalizedRoutes.reserve(config.busRoutes.size());
    for (const auto &route : config.busRoutes) {
        normalizedRoutes.push_back(normalizeToken(route.route).c_str());
    }

    std::map<String, std::vector<size_t>> routeIndexesByStop;
    for (size_t index = 0; index < config.busRoutes.size(); index++) {
        const auto &configuredRoute = config.busRoutes[index];
        const String stopId = configuredRoute.stopIdOverride.isEmpty() ? config.busStopId : configuredRoute.stopIdOverride;
        if (stopId.isEmpty()) {
            continue;
        }
        routeIndexesByStop[stopId].push_back(index);
    }

    bool fetchedAnyStop = false;
    String combinedError = "";

    for (const auto &entry : routeIndexesByStop) {
        JsonDocument doc;
        String stopError = "";
        const String url = String(TFL_STOPPOINT_URL_PREFIX) + entry.first + TFL_STOPPOINT_URL_SUFFIX;
        if (!getJson(url, doc, stopError)) {
            if (!combinedError.isEmpty()) {
                combinedError += " | ";
            }
            combinedError += "Bus stop " + entry.first + ": " + stopError;
            continue;
        }
        if (!doc.is<JsonArray>()) {
            if (!combinedError.isEmpty()) {
                combinedError += " | ";
            }
            combinedError += "Bus stop " + entry.first + ": Bus arrivals unavailable";
            continue;
        }

        fetchedAnyStop = true;

        for (JsonObject prediction : doc.as<JsonArray>()) {
            String route = prediction["lineName"] | "";
            if (route.isEmpty()) {
                route = prediction["lineId"] | "";
            }
            const std::string routeKey = normalizeToken(route).c_str();
            const String destination = prediction["destinationName"] | "";
            const String normalizedDestination = normalizeToken(destination);

            for (size_t index : entry.second) {
                const auto &configuredRoute = config.busRoutes[index];
                if (routeKey != normalizedRoutes[index]) {
                    continue;
                }
                if (!configuredRoute.destinationFilter.isEmpty() &&
                    normalizedDestination.indexOf(normalizeToken(configuredRoute.destinationFilter)) < 0) {
                    continue;
                }
                arrivalsByRoute[index].push_back(prediction["timeToStation"] | 0);
            }
        }
    }

    if (!fetchedAnyStop) {
        error = combinedError.isEmpty() ? "Bus arrivals unavailable" : combinedError;
        return false;
    }

    error = combinedError;
    String formatted = "";
    for (size_t index = 0; index < config.busRoutes.size(); index++) {
        const auto &configuredRoute = config.busRoutes[index];
        auto &arrivals = arrivalsByRoute[index];
        std::sort(arrivals.begin(), arrivals.end());
        if (!formatted.isEmpty()) {
            formatted += "\n";
        }

        formatted += configuredRoute.route;
        formatted += "|";
        formatted += configuredRoute.destinationFilter;
        formatted += "|";

        if (arrivals.empty()) {
            formatted += "--|";
            continue;
        }

        formatted += formatEta(arrivals[0]);
        formatted += "|";
        formatted += arrivals.size() > 1 ? formatEta(arrivals[1]) : "";
    }

    summary = formatted;
    return true;
}

bool TransitScreensaverPlugin::getJson(const String &url, JsonDocument &doc, String &error) const {
    WiFiClientSecure client;
    client.setCACertBundle(x509_crt_imported_bundle_bin_start);

    HTTPClient http;
    // Avoid chunked-transfer parsing issues when feeding the response stream into ArduinoJson.
    http.useHTTP10(true);
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);

    const String authenticatedUrl = appendAuth(url);
    if (!http.begin(client, authenticatedUrl)) {
        error = "TfL connect failed";
        return false;
    }

    const int statusCode = http.GET();
    if (statusCode != HTTP_CODE_OK) {
        if (statusCode < 0) {
            error = String("TfL ") + http.errorToString(statusCode) + " (" + statusCode + ")";
        } else {
            error = String("TfL HTTP ") + statusCode;
        }
        http.end();
        return false;
    }

    const DeserializationError jsonError = deserializeJson(doc, http.getStream());
    http.end();

    if (jsonError) {
        error = String("TfL parse failed: ") + jsonError.c_str();
        return false;
    }

    return true;
}

String TransitScreensaverPlugin::appendAuth(const String &url) const {
    String authenticatedUrl = url;
    if (!config.appId.isEmpty()) {
        authenticatedUrl += authenticatedUrl.indexOf('?') >= 0 ? "&" : "?";
        authenticatedUrl += "app_id=" + config.appId;
    }
    if (!config.appKey.isEmpty()) {
        authenticatedUrl += authenticatedUrl.indexOf('?') >= 0 ? "&" : "?";
        authenticatedUrl += "app_key=" + config.appKey;
    }
    return authenticatedUrl;
}

String TransitScreensaverPlugin::buildUpdatedLabel() const {
    time_t now = time(nullptr);
    if (now <= 0) {
        return "TfL updated";
    }

    struct tm timeinfo {};
    localtime_r(&now, &timeinfo);

    char buffer[16];
    strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);

    return String("Updated ") + buffer;
}

String TransitScreensaverPlugin::formatEta(int seconds) const {
    if (seconds < 60) {
        return "due";
    }
    return String((seconds + 59) / 60) + "m";
}

String TransitScreensaverPlugin::combineErrors(const String &left, const String &right) const {
    if (left.isEmpty()) {
        return right;
    }
    if (right.isEmpty()) {
        return left;
    }
    return left + " | " + right;
}

String TransitScreensaverPlugin::lineRequestId(const String &lineId) const {
    const String normalized = normalizeToken(lineId);
    if (normalized == "bakerloo")
        return "bakerloo";
    if (normalized == "central")
        return "central";
    if (normalized == "circle")
        return "circle";
    if (normalized == "district")
        return "district";
    if (normalized == "hammersmithcity")
        return "hammersmith-city";
    if (normalized == "jubilee")
        return "jubilee";
    if (normalized == "metropolitan")
        return "metropolitan";
    if (normalized == "northern")
        return "northern";
    if (normalized == "piccadilly")
        return "piccadilly";
    if (normalized == "victoria")
        return "victoria";
    if (normalized == "waterloocity")
        return "waterloo-city";
    if (normalized == "elizabeth")
        return "elizabeth";
    return "";
}

String TransitScreensaverPlugin::lineColorHex(const String &lineId) const {
    const String normalized = normalizeToken(lineId);
    if (normalized == "bakerloo")
        return "B36305";
    if (normalized == "central")
        return "E32017";
    if (normalized == "circle")
        return "FFD300";
    if (normalized == "district")
        return "00782A";
    if (normalized == "hammersmithcity")
        return "F3A9BB";
    if (normalized == "jubilee")
        return "A0A5A9";
    if (normalized == "metropolitan")
        return "9B0056";
    if (normalized == "northern")
        return "000000";
    if (normalized == "piccadilly")
        return "003688";
    if (normalized == "victoria")
        return "0098D4";
    if (normalized == "waterloocity")
        return "95CDBA";
    if (normalized == "elizabeth")
        return "6950A1";
    return "4C5866";
}

TransitScreensaverPlugin::Config::BusRoute TransitScreensaverPlugin::parseBusRoute(const String &value) {
    Config::BusRoute busRoute{};
    String token = value;
    token.trim();
    if (token.isEmpty()) {
        return busRoute;
    }

    const int destinationSplit = token.indexOf('>');
    String routeToken = destinationSplit >= 0 ? token.substring(0, destinationSplit) : token;
    routeToken.trim();
    if (destinationSplit >= 0) {
        busRoute.destinationFilter = token.substring(destinationSplit + 1);
        busRoute.destinationFilter.trim();
    }

    const int stopSplit = routeToken.indexOf('@');
    if (stopSplit >= 0) {
        busRoute.route = routeToken.substring(0, stopSplit);
        busRoute.stopIdOverride = routeToken.substring(stopSplit + 1);
        busRoute.stopIdOverride.trim();
    } else {
        busRoute.route = routeToken;
    }
    busRoute.route.trim();

    if (busRoute.route.isEmpty()) {
        return Config::BusRoute{};
    }

    busRoute.label = busRoute.route;
    if (!busRoute.destinationFilter.isEmpty()) {
        busRoute.label += " to ";
        busRoute.label += busRoute.destinationFilter;
    }
    return busRoute;
}

std::vector<String> TransitScreensaverPlugin::splitCsv(const String &value) {
    std::vector<String> tokens;
    int start = 0;
    while (start <= value.length()) {
        const int comma = value.indexOf(',', start);
        String token = comma >= 0 ? value.substring(start, comma) : value.substring(start);
        token.trim();
        if (!token.isEmpty()) {
            tokens.push_back(token);
        }
        if (comma < 0) {
            break;
        }
        start = comma + 1;
    }
    return tokens;
}

String TransitScreensaverPlugin::normalizeToken(const String &value) {
    String normalized = "";
    for (size_t i = 0; i < value.length(); i++) {
        const char current = static_cast<char>(std::tolower(static_cast<unsigned char>(value.charAt(i))));
        if (std::isalnum(static_cast<unsigned char>(current))) {
            normalized += current;
        }
    }
    return normalized;
}
