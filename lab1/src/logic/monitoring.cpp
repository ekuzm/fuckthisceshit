#include "constants.hpp"
#include "logic/monitoring.hpp"

#include "logic/state.hpp"
#include "logic/journal.hpp"
#include <glib-unix.h>
#include <libudev.h>
#include <systemd/sd-bus.h>

// Обрабатывает доступные события устройств питания и перечитывает показания; при обрыве снимает обработчик.
static gboolean on_udev_ready(gint, GIOCondition condition, gpointer p) {
	// GLib передал наш &app как нетипизированный указатель; восстанавливаем тип Logic.
	Logic& current = *static_cast<Logic*>(p);
	// Условия — битовые флаги: | объединяет ошибки, & проверяет наличие хотя бы одной.
	if (condition & (G_IO_ERR | G_IO_HUP)) {
		current.udev_source = NO_EVENT_SOURCE;
		log(current, Kind::System, "udev monitor disconnected. Manual refresh is available");
		return G_SOURCE_REMOVE;
	}
	// Сокет libudev неблокирующий. Ограничиваем пакет для отзывчивости GUI.
	for (int count = 0; count < UDEV_EVENTS_PER_BATCH; ++count) {
		udev_device* device = udev_monitor_receive_device(current.monitor);
		if (!device) {
			break;
		}
		// Читаем каждый полученный снимок, не откладываем до таймера.
		refresh(current);
		udev_device_unref(device);
	}
	// CONTINUE оставляет обработчик в цикле GLib; REMOVE выше удаляет его.
	return G_SOURCE_CONTINUE;
}

// Записывает сигнал logind о подготовке ко сну или возобновлении; после сна обновляет показания.
static int on_prepare_for_sleep(sd_bus_message* message, void* p, sd_bus_error*) {
	Logic& current = *static_cast<Logic*>(p);
	int sleeping = 0;
	// Формат "b" читает логический аргумент сигнала; true — подготовка, false — возобновление.
	const int read = sd_bus_message_read(message, "b", &sleeping);
	if (read < 0) {
		return read;
	}
	if (sleeping) {
		log(current, Kind::Sleep, "System is preparing to suspend / hibernate");
	} else {
		log(current, Kind::Sleep, "System resumed");
		refresh(current);
	}
	return 0;
}

// Передаёт накопленные сообщения D-Bus обработчикам; при ошибке отключает наблюдение за соединением.
static gboolean on_bus_ready(gint, GIOCondition condition, gpointer p) {
	Logic& current = *static_cast<Logic*>(p);
	int status = 0;
	if (!(condition & (G_IO_ERR | G_IO_HUP))) {
		// Полностью опустошаем и внутреннюю очередь sd-bus.
		while (true) {
			// Положительный результат — сообщение обработано, 0 — очередь пуста, отрицательный — ошибка.
			status = sd_bus_process(current.bus, nullptr);
			if (status <= 0) {
				break;
			}
		}
	}
	if (status < 0 || (condition & (G_IO_ERR | G_IO_HUP))) {
		current.bus_source = NO_EVENT_SOURCE;
		log(current, Kind::System, "Lost subscription to logind sleep events");
		return G_SOURCE_REMOVE;
	}
	return G_SOURCE_CONTINUE;
}

// Подписывается на события power_supply и подключает сокет udev к главному циклу GLib.
static void start_udev_monitor(Logic& app) {
	app.context = udev_new();
	if (app.context) {
		app.monitor = udev_monitor_new_from_netlink(app.context, "udev");
	}
	if (app.monitor &&
	    udev_monitor_filter_add_match_subsystem_devtype(app.monitor, "power_supply", nullptr) >=
	        0 &&
	    udev_monitor_enable_receiving(app.monitor) >= 0) {
		const int fd = udev_monitor_get_fd(app.monitor);
		if (fd >= 0) {
			// GLib вызовет on_udev_ready при готовности сокета или ошибке; отдельный поток не нужен.
			app.udev_source = g_unix_fd_add(
			    fd, static_cast<GIOCondition>(G_IO_IN | G_IO_ERR | G_IO_HUP), on_udev_ready, &app);
		}
	}
	if (!app.udev_source) {
		log(app, Kind::System, "Failed to subscribe to udev. Use Refresh");
	}
}

// Подписывается на сигнал PrepareForSleep службы logind и подключает D-Bus к циклу GLib.
static void start_sleep_monitor(Logic& app) {
	int result = sd_bus_open_system(&app.bus);
	if (result >= 0) {
		// Фильтр выбирает конкретный сигнал logind; он не различает сон и гибернацию.
		result = sd_bus_add_match(
		    app.bus, &app.sleep_slot,
		    "type='signal',sender='org.freedesktop.login1',path='/org/freedesktop/login1',"
		    "interface='org.freedesktop.login1.Manager',member='PrepareForSleep'",
		    on_prepare_for_sleep, &app);
	}
	if (result >= 0) {
		const int fd = sd_bus_get_fd(app.bus);
		if (fd >= 0) {
			app.bus_source = g_unix_fd_add(
			    fd, static_cast<GIOCondition>(G_IO_IN | G_IO_ERR | G_IO_HUP), on_bus_ready, &app);
		}
		// Синхронная регистрация могла уже оставить сигналы во внутренней очереди.
		while (sd_bus_process(app.bus, nullptr) > 0) {
		}
	}
	if (!app.bus_source) {
		log(app, Kind::System, "logind sleep events are unavailable; button requests are logged");
	}
}

// Запускает подписки на изменения устройств питания и переходы в сон.
void start_monitors(Logic& app) {
	start_udev_monitor(app);
	start_sleep_monitor(app);
}

// Удаляет обработчики GLib и освобождает подключения udev и D-Bus при завершении приложения.
void stop_monitors(Logic& app) {
	if (app.udev_source) {
		g_source_remove(app.udev_source);
	}
	if (app.bus_source) {
		g_source_remove(app.bus_source);
	}
	sd_bus_slot_unref(app.sleep_slot);
	sd_bus_unref(app.bus);
	if (app.monitor) {
		udev_monitor_unref(app.monitor);
	}
	if (app.context) {
		udev_unref(app.context);
	}
	// После завершения главного цикла асинхронные callbacks больше не исполняются.
}
