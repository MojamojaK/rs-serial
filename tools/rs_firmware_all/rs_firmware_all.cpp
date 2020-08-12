#include <cstdlib>
#include <condition_variable>
#include <iostream>
#include <ostream>
#include <fstream>
#include <vector>
#include <optional>
#include <filesystem>
#include <string>

#include <librealsense2/rs.hpp>
#include <queue>
#include <atomic>
#include <map>

const std::filesystem::path BASE_FW_DR = "./fw";

std::vector<std::string> tokenize(std::string const &str, const char delimiter)
{
	std::vector<std::string> out;
    size_t start;
    size_t end = 0;
 
    while ((start = str.find_first_not_of(delimiter, end)) != std::string::npos)
    {
        end = str.find(delimiter, start);
        out.emplace_back(str.substr(start, end - start));
    }
	return out;
}

typedef struct fw_version {
	unsigned int major;
	unsigned int minor;
	unsigned int patch;
	unsigned int revision;	
} fw_version;


std::optional<fw_version> parse_fw_filename(const std::string &filename) {
	const std::vector<std::string> tokens = tokenize(filename, '_');
	if (tokens.size() != 7) {
		return std::nullopt;
	}
	try
	{
		const unsigned int major = std::stoi(tokens[3]);
		const unsigned int minor = std::stoi(tokens[4]);
		const unsigned int patch = std::stoi(tokens[5]);
		const unsigned int revision = std::stoi(tokens[6]);
		// std::cout << "fw_version(" << major << ", " << minor << ", "
		// << patch << ", " << revision << ")" << std::endl;
		return fw_version{major, minor, patch, revision};
	}
	catch (const std::invalid_argument &) {
		return std::nullopt;
	}
	catch (const std::out_of_range &) {
		return std::nullopt;
	}
}

std::optional<std::filesystem::path> get_latest_firmware_path()
{
	if (!std::filesystem::is_directory(BASE_FW_DR)) {
		return std::nullopt;
	}
	std::vector<std::pair<fw_version, std::filesystem::path>> fws;
	for (const std::filesystem::directory_entry &child: std::filesystem::directory_iterator(BASE_FW_DR)) {
		const std::filesystem::path &path = child.path();
		if (child.exists() && child.is_regular_file() && path.extension() == ".bin") {
			const std::string filename = path.filename().string();
			if (const std::optional<fw_version> version = parse_fw_filename(filename)) {
				// std::cout << path << std::endl;
				fws.emplace_back(std::make_pair(*version, path));
			}
		}
	}
	if (fws.empty()) {
		return std::nullopt;
	}
	const std::function<bool(const std::pair<fw_version, std::filesystem::path>, const std::pair<fw_version, std::filesystem::path>)>
		compare_fw = [](const std::pair<fw_version, std::filesystem::path> &a, const std::pair<fw_version, std::filesystem::path> &b)
	{
		if (b.first.major > a.first.major) return true;
		if (b.first.major < a.first.major) return false;
		if (b.first.minor > a.first.minor) return true;
		if (b.first.minor < a.first.minor) return false;
		if (b.first.patch > a.first.patch) return true;
		if (b.first.patch < a.first.patch) return false;
		if (b.first.revision > a.first.revision) return true;
		if (b.first.revision < a.first.revision) return false;
		return false;
	};
	std::sort(fws.begin(), fws.end(), compare_fw);
	return std::filesystem::absolute(fws[fws.size()-1].second);
}

std::vector<uint8_t> read_fw_file(const std::filesystem::path &file_path)
{
    std::vector<uint8_t> rv;

    std::ifstream file(file_path, std::ios::in | std::ios::binary | std::ios::ate);
    if (file.is_open())
    {
        rv.resize(file.tellg());

        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(rv.data()), rv.size());
        file.close();
    }

    return rv;
}

int main (void) {
	const std::optional<std::filesystem::path> &fw_path = get_latest_firmware_path();
	if (!fw_path.has_value())
	{
		std::cout << "Firmware not found" << std::endl;
		return EXIT_FAILURE;
	}
	std::cout << fw_path->string() << std::endl;

	const std::vector<uint8_t> fw_image = read_fw_file(*fw_path);
	if (fw_image.empty())
	{
		std::cout << "Firmware file empty" << std::endl;
		return EXIT_FAILURE;
	}

	const rs2::context context;
	const rs2::device_list &device_list = context.query_devices();
	const size_t device_count = device_list.size();
	if (device_count <= 0)
	{
		std::cout << "Devices not found" << std::endl;
		return EXIT_FAILURE;
	}

	std::map<std::string, std::string> serial_update_map;
	for (uint32_t i = 0; i < device_count; i++)
	{
		try
		{
			const rs2::device &device = device_list[i];
			if (device.supports(rs2_camera_info::RS2_CAMERA_INFO_SERIAL_NUMBER) 
				&& device.supports(rs2_camera_info::RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID))
			{
				const std::string serial_number = device.get_info(rs2_camera_info::RS2_CAMERA_INFO_SERIAL_NUMBER);
				const std::string update_id = device.get_info(rs2_camera_info::RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID);
				serial_update_map[update_id] = serial_number;
			}
		}
		catch (rs2::error &e)
		{
			std::cout << "Setting device #" << (i + 1) << " to update state Failed "
				<< e.get_failed_function() << "(" << e.get_failed_args() << "): "
				<< e.what() << " (Try Again)" << std::endl;
			return EXIT_FAILURE;
		}
	};

	std::cout << "Set all devices to update state" << std::endl;
	for (uint32_t i = 0; i < device_count; i++)
	{
		try
		{
			const rs2::device &device = device_list[i];
			if (!device.is<rs2::update_device>() && device.is<rs2::updatable>())
			{
				device.as<rs2::updatable>().enter_update_state();
			}
		}
		catch (rs2::error &e)
		{
			std::cout << "Setting device #" << (i + 1) << " to update state Failed "
				<< e.get_failed_function() << "(" << e.get_failed_args() << "): "
				<< e.what() << " (Try Again)" << std::endl;
			return EXIT_FAILURE;
		}
	};
	std::cout << "Set all devices to update state Complete" << std::endl;

	std::cout << "Wait for all devices to enter update state" << std::endl;
	const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	for (;;)
	{
		try
		{
			const rs2::device_list &current_devices = context.query_devices();
			const uint32_t current_device_count = current_devices.size();
			uint32_t update_device_count = 0;
			for (uint32_t i = 0; i < current_device_count; i++)
			{
				const rs2::device &device = current_devices[i];
				if (device.is<rs2::update_device>())
				{
					update_device_count++;
				}
			}
			if (update_device_count >= device_count)
			{
				break;
			}
		}
		catch (...)
		{
		}
		if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() > 15)
		{
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
	std::cout << "Wait for all devices to enter update state Complete" << std::endl;

	std::cout << "Updating devices !!!DO NOT CLOSE PROGRAM OR SHUTDOWN!!!" << std::endl;
	for (;;)
	{
		const rs2::device_list &current_devices = context.query_devices();
		const uint32_t current_device_count = current_devices.size();
		if (current_device_count < device_count)
		{
			continue;
		}
		std::vector<std::thread> threads;
		uint32_t count = 0;
		std::condition_variable cv;
		std::mutex m, n, o;
		for (uint32_t i = 0; i < current_device_count; i++)
		{
			threads.emplace_back(std::thread([i, &current_devices, &fw_image, &cv, &m, &n, &o, &count, &serial_update_map]()
			{
				for (;;)
				{
					std::unique_lock<std::mutex> count_lock(n);
					if (count >= 20)
					{
						count_lock.unlock();
						std::unique_lock<std::mutex> lock(m);
						cv.wait(lock);
					} else
					{
						count += 1;
						break;
					}					
				}

				try
				{
					const rs2::device &device = current_devices[i];
					const std::string update_id = device.get_info(rs2_camera_info::RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID);
					const bool has_serial = serial_update_map.find(update_id) != serial_update_map.end();
					std::ostringstream name_ss;
					name_ss << (has_serial ? "sn(" : "uid(");
					name_ss << (has_serial ? serial_update_map[update_id] : update_id);
					name_ss << ")";
					const std::string name = name_ss.str();
					if (device.is<rs2::update_device>())
					{
						std::unique_lock<std::mutex> out_lock(o);
						std::cout << "Updating device " << name << std::endl;
						out_lock.unlock();

						try
						{
							device.as<rs2::update_device>().update(fw_image);
						}
						catch (const rs2::error &e)
						{
							out_lock.lock();
							std::cout << "Updating device " << name << " Failed "
								<< e.get_failed_function() << "(" << e.get_failed_args() << "): "
								<< e.what() << std::endl;
							out_lock.unlock();
							throw;
						}
						
						out_lock.lock();
						std::cout << "Updating device " << name << " Completed" << std::endl;
						out_lock.unlock();
					}
					else
					{
						std::unique_lock<std::mutex> out_lock(o);
						std::cout << "Skipping device " << name << std::endl;
						out_lock.unlock();
					}
				}
				catch (...) {}
				std::unique_lock<std::mutex> count_lock(n);
				count -= 1;
				count_lock.unlock();
				cv.notify_one();
			}));
		};
		for (std::thread &thread: threads)
		{
			thread.join();
		}
		break;
	}
	std::cout << "Updating devices Complete" << std::endl;
	
    return EXIT_SUCCESS;
}
