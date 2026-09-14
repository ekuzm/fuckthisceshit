#pragma once

#include <string>

// Заменяет повреждённые последовательности UTF-8 символом замены и возвращает исправленный текст.
std::string valid_utf8(const std::string& text);
// Преобразует признак внешнего питания в текст для интерфейса и журнала.
std::string source_name(int online);
// Преобразует статус батареи из sysfs в подпись; незнакомые статусы считает неизвестными.
std::string status_name(const std::string& status);
// Округляет процент до целого и добавляет знак %; отрицательное значение показывает как неизвестное.
std::string percent(double value);
