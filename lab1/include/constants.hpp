#pragma once

// Показания питания и единицы измерения.
inline constexpr double UNKNOWN_READING = -1.0;
inline constexpr int POWER_SOURCE_UNKNOWN = -1;
inline constexpr int POWER_SOURCE_OFFLINE = 0;
inline constexpr int POWER_SOURCE_ONLINE = 1;
inline constexpr int BATTERY_ABSENT = 0;
inline constexpr double EMPTY_CHARGE_PERCENT = 0.0;
inline constexpr double FULL_CHARGE_PERCENT = 100.0;
inline constexpr double MINUTES_PER_HOUR = 60.0;
inline constexpr double HIGH_CHARGE_PERCENT = 50.0;
inline constexpr double LOW_CHARGE_PERCENT = 20.0;
inline constexpr double MAX_DISPLAY_MINUTES = 1e9;

// Статусы батареи, используемые при чтении sysfs.
inline constexpr char BATTERY_STATUS_UNKNOWN[] = "Unknown";
inline constexpr char BATTERY_STATUS_CHARGING[] = "Charging";
inline constexpr char BATTERY_STATUS_DISCHARGING[] = "Discharging";
inline constexpr char BATTERY_STATUS_FULL[] = "Full";
inline constexpr char BATTERY_STATUS_NOT_CHARGING[] = "Not charging";

// Текст для неизвестного числового показания в интерфейсе.
inline constexpr char UNKNOWN_VALUE_TEXT[] = "unknown";

// Системные настройки.
inline constexpr int UDEV_EVENTS_PER_BATCH = 64;
inline constexpr int AUTOSTART_DIRECTORY_MODE = 0700;
inline constexpr int NULL_TERMINATED_TEXT = -1;
inline constexpr unsigned int NO_EVENT_SOURCE = 0;

// Геометрия виджета и подписей (логические пиксели).
inline constexpr int WIDGET_WIDTH = 300;
inline constexpr int WIDGET_HEIGHT = 200;
inline constexpr int LABEL_LEFT = 10;
inline constexpr int LABEL_WIDTH = WIDGET_WIDTH - 2 * LABEL_LEFT;
inline constexpr int CHARGE_LABEL_Y = 48;
inline constexpr int CHARGE_FONT_SIZE = 28;
inline constexpr int BATTERY_LABEL_Y = 83;
inline constexpr int SMALL_FONT_SIZE = 11;
inline constexpr int SOURCE_LABEL_Y = 122;
inline constexpr int SOURCE_FONT_SIZE = 14;
inline constexpr int DETAIL_LABEL_Y = 145;
inline constexpr int DETAIL_FONT_SIZE = 12;
inline constexpr int HINT_LABEL_Y = 178;

// Кольцевой индикатор; углы задаются в радианах.
inline constexpr double PI = 3.14159265358979323846;
inline constexpr double FULL_CIRCLE = 2 * PI;
inline constexpr double RING_START_ANGLE = -PI / 2;
inline constexpr double RING_CENTER_X = WIDGET_WIDTH / 2.0;
inline constexpr double RING_CENTER_Y = 69.0;
inline constexpr double RING_RADIUS = 44.0;
inline constexpr double RING_LINE_WIDTH = 9.0;

struct RgbColor {
    double red;
    double green;
    double blue;
};

inline constexpr RgbColor MUTED_TEXT_COLOR{0.65, 0.71, 0.80};
inline constexpr RgbColor TEXT_COLOR{0.94, 0.96, 1.0};
inline constexpr RgbColor BACKGROUND_COLOR{0.06, 0.09, 0.15};
inline constexpr RgbColor RING_TRACK_COLOR{0.16, 0.21, 0.29};
inline constexpr RgbColor HIGH_CHARGE_COLOR{0.20, 0.83, 0.60};
inline constexpr RgbColor MEDIUM_CHARGE_COLOR{0.98, 0.76, 0.20};
inline constexpr RgbColor LOW_CHARGE_COLOR{0.98, 0.33, 0.36};

// Окно журнала и оформление GTK.
inline constexpr int LOG_WINDOW_WIDTH = 780;
inline constexpr int LOG_WINDOW_HEIGHT = 440;
inline constexpr int LOG_BOX_SPACING = 10;
inline constexpr unsigned int LOG_BORDER_WIDTH = 12;
inline constexpr unsigned int NO_BOX_PADDING = 0;
inline constexpr char GTK_THEME_CSS[] =
    "window, menu, dialog { background-color: #101726; color: #f0f5ff; }"
    "textview text { background-color: #101726; color: #f0f5ff; }"
    "button, combobox button { background-image: none; background-color: #29364a; color: "
    "#f0f5ff; }"
    "button:hover, menuitem:hover { background-color: #374b65; }";
