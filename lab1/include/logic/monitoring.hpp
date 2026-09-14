#pragma once

struct Logic;

// Запускает подписки на изменения устройств питания и переходы в сон.
void start_monitors(Logic& app);
// Удаляет обработчики GLib и освобождает подключения udev и D-Bus при завершении приложения.
void stop_monitors(Logic& app);
