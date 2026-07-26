#pragma once
#include <vector>
#include <string>
#include <fstream>

namespace aZero::File
{
	inline std::string ReadString(const std::vector<std::byte>& byteArray, uint64_t& inOutOffset)
	{
		uint32_t len;
		std::memcpy((char*)&len, &byteArray[inOutOffset], sizeof(len));

		inOutOffset += sizeof(len);

		if (len == 0) {
			return "";
		}

		std::string output;
		output.resize(len);
		std::memcpy(output.data(), &byteArray[inOutOffset], len);

		inOutOffset += len;

		return output;
	}

	template<typename T>
	inline std::vector<T> ReadVector(const std::vector<std::byte>& byteArray, uint64_t& inOutOffset)
	{
		uint32_t size;
		std::memcpy((char*)&size, &byteArray[inOutOffset], sizeof(size));
		inOutOffset += sizeof(size);

		uint32_t stride;
		std::memcpy((char*)&stride, &byteArray[inOutOffset], sizeof(stride));
		inOutOffset += sizeof(stride);

		if (sizeof(T) != stride) {
			throw std::runtime_error("Stride doesn't match template argument element size.");
		}

		if (size == 0) {
			return std::vector<T>();
		}

		std::vector<T> output(size);
		std::memcpy(output.data(), &byteArray[inOutOffset], size * stride);

		inOutOffset += size * stride;

		return output;
	}

	inline void WriteString(const std::string& str, std::ofstream& file)
	{
		uint32_t len = static_cast<uint32_t>(str.length());
		file.write((char*)&len, sizeof(len));

		if (str.empty()) {
			return;
		}
		file.write(str.data(), len);
	}

	inline std::string ReadString(std::ifstream& file)
	{
		uint32_t len;
		file.read((char*)&len, sizeof(len));

		if (len == 0) {
			return "";
		}

		std::string output;
		output.resize(len);
		file.read(output.data(), len);
		return output;
	}

	template<typename T>
	inline void WriteVector(const std::vector<T>& vector, std::ofstream& file)
	{
		uint32_t vecLen = (uint32_t)vector.size();
		file.write((char*)&vecLen, sizeof(vecLen));

		uint32_t stride = (uint32_t)sizeof(T);
		file.write((char*)&stride, sizeof(stride));

		if (vector.size() == 0) {
			return;
		}

		file.write((char*)vector.data(), vector.size() * sizeof(T));
	}

	template<typename T>
	inline std::vector<T> ReadVector(std::ifstream& file)
	{
		uint32_t size, stride;
		file.read((char*)&size, sizeof(size));
		file.read((char*)&stride, sizeof(stride));

		if (sizeof(T) != stride) {
			throw std::runtime_error("Stride doesn't match template argument element size.");
		}

		if (size == 0) {
			return std::vector<T>();
		}

		std::vector<T> output(size);
		file.read((char*)output.data(), size * stride);
		return output;
	}
}