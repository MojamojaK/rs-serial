#include <cstdlib>
#include <iostream>
#include <ostream>
#include <fstream>
#include <vector>

#include <librealsense2/rs.hpp>

int main (void) {
	const rs2::context context;
	const rs2::device_list &device_list = context.query_devices();
	const size_t device_count = device_list.size();
	std::vector<std::string> serials;
	serials.reserve(device_count);
	
	std::cout << "Found " << device_count << " devices" << std::endl;
	for (uint32_t i = 0; i < device_count; i++)
	{
		try
		{
			const std::string &serial = device_list[i].get_info(rs2_camera_info::RS2_CAMERA_INFO_SERIAL_NUMBER);
			serials.emplace_back(serial);
		}
		catch (const rs2::error &e)
		{
			std::cerr << "Failed Reading Serial number of " << (i + 1) << "th device: " << e.get_failed_function() << "(" << e.get_failed_args() << "):" << e.what() << std::endl;
			return EXIT_FAILURE;
		}
	}

	const size_t actual_size = serials.size();
	std::ofstream output("serials.txt", std::ios::out);
	for (size_t i = 0; i < actual_size; i++) {
		std::ostringstream oss;
		oss << "#" << i << " " << serials[i] << std::endl;
		std::cout << oss.str();
		output << oss.str();
	}
	output.close();
	
    return EXIT_SUCCESS;
}