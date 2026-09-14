#pragma once

struct Logic;

// Запускает systemctl suspend/hibernate без ожидания в UI; повторный запрос во время выполнения пропускает.
void request_power_action(Logic& app, const char* action);
