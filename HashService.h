#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <Signature.h>

class HashService
{
public:
	/**
	 * @brief compute the hash value for the given data
	 *
	 * @param data input buffer
	 * @param size size of the buffer we want to calcute the hash
	 * @return uint32_t hash value
	 */
	static uint32_t hash(uint8_t *data, uint32_t size)
	{
		uint64_t hashValue = 0;
		
		for (uint32_t i = 0; i < size; i++) {
    		hashValue = ((hashValue << BSHIFT) + data[i]) % M;
  		}

		return static_cast<uint32_t>(hashValue);
	}

	/**
	 * @brief compute the hash value for a one-byte-shifted string adding one character
	 *        (e.g. original string: ABBA, next string: BBAB, chunkSize = 4)
	 * 
	 * @param data input buffer
	 * @param size size of the buffer we want to calcute the hash
	 * @param prevHash previous hash
	 * @return uint32_t hash value
	 */
	static uint32_t rolling_hash(uint8_t* data, uint32_t size, uint32_t prevHash)
	{
  		uint64_t hashValue = prevHash;
  		uint64_t power = 1;
  		
		for (uint32_t i = 1; i < size; i++) {
    		power <<= BSHIFT;
    		power %= M;
  		}

  		hashValue += M;
  		hashValue -= ((power * data[0]) % M);
  		hashValue <<= BSHIFT;
		hashValue %= M;
  		hashValue += data[size];  
  		hashValue %= M;

  		return static_cast<uint32_t>(hashValue);
	}

	/**
	 * @brief get a list of the signatures for each chunk of a buffer
	 *
	 * @param data input buffer
	 * @param size buffer size
	 * @param chunkSize chunk size
	 * @return std::unique_ptr<std::vector<Signature>>
	 */
	static std::unique_ptr<std::vector<Signature>> getSignatures(uint8_t *data, uint32_t size, uint32_t chunkSize)
	{
		std::unique_ptr<std::vector<Signature>> signatures(new std::vector<Signature>());

		uint32_t chunkId = 0;
		uint8_t *dataPtr = nullptr;
		uint8_t *end = data + size - chunkSize;

		for(dataPtr = data ; dataPtr < end; dataPtr += chunkSize, chunkId++) {
			signatures->push_back({chunkId, static_cast<uint32_t>(dataPtr - data), hash(dataPtr, chunkSize), chunkSize});
		}

		uint32_t lastChunkSize = size % chunkSize;
		signatures->push_back({chunkId, static_cast<uint32_t>(dataPtr - data), hash(dataPtr, lastChunkSize), lastChunkSize});

		return std::move(signatures);
	}

	/**
	 * @brief function to compare two binary blobs of the same size
	 * 
	 * @param data1  first blob
	 * @param data2  second blob
	 * @param size   blob size
	 * @return true  blobs match
	 * @return false blobs unmatch
	 */
	static bool compare(uint8_t *data1, uint8_t *data2, uint32_t size) {
		for(int i = 0; i < size; i++)
			if (data1[i] != data2[i])
				return false;
		return true;
	}

    /**
     * @brief search a pattern into a binary buffer
     * 
     * @param data 
     * @param size 
     * @param chunkHash 
     * @param chunkSize 
     * @return uint32_t 
     */
	static uint32_t search(uint8_t *data, uint32_t size, uint32_t chunkHash, uint32_t chunkSize)
	{
		uint32_t offset = 0;
		uint32_t dataHash  = hash(data, chunkSize);
		uint8_t *dataPtr = data;
		uint8_t *end = dataPtr + size - chunkSize;

		if (chunkHash == dataHash) return offset;

		for(offset = 1; dataPtr < end; dataPtr++, offset++) {
			dataHash = rolling_hash(dataPtr, chunkSize, dataHash);
			if (chunkHash == dataHash) return offset;
		}

		dataHash = rolling_hash(dataPtr, size % chunkSize, dataHash);
		if (chunkHash == dataHash) return offset;

		return size;
	}

	static constexpr uint32_t BSHIFT = 8;
	
	static constexpr uint32_t B = 1 << BSHIFT;
	static constexpr uint32_t M = 4294967291;
};