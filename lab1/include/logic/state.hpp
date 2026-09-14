#pragma once

#include "power.hpp"

#include <functional>
#include <string>
#include <vector>

struct udev;
struct udev_monitor;
struct sd_bus;
struct sd_bus_slot;

enum class Kind { All, Charger, Charge, Sleep, System };
struct Event {
	Kind kind;
	std::string line;
};

// Логика не хранит GTK-виджеты: интерфейс подписывается на изменения.
// Все операции и уведомления выполняются в одном главном цикле GLib.
struct Logic {
	std::vector<Event> events;
	PowerState state;
	bool initialized = false;
	bool busy = false;
	// Необязательные обработчики UI: логика вызывает их, не зная об окнах и виджетах.
	std::function<void()> on_state_changed;
	std::function<void()> on_log_changed;
	std::function<void(const std::string&)> on_error;

	udev* context{};
	udev_monitor* monitor{};
	sd_bus* bus{};
	sd_bus_slot* sleep_slot{};
	// Идентификаторы регистраций в GLib нужны для удаления обработчиков; {} задаёт нули.
	unsigned int udev_source{}, bus_source{};
};

// Читает новый снимок, записывает изменения в журнал, сохраняет данные и уведомляет интерфейс.
void refresh(Logic& app);
