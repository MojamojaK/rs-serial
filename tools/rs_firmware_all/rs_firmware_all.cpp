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
		return EXIT_FAILURE;
	}
	std::cout << fw_path->string() << std::endl;

	rs2::context context;
	const rs2::device_list &device_list = context.query_devices();
	const size_t device_count = device_list.size();
	if (device_count <= 0)
	{
		return EXIT_FAILURE;
	}
	std::cout << "Found " << device_count << " devices" << std::endl;
	for (uint32_t i = 0; i < device_count; i++)
	{
		try
		{
			device_list[i].get_info(rs2_camera_info::RS2_CAMERA_INFO_SERIAL_NUMBER);
		}
		catch (const rs2::error &e)
		{
			std::cerr << "Failed Reading Serial number of " << (i + 1) << "th device: " << e.get_failed_function() << "(" << e.get_failed_args() << "):" << e.what() << std::endl;
			return EXIT_FAILURE;
		}
	}

	const std::vector<uint8_t> fw_image = read_fw_file(*fw_path);
	if (fw_image.empty())
	{
		return EXIT_FAILURE;
	}

	size_t remaining = 0;
	std::mutex m;
	std::condition_variable cv;
	std::queue<rs2::update_device> update_queue;

	std::vector<std::thread> thread_pool(4, std::thread([&]()
	{
		for(;;)
		{
			std::unique_lock<std::mutex> lk(m);
			cv.wait(lk);
			if (remaining <= 0)
			{
				break;
			}
		}
	}));
	

	context.set_devices_changed_callback([&](rs2::event_information &info)
		{
			const rs2::device_list& new_devices = info.get_new_devices();
			const uint32_t new_device_count = new_devices.size();
			if (new_device_count == 0) return;
			for (size_t i = 0; i < new_device_count; i++)
			{
				const rs2::device &device = new_devices[i];
				if (device.is<rs2::update_device>())
				{
					update_queue.push(device.as<rs2::update_device>());
					cv.notify_all();
				}
			}
		}
	);

	const std::function<void(const rs2::device&)> enter_update = 
		[](const rs2::device &device)
	{
		device.as<rs2::updatable>().enter_update_state();
	};
	t.unlock();
	
    return EXIT_SUCCESS;
}