#pragma once

#include <memory>
#include <cstdint>
#include <fstream>

struct FileHandle
{
	uint64_t size;
	std::unique_ptr<uint8_t[]> data;
};

class FileService
{
public:
    /**
     * @brief load a file in memory a return a file handle to access it
     * 
     * @param filename 
     * @return FileHandle 
     */
	static FileHandle load(const std::string &filename) throw()
	{
		FileHandle ret;

		std::ifstream ifs(filename, std::ifstream::in | std::ifstream::ate | std::ifstream::binary);
		uint64_t fileSize = ifs.tellg();
		ifs.seekg(std::ifstream::beg);

		std::unique_ptr<uint8_t[]> buffer(new uint8_t[fileSize]);
		ifs.read(reinterpret_cast<char *>(buffer.get()), fileSize);
		ifs.close();

		ret.size = fileSize;
		ret.data = std::move(buffer);

		return ret;
	}
};