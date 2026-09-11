#include "logic/monitoring.hpp"

#include "logic/state.hpp"
#include "logic/journal.hpp"
#include <glib-unix.h>
#include <libudev.h>
#include <systemd/sd-bus.h>

namespace power_widget {

static gboolean on_udev_ready(gint, GIOCondition condition, gpointer p) {
    auto& current = *static_cast<Logic*>(p);
    if (condition & (G_IO_ERR | G_IO_HUP)) {
        current.udev_source = 0;
        log(current, Kind::System, "Монитор udev отключён. Доступно ручное обновление");
        return G_SOURCE_REMOVE;
    }
    // Сокет libudev неблокирующий. Ограничиваем пакет для отзывчивости GUI.
    for (int count = 0; count < 64; ++count) {
        udev_device* device = udev_monitor_receive_device(current.monitor);
        if (!device) break;
        // Читаем каждый полученный снимок, не откладываем до таймера.
        refresh(current);
        udev_device_unref(device);
    }
    return G_SOURCE_CONTINUE;
}

static int on_prepare_for_sleep(sd_bus_message* message, void* p, sd_bus_error*) {
    auto& current = *static_cast<Logic*>(p);
    int sleeping = 0;
    const int read = sd_bus_message_read(message, "b", &sleeping);
    if (read < 0) return read;
    log(current, Kind::Sleep, sleeping ? "Система готовится ко сну / гибернации" : "Система возобновила работу");
    if (!sleeping) refresh(current);
    return 0;
}

static gboolean on_bus_ready(gint, GIOCondition condition, gpointer p) {
    auto& current = *static_cast<Logic*>(p);
    int status = 0;
    if (!(condition & (G_IO_ERR | G_IO_HUP))) {
        // Полностью опустошаем и внутреннюю очередь sd-bus.
        do { status = sd_bus_process(current.bus, nullptr); } while (status > 0);
    }
    if (status < 0 || (condition & (G_IO_ERR | G_IO_HUP))) {
        current.bus_source = 0;
        log(current, Kind::System, "Подписка на события сна logind потеряна");
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

static void start_udev_monitor(Logic& app) {
    app.context = udev_new();
    if (app.context) app.monitor = udev_monitor_new_from_netlink(app.context, "udev");
    if (app.monitor && udev_monitor_filter_add_match_subsystem_devtype(app.monitor, "power_supply", nullptr) >= 0 &&
        udev_monitor_enable_receiving(app.monitor) >= 0) {
        const int fd = udev_monitor_get_fd(app.monitor);
        if (fd >= 0) app.udev_source = g_unix_fd_add(fd, static_cast<GIOCondition>(G_IO_IN | G_IO_ERR | G_IO_HUP),
            on_udev_ready, &app);
    }
    if (!app.udev_source) log(app, Kind::System, "Не удалось подписаться на udev. Используйте «Обновить»");
}

static void start_sleep_monitor(Logic& app) {
    int result = sd_bus_open_system(&app.bus);
    if (result >= 0) result = sd_bus_add_match(app.bus, &app.sleep_slot,
        "type='signal',sender='org.freedesktop.login1',path='/org/freedesktop/login1',"
        "interface='org.freedesktop.login1.Manager',member='PrepareForSleep'",
        on_prepare_for_sleep, &app);
    if (result >= 0) {
        const int fd = sd_bus_get_fd(app.bus);
        if (fd >= 0) app.bus_source = g_unix_fd_add(fd, static_cast<GIOCondition>(G_IO_IN | G_IO_ERR | G_IO_HUP),
            on_bus_ready, &app);
        // Синхронная регистрация могла уже оставить сигналы во внутренней очереди.
        while (sd_bus_process(app.bus, nullptr) > 0) {}
    }
    if (!app.bus_source) log(app, Kind::System, "События сна logind недоступны; запросы кнопок журналируются");
}

void start_monitors(Logic& app) {
    start_udev_monitor(app);
    start_sleep_monitor(app);
}

void stop_monitors(Logic& app) {
    if (app.udev_source) g_source_remove(app.udev_source);
    if (app.bus_source) g_source_remove(app.bus_source);
    sd_bus_slot_unref(app.sleep_slot);
    sd_bus_unref(app.bus);
    if (app.monitor) udev_monitor_unref(app.monitor);
    if (app.context) udev_unref(app.context);
    // После завершения главного цикла асинхронные callbacks больше не исполняются.
}

} // namespace power_widget
