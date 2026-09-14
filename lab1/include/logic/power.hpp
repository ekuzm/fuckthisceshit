#pragma once

#include "constants.hpp"

#include <string>
#include <vector>

// Показания одной батареи: неизвестные процент и время обозначаются UNKNOWN_READING.
struct Battery {
	std::string name;
	std::string status;
	double percent = UNKNOWN_READING;
	double minutes = UNKNOWN_READING;
};

// Один снимок питания компьютера; при ошибке обхода может содержать частично прочитанные данные.
struct PowerState {
	// -1 означает отсутствие достоверных данных, а не работу от батареи.
	int online = POWER_SOURCE_UNKNOWN;
	std::vector<Battery> batteries;
	std::string error;
};

// Обходит устройства питания, собирает батареи и определяет внешнее питание; ошибки сохраняет в результате.
PowerState read_power();

const Battery* find_battery(const PowerState& state, const std::string& name);
