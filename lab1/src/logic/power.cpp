#include "constants.hpp"
#include "logic/power.hpp"

#include <cmath>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <fstream>

// Читает первую строку файла; если прочитать нечего или файл недоступен, возвращает пустую строку.
static std::string read_text(const std::filesystem::path& path) {
	std::ifstream input(path);
	std::string value;
	std::getline(input, value);
	return value;
}

// Читает конечное число из файла; при ошибке возвращает UNKNOWN_READING, по запросу берёт модуль.
static double number(const std::filesystem::path& path, bool absolute = false) {
	std::ifstream input(path);
	double value;
	// При ошибке чтения правая часть || не вычисляется: не используем незаполненное value.
	if (!(input >> value) || !std::isfinite(value)) {
		return UNKNOWN_READING;
	}
	if (absolute) {
		return std::abs(value);
	}
	return value;
}

// Вычисляет процент как текущий запас / полный запас × 100 и ограничивает его сверху.
static double charge_percent(double now, double full) {
	// Оба запаса должны быть в одинаковых единицах; проверка full защищает от деления на ноль.
	if (now < 0 || full <= 0) {
		return UNKNOWN_READING;
	}
	double percent = FULL_CHARGE_PERCENT * now / full;
	if (percent > FULL_CHARGE_PERCENT) {
		percent = FULL_CHARGE_PERCENT;
	}
	return percent;
}

// Оценивает минуты работы по запасу и расходу; при недостоверных данных возвращает UNKNOWN_READING.
static double remaining_minutes(double now, double rate) {
	if (now < 0 || rate <= 0) {
		return UNKNOWN_READING;
	}
	// Энергия / мощность или заряд / ток дают часы; умножение переводит их в минуты.
	const double minutes = MINUTES_PER_HOUR * now / rate;
	if (!std::isfinite(minutes)) {
		return UNKNOWN_READING;
	}
	return minutes;
}

// Читает статус одной батареи, получает или рассчитывает процент и время разряда.
static Battery read_battery(const std::filesystem::path& path) {
	Battery battery;
	battery.name = path.filename().string();
	// Оператор / у filesystem::path добавляет имя файла к пути каталога.
	battery.status = read_text(path / "status");
	if (battery.status.empty()) {
		battery.status = BATTERY_STATUS_UNKNOWN;
	}
	const double energy = number(path / "energy_now");
	const double charge = number(path / "charge_now");
	// Сначала берём готовый capacity, затем пробуем расчёт по энергии, затем по заряду.
	battery.percent = number(path / "capacity");
	if (battery.percent < 0 || battery.percent > FULL_CHARGE_PERCENT) {
		battery.percent = charge_percent(energy, number(path / "energy_full"));
	}
	if (battery.percent < 0) {
		battery.percent = charge_percent(charge, number(path / "charge_full"));
	}

	// Время оцениваем только при разряде. true просит number взять модуль мощности или тока.
	if (battery.status == BATTERY_STATUS_DISCHARGING) {
		battery.minutes = remaining_minutes(energy, number(path / "power_now", true));
		if (battery.minutes < 0) {
			battery.minutes = remaining_minutes(charge, number(path / "current_now", true));
		}
	}
	return battery;
}

// Обходит устройства питания, собирает батареи и определяет внешнее питание; ошибки сохраняет в результате.
PowerState read_power() {
	PowerState state;
	DIR* directory = opendir("/sys/class/power_supply");
	if (directory == nullptr) {
		state.error = std::string("Failed to read power_supply: ") + std::strerror(errno);
		return state;
	}

	// Флаги накапливаются по всем устройствам: найден ли адаптер, есть ли неизвестный или подключённый.
	// Статусы батарей нужны как запасной способ определить питание.
	bool adapter_seen = false;
	bool adapter_unknown = false;
	bool adapter_online = false;
	bool discharging = false;
	bool charging = false;

	while (true) {
		// readdir возвращает nullptr и в конце каталога, и при ошибке; отличаем их по errno.
		errno = 0;
		dirent* entry = readdir(directory);
		if (entry == nullptr) {
			if (errno != 0) {
				state.error = std::string("Failed to enumerate power_supply devices: ") + std::strerror(errno);
			}
			break;
		}

		std::string name = entry->d_name;
		// Служебные записи текущего и родительского каталога не являются устройствами.
		if (name == "." || name == "..") {
			continue;
		}

		std::string path = std::string("/sys/class/power_supply/") + name;
		// Исключаем источники периферии, например мыши; отсутствующий scope сам по себе не исключает устройство.
		if (read_text(path + "/scope") == "Device") {
			continue;
		}

		std::string type = read_text(path + "/type");
		if (type == "Battery") {
			// Пропускаем только явно отсутствующую батарею; ошибка чтения present не равна отсутствию.
			if (number(path + "/present") == BATTERY_ABSENT) {
				continue;
			}

			Battery battery = read_battery(path);
			if (battery.status == BATTERY_STATUS_DISCHARGING) {
				discharging = true;
			}
			if (battery.status == BATTERY_STATUS_CHARGING) {
				charging = true;
			}
			state.batteries.push_back(battery);
			continue;
		}

		// Позиция 0 означает начало строки: подходят также типы USB_C и другие варианты USB.
		bool is_usb = type.find("USB") == 0;
		if (type == "Mains" || is_usb || type == "Wireless") {
			adapter_seen = true;
			// Наличие записи адаптера не означает подключение: это определяет файл online.
			double online = number(path + "/online");
			if (online == POWER_SOURCE_ONLINE) {
				adapter_online = true;
			} else if (online != POWER_SOURCE_OFFLINE) {
				adapter_unknown = true;
			}
		}
	}
	closedir(directory);

	// Сначала доверяем адаптерам; если их данных недостаточно, судим по зарядке/разряду.
	// Если ни одно условие не подошло, сохраняется неизвестный источник питания.
	if (adapter_online) {
		state.online = POWER_SOURCE_ONLINE;
	} else if (adapter_seen && !adapter_unknown) {
		state.online = POWER_SOURCE_OFFLINE;
	} else if (charging && !discharging) {
		state.online = POWER_SOURCE_ONLINE;
	} else if (discharging && !charging) {
		state.online = POWER_SOURCE_OFFLINE;
	}

	// Сортируем по имени обменом элементов, чтобы порядок не зависел от обхода каталога.
	for (std::size_t i = 0; i < state.batteries.size(); i++) {
		for (std::size_t j = i + 1; j < state.batteries.size(); j++) {
			if (state.batteries[j].name < state.batteries[i].name) {
				Battery temporary = state.batteries[i];
				state.batteries[i] = state.batteries[j];
				state.batteries[j] = temporary;
			}
		}
	}
	return state;
}

const Battery* find_battery(const PowerState& state, const std::string& name) {
	for (const Battery& battery : state.batteries) {
		if (battery.name == name) {
			// Адрес элемента вектора, а не копии; действителен, пока элемент существует и не перемещён.
			return &battery;
		}
	}
	return nullptr;
}
