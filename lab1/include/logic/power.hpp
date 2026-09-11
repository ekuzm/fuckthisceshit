#pragma once

#include <string>
#include <vector>

struct Battery {
	std::string name;
	std::string status;
	double percent = -1;
	double minutes = -1;
};

struct PowerState {
	// -1 означает отсутствие достоверных данных, а не работу от батареи.
	int online = -1;
	std::vector<Battery> batteries;
	std::string error;
};

PowerState read_power();

const Battery* find_battery(const PowerState& state, const std::string& name);
