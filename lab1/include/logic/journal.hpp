#pragma once

#include <string>

struct Logic;
enum class Kind;

// Добавляет событие с временем и категорией в память и уведомляет интерфейс об изменении журнала.
void log(Logic& app, Kind kind, const std::string& message);
// Записывает ошибку в журнал и передаёт её обработчику интерфейса, если он назначен.
void report_error(Logic& app, const std::string& text);
// Сохраняет все события в текстовый файл независимо от выбранного фильтра окна журнала.
void save_log(Logic& app, const std::string& path);
