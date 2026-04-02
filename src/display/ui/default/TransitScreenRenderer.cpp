#include "TransitScreenRenderer.h"

#include <algorithm>
#include <cstdlib>

namespace {
constexpr int TRANSIT_STYLE_ROUNDEL = 0;
constexpr int TRANSIT_STYLE_BOARD = 1;

constexpr uint32_t TRANSIT_BLACK_HEX = 0x0A0806;
constexpr uint32_t TRANSIT_WHITE_HEX = 0xF3E8D2;
constexpr uint32_t TRANSIT_IVORY_HEX = 0xE7D6B8;
constexpr uint32_t TRANSIT_PANEL_HEX = 0x15100D;
constexpr uint32_t TRANSIT_ROW_HEX = 0x100C0A;
constexpr uint32_t TRANSIT_WINDOW_HEX = 0x090706;
constexpr uint32_t TRANSIT_BUS_ROW_HEX = 0x3C0F11;
constexpr uint32_t TRANSIT_BUS_WINDOW_HEX = 0x24080A;
constexpr uint32_t TRANSIT_BUS_RED_HEX = 0xB23A2C;
constexpr uint32_t TRANSIT_BOARD_AMBER_HEX = 0xF3B33D;
constexpr uint32_t TRANSIT_BOARD_BORDER_HEX = 0xA8741A;
constexpr uint32_t TRANSIT_BOARD_PANEL_HEX = 0x000000;
constexpr uint32_t TRANSIT_BOARD_ROW_HEX = 0x000000;
constexpr uint32_t TRANSIT_BOARD_GLOW_HEX = 0x2A1900;
constexpr int TRANSIT_MIN_PANEL_WIDTH = 334;
constexpr int TRANSIT_MAX_PANEL_WIDTH = 404;
constexpr int TRANSIT_MAX_CONTENT_WIDTH = 360;
constexpr int TRANSIT_TUBE_SIDE_MARGIN = 7;
constexpr int TRANSIT_PANEL_SIDE_MARGIN = 31;
constexpr int TRANSIT_CONTENT_SIDE_INSET = 22;
constexpr int TRANSIT_TUBE_SCROLL_GAP = 28;
constexpr int TRANSIT_TUBE_SCROLL_SPEED_PX_PER_SEC = 28;
constexpr int TRANSIT_TUBE_ROW_HEIGHT = 38;
constexpr int TRANSIT_BUS_ROW_HEIGHT = 46;
constexpr int TRANSIT_BUS_CHIP_WIDTH = 68;
constexpr int TRANSIT_CLOCK_WIDTH = 292;
constexpr int TRANSIT_CLOCK_Y = -124;
constexpr int TRANSIT_CLOCK_GAP = 6;
constexpr int TRANSIT_ROW_BORDER_WIDTH = 2;
constexpr int TRANSIT_WINDOW_BORDER_WIDTH = 2;

struct TransitTubeRow {
    String name;
    String status;
    String colorHex;
};

struct TransitBusRow {
    String route;
    String destination;
    String firstEta;
    String secondEta;
};

std::vector<TransitTubeRow> parseTransitTubeRows(const String &payload) {
    std::vector<TransitTubeRow> rows;
    int start = 0;
    while (start < (int)payload.length()) {
        int lineEnd = payload.indexOf('\n', start);
        String line = lineEnd >= 0 ? payload.substring(start, lineEnd) : payload.substring(start);
        line.trim();
        if (!line.isEmpty()) {
            int first = line.indexOf('|');
            int second = first >= 0 ? line.indexOf('|', first + 1) : -1;
            if (first >= 0 && second >= 0) {
                TransitTubeRow row{};
                row.name = line.substring(0, first);
                row.status = line.substring(first + 1, second);
                row.colorHex = line.substring(second + 1);
                rows.push_back(row);
            }
        }
        if (lineEnd < 0) {
            break;
        }
        start = lineEnd + 1;
    }
    return rows;
}

std::vector<TransitBusRow> parseTransitBusRows(const String &payload) {
    std::vector<TransitBusRow> rows;
    int start = 0;
    while (start < (int)payload.length()) {
        int lineEnd = payload.indexOf('\n', start);
        String line = lineEnd >= 0 ? payload.substring(start, lineEnd) : payload.substring(start);
        line.trim();
        if (!line.isEmpty()) {
            int first = line.indexOf('|');
            int second = first >= 0 ? line.indexOf('|', first + 1) : -1;
            int third = second >= 0 ? line.indexOf('|', second + 1) : -1;
            if (first >= 0 && second >= 0) {
                TransitBusRow row{};
                row.route = line.substring(0, first);
                row.destination = line.substring(first + 1, second);
                if (third >= 0) {
                    row.firstEta = line.substring(second + 1, third);
                    row.secondEta = line.substring(third + 1);
                } else {
                    row.firstEta = line.substring(second + 1);
                }
                rows.push_back(row);
            }
        }
        if (lineEnd < 0) {
            break;
        }
        start = lineEnd + 1;
    }
    return rows;
}

lv_color_t parseTransitColor(const String &hex) {
    return lv_color_hex(static_cast<uint32_t>(strtoul(hex.c_str(), nullptr, 16)));
}

lv_color_t transitIvory() { return lv_color_hex(TRANSIT_IVORY_HEX); }
lv_color_t transitAmber() { return lv_color_hex(TRANSIT_BOARD_AMBER_HEX); }
lv_color_t transitWhite() { return lv_color_hex(TRANSIT_WHITE_HEX); }
lv_color_t transitBorder(int style) {
    return style == TRANSIT_STYLE_BOARD ? lv_color_hex(TRANSIT_BOARD_BORDER_HEX) : transitIvory();
}
lv_color_t transitRowBorderColor(int style, const String &colorHex) {
    return style == TRANSIT_STYLE_BOARD ? parseTransitColor(colorHex) : transitBorder(style);
}
lv_color_t transitBusBorderColor(int style) {
    return style == TRANSIT_STYLE_BOARD ? lv_color_hex(TRANSIT_BUS_RED_HEX) : transitBorder(style);
}
lv_color_t transitText(int style) { return style == TRANSIT_STYLE_BOARD ? transitWhite() : transitIvory(); }
lv_color_t transitPanel(int style) {
    return lv_color_hex(style == TRANSIT_STYLE_BOARD ? TRANSIT_BOARD_PANEL_HEX : TRANSIT_PANEL_HEX);
}
lv_color_t transitRowBackground(int style) {
    return lv_color_hex(style == TRANSIT_STYLE_BOARD ? TRANSIT_BOARD_ROW_HEX : TRANSIT_ROW_HEX);
}
lv_color_t transitBusRowBackground(int style) {
    return lv_color_hex(style == TRANSIT_STYLE_BOARD ? TRANSIT_BOARD_ROW_HEX : TRANSIT_BUS_ROW_HEX);
}
lv_color_t transitBusChipBackground(int style) {
    return lv_color_hex(style == TRANSIT_STYLE_BOARD ? TRANSIT_BOARD_ROW_HEX : TRANSIT_BUS_RED_HEX);
}
lv_color_t transitWindowBackground(int style) {
    return lv_color_hex(style == TRANSIT_STYLE_BOARD ? TRANSIT_BOARD_ROW_HEX : TRANSIT_WINDOW_HEX);
}
lv_color_t transitBusWindowBackground(int style) {
    return lv_color_hex(style == TRANSIT_STYLE_BOARD ? TRANSIT_BOARD_ROW_HEX : TRANSIT_BUS_WINDOW_HEX);
}
lv_color_t transitChipTextColor(const String &colorHex, int style) {
    if (style == TRANSIT_STYLE_BOARD) {
        return (colorHex == "FFD300" || colorHex == "95CDBA") ? lv_color_hex(0x111111) : lv_color_hex(0xF5ECD6);
    }
    return (colorHex == "FFD300" || colorHex == "95CDBA") ? lv_color_hex(0x111316) : transitIvory();
}
lv_font_t withFallbackCopy(const lv_font_t &font, const lv_font_t &fallback) {
    lv_font_t copy = font;
    copy.fallback = &fallback;
    return copy;
}
const lv_font_t *transitHeaderFont(int style) {
    (void)style;
    static const lv_font_t font = withFallbackCopy(transit_font_light_14, lv_font_montserrat_14);
    return &font;
}
const lv_font_t *transitRowFont(int style) {
    (void)style;
    static const lv_font_t font = withFallbackCopy(transit_font_20, lv_font_montserrat_20);
    return &font;
}
const lv_font_t *transitStatusFont(int style) {
    (void)style;
    static const lv_font_t font = withFallbackCopy(transit_font_20, lv_font_montserrat_20);
    return &font;
}
const lv_font_t *transitBusDestinationFont(int style) {
    (void)style;
    static const lv_font_t font = withFallbackCopy(transit_font_light_18, lv_font_montserrat_18);
    return &font;
}
const lv_font_t *transitBusTimeFont(int style) {
    (void)style;
    static const lv_font_t font = withFallbackCopy(transit_font_light_20, lv_font_montserrat_20);
    return &font;
}
const lv_font_t *transitTimeFont(int style) {
    (void)style;
    return &transit_font_40;
}
void setTransitAnimX(void *object, int32_t value) {
    lv_obj_set_x(static_cast<lv_obj_t *>(object), value);
}
int32_t getTransitAnimX(lv_anim_t *animation) {
    return lv_obj_get_x(static_cast<lv_obj_t *>(animation->var));
}
} // namespace

void TransitScreenRenderer::onTransitUpdate(int enabled, int loading, const String &tube, const String &bus,
                                            const String &footer, const String &error) {
    enabled_ = enabled;
    loading_ = loading;
    tubeSummary_ = tube;
    busSummary_ = bus;
    footer_ = footer;
    error_ = error;
}

void TransitScreenRenderer::setStyle(int style) { style_ = style; }

bool TransitScreenRenderer::isEnabled() const { return enabled_; }

uint32_t TransitScreenRenderer::backgroundColorHex() const { return TRANSIT_BLACK_HEX; }

lv_obj_t *TransitScreenRenderer::getClockPanel() const { return clockPanel_; }

bool TransitScreenRenderer::updateClock(const char *hours, const char *minutes) {
    if (clockPanel_ == nullptr) {
        return false;
    }
    if (clockHours_ != nullptr) {
        lv_label_set_text(clockHours_, hours);
    }
    if (clockMinutes_ != nullptr) {
        lv_label_set_text(clockMinutes_, minutes);
    }
    lv_obj_clear_flag(clockPanel_, LV_OBJ_FLAG_HIDDEN);
    return true;
}

void TransitScreenRenderer::hideClock() {
    if (clockPanel_ != nullptr) {
        lv_obj_add_flag(clockPanel_, LV_OBJ_FLAG_HIDDEN);
    }
}

void TransitScreenRenderer::ensureWidgets(lv_obj_t *screen) {
    if (panel_ != nullptr || screen == nullptr) {
        return;
    }

    renderedStyle_ = style_;

    const int screenWidth = lv_obj_get_width(screen);
    const int screenHeight = lv_obj_get_height(screen);
    const int screenDiameter = std::min(screenWidth, screenHeight);
    const int transitLowerPanelWidth =
        std::min(TRANSIT_MAX_PANEL_WIDTH, std::max(TRANSIT_MIN_PANEL_WIDTH, screenWidth - (TRANSIT_PANEL_SIDE_MARGIN * 2)));
    const int transitTubeWidth = std::max(transitLowerPanelWidth, std::max(0, screenDiameter - (TRANSIT_TUBE_SIDE_MARGIN * 2)));
    const int transitContentWidth =
        std::min(TRANSIT_MAX_CONTENT_WIDTH, transitLowerPanelWidth - (TRANSIT_CONTENT_SIDE_INSET * 2));

    clockPanel_ = lv_obj_create(screen);
    lv_obj_remove_style_all(clockPanel_);
    lv_obj_set_size(clockPanel_, TRANSIT_CLOCK_WIDTH, LV_SIZE_CONTENT);
    lv_obj_align(clockPanel_, LV_ALIGN_CENTER, 0, TRANSIT_CLOCK_Y);
    lv_obj_set_flex_flow(clockPanel_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(clockPanel_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_all(clockPanel_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(clockPanel_, TRANSIT_CLOCK_GAP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(clockPanel_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    clockHours_ = lv_label_create(clockPanel_);
    lv_label_set_text(clockHours_, "00");
    lv_obj_set_style_text_font(clockHours_, transitTimeFont(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(clockHours_, transitText(style_), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *colon = lv_label_create(clockPanel_);
    lv_label_set_text(colon, ":");
    lv_obj_set_style_text_font(colon, transitTimeFont(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(colon, transitText(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(colon, LV_OPA_80, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_y(colon, -3);

    clockMinutes_ = lv_label_create(clockPanel_);
    lv_label_set_text(clockMinutes_, "00");
    lv_obj_set_style_text_font(clockMinutes_, transitTimeFont(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(clockMinutes_, transitText(style_), LV_PART_MAIN | LV_STATE_DEFAULT);

    panel_ = lv_obj_create(screen);
    lv_obj_remove_style_all(panel_);
    lv_obj_set_width(panel_, transitTubeWidth);
    lv_obj_set_height(panel_, LV_SIZE_CONTENT);
    lv_obj_align(panel_, LV_ALIGN_CENTER, 0, 18);
    lv_obj_set_flex_flow(panel_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(panel_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(panel_, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(panel_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    tubeHeader_ = lv_label_create(panel_);
    lv_obj_set_width(tubeHeader_, transitTubeWidth);
    lv_obj_set_style_text_font(tubeHeader_, transitHeaderFont(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(tubeHeader_, transitText(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(tubeHeader_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(tubeHeader_, LV_OPA_70, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(tubeHeader_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(tubeHeader_, "UNDERGROUND");

    tubeRows_ = lv_obj_create(panel_);
    lv_obj_remove_style_all(tubeRows_);
    lv_obj_set_width(tubeRows_, transitTubeWidth);
    lv_obj_set_height(tubeRows_, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(tubeRows_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tubeRows_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(tubeRows_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(tubeRows_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *lowerContent = lv_obj_create(panel_);
    lv_obj_remove_style_all(lowerContent);
    lv_obj_set_width(lowerContent, transitContentWidth);
    lv_obj_set_height(lowerContent, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(lowerContent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(lowerContent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(lowerContent, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(lowerContent, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(lowerContent, transitPanel(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(lowerContent, LV_OPA_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(lowerContent, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    busHeader_ = lv_label_create(lowerContent);
    lv_obj_set_width(busHeader_, transitContentWidth);
    lv_obj_set_style_text_font(busHeader_, transitHeaderFont(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(busHeader_, transitText(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(busHeader_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(busHeader_, LV_OPA_70, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(busHeader_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(busHeader_, "BUSES");

    busRows_ = lv_obj_create(lowerContent);
    lv_obj_remove_style_all(busRows_);
    lv_obj_set_width(busRows_, transitContentWidth);
    lv_obj_set_height(busRows_, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(busRows_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(busRows_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(busRows_, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(busRows_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    footerLabel_ = lv_label_create(lowerContent);
    lv_obj_set_width(footerLabel_, transitContentWidth);
    lv_obj_set_style_text_font(footerLabel_, transitHeaderFont(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(footerLabel_, transitText(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(footerLabel_, LV_OPA_70, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(footerLabel_, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(footerLabel_, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_long_mode(footerLabel_, LV_LABEL_LONG_WRAP);
}

void TransitScreenRenderer::resetWidgets() {
    if (clockPanel_ != nullptr && lv_obj_is_valid(clockPanel_)) {
        lv_obj_del(clockPanel_);
    }
    clockPanel_ = nullptr;
    clockHours_ = nullptr;
    clockMinutes_ = nullptr;
    if (panel_ != nullptr && lv_obj_is_valid(panel_)) {
        lv_obj_del(panel_);
    }
    panel_ = nullptr;
    tubeHeader_ = nullptr;
    tubeRows_ = nullptr;
    busHeader_ = nullptr;
    busRows_ = nullptr;
    footerLabel_ = nullptr;
    renderedStyle_ = -1;
    renderedTubeSummary_ = "";
    renderedBusSummary_ = "";
    renderedFooter_ = "";
    renderedError_ = "";
    renderedLoading_ = -1;
}

bool TransitScreenRenderer::update(lv_obj_t *screen, lv_obj_t *logo, lv_obj_t *statusContainer) {
    if (!enabled_) {
        resetWidgets();
        lv_obj_clear_flag(logo, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(statusContainer, LV_OBJ_FLAG_HIDDEN);
        return false;
    }

    if (panel_ != nullptr && renderedStyle_ != style_) {
        resetWidgets();
    }
    ensureWidgets(screen);
    lv_obj_add_flag(logo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(statusContainer, LV_OBJ_FLAG_HIDDEN);

    const bool hasTube = tubeSummary_.length() > 0;
    const bool hasBus = busSummary_.length() > 0;
    const bool styleChanged = renderedStyle_ != style_;
    const bool tubeChanged = styleChanged || renderedTubeSummary_ != tubeSummary_;
    const bool busChanged = styleChanged || renderedBusSummary_ != busSummary_;
    const bool footerChanged = styleChanged || renderedFooter_ != footer_ ||
                               renderedError_ != error_ || renderedLoading_ != loading_;

    if (tubeHeader_ != nullptr) {
        hasTube ? lv_obj_clear_flag(tubeHeader_, LV_OBJ_FLAG_HIDDEN)
                : lv_obj_add_flag(tubeHeader_, LV_OBJ_FLAG_HIDDEN);
    }
    if (tubeRows_ != nullptr && tubeChanged) {
        lv_obj_clean(tubeRows_);
        if (hasTube) {
            const auto rows = parseTransitTubeRows(tubeSummary_);
            if (!rows.empty()) {
                lv_obj_t *viewport = lv_obj_create(tubeRows_);
                lv_obj_remove_style_all(viewport);
                lv_obj_set_width(viewport, lv_pct(100));
                lv_obj_set_height(viewport, LV_SIZE_CONTENT);
                lv_obj_set_style_pad_all(viewport, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_clear_flag(viewport, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

                lv_obj_t *track = lv_obj_create(viewport);
                lv_obj_remove_style_all(track);
                lv_obj_set_width(track, LV_SIZE_CONTENT);
                lv_obj_set_height(track, LV_SIZE_CONTENT);
                lv_obj_set_flex_flow(track, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(track, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                lv_obj_set_style_pad_column(track, TRANSIT_TUBE_SCROLL_GAP, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_pad_all(track, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

                auto createBannerSequence = [&](lv_obj_t *parent) {
                    lv_obj_t *sequence = lv_obj_create(parent);
                    lv_obj_remove_style_all(sequence);
                    lv_obj_set_width(sequence, LV_SIZE_CONTENT);
                    lv_obj_set_height(sequence, LV_SIZE_CONTENT);
                    lv_obj_set_flex_flow(sequence, LV_FLEX_FLOW_ROW);
                    lv_obj_set_flex_align(sequence, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                    lv_obj_set_style_pad_column(sequence, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_all(sequence, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_clear_flag(sequence, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

                    for (const auto &row : rows) {
                        lv_obj_t *lineRow = lv_obj_create(sequence);
                        lv_obj_remove_style_all(lineRow);
                        lv_obj_set_width(lineRow, LV_SIZE_CONTENT);
                        lv_obj_set_height(lineRow, TRANSIT_TUBE_ROW_HEIGHT);
                        lv_obj_set_flex_flow(lineRow, LV_FLEX_FLOW_ROW);
                        lv_obj_set_flex_align(lineRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                        lv_obj_set_style_pad_column(lineRow, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_pad_left(lineRow, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_pad_right(lineRow, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_pad_top(lineRow, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_pad_bottom(lineRow, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_border_width(lineRow, TRANSIT_ROW_BORDER_WIDTH, LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_border_color(lineRow, transitRowBorderColor(style_, row.colorHex),
                                                     LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_border_opa(lineRow, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_radius(lineRow, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(lineRow, transitRowBackground(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_opa(lineRow, style_ == TRANSIT_STYLE_BOARD ? LV_OPA_40 : LV_OPA_20,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                        if (style_ == TRANSIT_STYLE_BOARD) {
                            lv_obj_set_style_shadow_color(lineRow, lv_color_hex(TRANSIT_BOARD_GLOW_HEX),
                                                          LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_shadow_width(lineRow, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_shadow_spread(lineRow, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_shadow_opa(lineRow, LV_OPA_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        lv_obj_clear_flag(lineRow, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

                        lv_obj_t *chipBox = lv_obj_create(lineRow);
                        lv_obj_remove_style_all(chipBox);
                        lv_obj_set_height(chipBox, lv_pct(100));
                        lv_obj_set_width(chipBox, LV_SIZE_CONTENT);
                        lv_obj_set_flex_flow(chipBox, LV_FLEX_FLOW_ROW);
                        lv_obj_set_flex_align(chipBox, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                        lv_obj_set_style_bg_color(chipBox, parseTransitColor(row.colorHex), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_opa(chipBox, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_radius(chipBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_pad_left(chipBox, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_pad_right(chipBox, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_pad_top(chipBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_pad_bottom(chipBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_clear_flag(chipBox, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

                        lv_obj_t *chip = lv_label_create(chipBox);
                        lv_label_set_text(chip, row.name.c_str());
                        lv_obj_set_style_text_font(chip, transitRowFont(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_text_color(chip, transitChipTextColor(row.colorHex, style_),
                                                    LV_PART_MAIN | LV_STATE_DEFAULT);

                        lv_obj_t *status = lv_label_create(lineRow);
                        lv_label_set_text(status, row.status.c_str());
                        lv_obj_set_style_text_font(status, transitStatusFont(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_text_color(status, transitText(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_pad_top(status, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_pad_bottom(status, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }

                    return sequence;
                };

                lv_obj_t *sequence = createBannerSequence(track);
                lv_obj_t *duplicate = createBannerSequence(track);
                lv_obj_update_layout(viewport);
                lv_obj_update_layout(sequence);
                lv_obj_update_layout(duplicate);
                lv_obj_update_layout(track);

                const int viewportWidth = lv_obj_get_content_width(viewport);
                const int sequenceWidth = lv_obj_get_width(sequence);
                const int scrollDistance = sequenceWidth + TRANSIT_TUBE_SCROLL_GAP;

                if (sequenceWidth <= viewportWidth) {
                    lv_obj_add_flag(duplicate, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_width(track, sequenceWidth);
                    lv_obj_align(track, LV_ALIGN_TOP_MID, 0, 0);
                } else {
                    lv_obj_set_x(track, 0);
                    lv_anim_t animation;
                    lv_anim_init(&animation);
                    lv_anim_set_var(&animation, track);
                    lv_anim_set_exec_cb(&animation, setTransitAnimX);
                    lv_anim_set_get_value_cb(&animation, getTransitAnimX);
                    lv_anim_set_values(&animation, 0, -scrollDistance);
                    lv_anim_set_time(&animation, std::max(8000, (scrollDistance * 1000) / TRANSIT_TUBE_SCROLL_SPEED_PX_PER_SEC));
                    lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
                    lv_anim_start(&animation);
                }
            }
            lv_obj_clear_flag(tubeRows_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(tubeRows_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (busHeader_ != nullptr) {
        hasBus ? lv_obj_clear_flag(busHeader_, LV_OBJ_FLAG_HIDDEN)
               : lv_obj_add_flag(busHeader_, LV_OBJ_FLAG_HIDDEN);
    }
    if (busRows_ != nullptr && busChanged) {
        lv_obj_clean(busRows_);
        if (hasBus) {
            const auto rows = parseTransitBusRows(busSummary_);
            for (const auto &row : rows) {
                const lv_color_t busBorderColor = transitBusBorderColor(style_);
                lv_obj_t *busRow = lv_obj_create(busRows_);
                lv_obj_remove_style_all(busRow);
                lv_obj_set_width(busRow, lv_pct(100));
                lv_obj_set_height(busRow, TRANSIT_BUS_ROW_HEIGHT);
                lv_obj_set_flex_flow(busRow, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(busRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                lv_obj_set_style_pad_column(busRow, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_pad_left(busRow, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_pad_right(busRow, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_pad_top(busRow, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_pad_bottom(busRow, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_width(busRow, TRANSIT_ROW_BORDER_WIDTH, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_color(busRow, busBorderColor, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_opa(busRow, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_radius(busRow, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(busRow, transitBusRowBackground(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(busRow, style_ == TRANSIT_STYLE_BOARD ? LV_OPA_40 : LV_OPA_20,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
                if (style_ == TRANSIT_STYLE_BOARD) {
                    lv_obj_set_style_shadow_width(busRow, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_opa(busRow, LV_OPA_0, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                lv_obj_clear_flag(busRow, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

                lv_obj_t *chipBox = lv_obj_create(busRow);
                lv_obj_remove_style_all(chipBox);
                lv_obj_set_height(chipBox, lv_pct(100));
                lv_obj_set_width(chipBox, TRANSIT_BUS_CHIP_WIDTH);
                lv_obj_set_flex_flow(chipBox, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(chipBox, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                lv_obj_set_style_bg_color(chipBox, transitBusChipBackground(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(chipBox, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_radius(chipBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_pad_left(chipBox, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_pad_right(chipBox, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_pad_top(chipBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_pad_bottom(chipBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_clear_flag(chipBox, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

                lv_obj_t *chip = lv_label_create(chipBox);
                lv_label_set_text(chip, row.route.c_str());
                lv_obj_set_style_text_font(chip, transitRowFont(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(chip, transitText(style_), LV_PART_MAIN | LV_STATE_DEFAULT);

                lv_obj_t *detail = lv_label_create(busRow);
                String text = row.destination;
                if (!text.isEmpty()) {
                    text = "to " + text;
                }
                lv_label_set_text(detail, text.c_str());
                lv_obj_set_style_text_font(detail, transitBusDestinationFont(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(detail, transitText(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_pad_top(detail, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_pad_bottom(detail, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_flex_grow(detail, 1);
                lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_label_set_long_mode(detail, LV_LABEL_LONG_CLIP);

                lv_obj_t *destinationSeparator = lv_obj_create(busRow);
                lv_obj_remove_style_all(destinationSeparator);
                lv_obj_set_size(destinationSeparator, 1, 20);
                lv_obj_set_style_bg_color(destinationSeparator, busBorderColor, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(destinationSeparator, LV_OPA_60, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_clear_flag(destinationSeparator, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

                lv_obj_t *firstEtaBox = lv_obj_create(busRow);
                lv_obj_remove_style_all(firstEtaBox);
                lv_obj_set_height(firstEtaBox, 28);
                lv_obj_set_width(firstEtaBox, LV_SIZE_CONTENT);
                lv_obj_set_flex_flow(firstEtaBox, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(firstEtaBox, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                lv_obj_set_style_bg_color(firstEtaBox, transitBusWindowBackground(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(firstEtaBox, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_width(firstEtaBox, TRANSIT_WINDOW_BORDER_WIDTH, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_color(firstEtaBox, busBorderColor, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_opa(firstEtaBox, LV_OPA_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_radius(firstEtaBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_pad_left(firstEtaBox, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_pad_right(firstEtaBox, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_pad_top(firstEtaBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_pad_bottom(firstEtaBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_clear_flag(firstEtaBox, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

                lv_obj_t *firstEta = lv_label_create(firstEtaBox);
                lv_label_set_text(firstEta, row.firstEta.c_str());
                lv_obj_set_style_text_font(firstEta, transitBusTimeFont(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(firstEta, transitText(style_), LV_PART_MAIN | LV_STATE_DEFAULT);

                if (!row.secondEta.isEmpty()) {
                    lv_obj_t *separator = lv_obj_create(busRow);
                    lv_obj_remove_style_all(separator);
                    lv_obj_set_size(separator, 1, 18);
                    lv_obj_set_style_bg_color(separator, busBorderColor, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(separator, LV_OPA_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_clear_flag(separator, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

                    lv_obj_t *secondEtaBox = lv_obj_create(busRow);
                    lv_obj_remove_style_all(secondEtaBox);
                    lv_obj_set_height(secondEtaBox, 28);
                    lv_obj_set_width(secondEtaBox, LV_SIZE_CONTENT);
                    lv_obj_set_flex_flow(secondEtaBox, LV_FLEX_FLOW_ROW);
                    lv_obj_set_flex_align(secondEtaBox, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                    lv_obj_set_style_bg_color(secondEtaBox, transitBusWindowBackground(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(secondEtaBox, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(secondEtaBox, TRANSIT_WINDOW_BORDER_WIDTH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(secondEtaBox, busBorderColor, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_opa(secondEtaBox, LV_OPA_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(secondEtaBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_left(secondEtaBox, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(secondEtaBox, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(secondEtaBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(secondEtaBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_clear_flag(secondEtaBox, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

                    lv_obj_t *secondEta = lv_label_create(secondEtaBox);
                    lv_label_set_text(secondEta, row.secondEta.c_str());
                    lv_obj_set_style_text_font(secondEta, transitBusTimeFont(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(secondEta, transitText(style_), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            }
            lv_obj_clear_flag(busRows_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(busRows_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (footerLabel_ != nullptr && footerChanged) {
        String footerText = error_;
        if (footerText.isEmpty()) {
            footerText = footer_;
        }
        if (footerText.isEmpty() && !hasTube && !hasBus) {
            footerText = loading_ ? "Loading TfL..." : "TfL data unavailable";
        }

        if (footerText.isEmpty()) {
            lv_obj_add_flag(footerLabel_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(footerLabel_, footerText.c_str());
            lv_obj_set_style_text_opa(footerLabel_, error_.isEmpty() ? LV_OPA_70 : LV_OPA_COVER,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(footerLabel_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    renderedStyle_ = style_;
    renderedTubeSummary_ = tubeSummary_;
    renderedBusSummary_ = busSummary_;
    renderedFooter_ = footer_;
    renderedError_ = error_;
    renderedLoading_ = loading_;
    return true;
}
