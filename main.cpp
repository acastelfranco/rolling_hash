#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <map>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <vector>
#include <memory>
#include <endian.h>
#include <zlib.h>

class CompressionService
{
public:
	/**
	 * @brief compress binary buffer
	 *
	 * @param in preallocated input buffer
	 * @param in_len input buffer size
	 * @param out preallocated output buffer
	 * @param max_out_len maximum output buffer size
	 * @return size_t
	 */
	static size_t compress(const uint8_t *in, uint64_t in_len, const uint8_t *out, uint64_t max_out_len)
	{
		z_stream defstream;
		defstream.zalloc = Z_NULL;
		defstream.zfree = Z_NULL;
		defstream.opaque = Z_NULL;
		defstream.avail_in = (uInt)in_len + 1;
		defstream.next_in = (Bytef *)in;
		defstream.avail_out = (uInt)max_out_len;
		defstream.next_out = (Bytef *)out;
		deflateInit(&defstream, Z_BEST_COMPRESSION);
		deflate(&defstream, Z_FINISH);
		deflateEnd(&defstream);

		return defstream.total_out;
	}

	/**
	 * @brief decompress binary buffer
	 *
	 * @param in preallocated input buffer
	 * @param in_len input buffer size
	 * @param out preallocated output buffer
	 * @param max_out_len maximum output buffer size
	 * @return size_t
	 */
	static size_t decompress(const uint8_t *in, uint64_t in_len, uint8_t *out, uint64_t max_out_len)
	{
		z_stream infstream;
		infstream.zalloc = Z_NULL;
		infstream.zfree = Z_NULL;
		infstream.opaque = Z_NULL;
		infstream.avail_in = (uInt)in_len;
		infstream.next_in = (Bytef *)in;
		infstream.avail_out = (uInt)max_out_len;
		infstream.next_out = (Bytef *)out;
		inflateInit(&infstream);
		inflate(&infstream, Z_NO_FLUSH);
		inflateEnd(&infstream);

		return infstream.total_out - 1;
	}
};

class SignatureException : public std::runtime_error
{
public:
	SignatureException(std::string const &msg) : std::runtime_error(msg) {}
	virtual ~SignatureException() {}
};

class MalformedFileException : public std::runtime_error
{
public:
	MalformedFileException(std::string const &msg) : std::runtime_error(msg) {}
	virtual ~MalformedFileException() {}
};

struct Signature
{
	uint64_t hash;
	uint64_t size;
};

struct SignatureFileHeader
{
	uint64_t magic;
	uint64_t chunks;
};

/**
 * @brief this class save generated signature for a file
 *
 */
class SignatureService
{
public:
	SignatureService() {}

	SignatureService(const std::unique_ptr<std::vector<Signature>> &in)
	{
		m_signatures = *in.get();
	}

	virtual ~SignatureService() {}

	void append(const Signature &entry)
	{
		m_signatures.push_back(entry);
	}

	/**
	 * @brief load the signature from the given file. It clears previously loaded chunks
	 *
	 * @param filename file name
	 */
	void load(std::string filename) throw()
	{
		SignatureFileHeader header = {0};

		std::ifstream ifs(filename, std::ifstream::in | std::ifstream::ate | std::ifstream::binary);
		uint64_t compressedBlobSize = static_cast<uint64_t>(ifs.tellg()) - sizeof(SignatureFileHeader);
		ifs.seekg(std::ifstream::beg);
		ifs.read(reinterpret_cast<char *>(&header), sizeof(SignatureFileHeader));

		header.magic = be64toh(header.magic);
		header.chunks = be64toh(header.chunks);

		if (header.magic != MAGIC)
			throw SignatureException("invalid magic");

		uint64_t len = sizeof(uint64_t) * header.chunks * 2;

		std::unique_ptr<uint8_t[]> in(new uint8_t[len]);
		std::unique_ptr<uint8_t[]> out(new uint8_t[len]);

		if (!ifs.good() || len == 0)
			throw MalformedFileException("unexpected length");

		ifs.read(reinterpret_cast<char *>(in.get()), compressedBlobSize);

		uint64_t decompressedSize = CompressionService::decompress(in.get(), compressedBlobSize, out.get(), len);

		uint64_t *outPtr = reinterpret_cast<uint64_t *>(out.get());

		m_signatures.clear();
		for (int i = 0; i < header.chunks; i++)
		{
			uint64_t hash = 0;
			uint64_t size = 0;

			uint64_t *hashPtr = outPtr++;
			uint64_t *sizePtr = outPtr++;

			hash = be64toh(*hashPtr);
			size = be64toh(*sizePtr);

			Signature entry = {hash, size};

			m_signatures.push_back(entry);
		}

		ifs.close();
	}

	/**
	 * @brief save the signature in the given file
	 *
	 * @param filename file name
	 */
	void save(std::string filename) throw()
	{
		uint64_t len = sizeof(uint64_t) * m_signatures.size() * 2;

		std::unique_ptr<uint8_t[]> in(new uint8_t[len]);
		std::unique_ptr<uint8_t[]> out(new uint8_t[len]);

		SignatureFileHeader header = {htobe64(MAGIC), htobe64(m_signatures.size())};
		std::ofstream ofs(filename, std::ofstream::out | std::ofstream::binary);
		ofs.write(reinterpret_cast<char *>(&header), sizeof(SignatureFileHeader));

		uint64_t *inPtr = reinterpret_cast<uint64_t *>(in.get());

		for (Signature entry : m_signatures)
		{
			entry.hash = htobe64(entry.hash);
			entry.size = htobe64(entry.size);
			std::memcpy(inPtr++, &entry.hash, sizeof(uint64_t));
			std::memcpy(inPtr++, &entry.size, sizeof(uint64_t));
		}

		uint64_t compressedSize = CompressionService::compress(in.get(), len, out.get(), len);
		ofs.write(reinterpret_cast<const char *>(out.get()), compressedSize);

		m_signatures.clear();
		ofs.close();
	}

	/**
	 * @brief print the signature content, excluding the header
	 *
	 */
	void print()
	{
		uint32_t i = 0;
		for (Signature entry : m_signatures)
		{
			printf("chunk %u hash: %lu\n", i, entry.hash);
			printf("chunk %u size: %lu\n", i++, entry.size);
		}
	}

	/**
	 * @brief clear signatures vector
	 * 
	 */
	void clear() {
		m_signatures.clear();
	}

	/**
	 * @brief overloading of the subscript operators
	 * 
	 * @param pos 
	 * @return Signature& 
	 */
	Signature &operator[]( size_t pos ) {
		return m_signatures[pos];
	}

	Signature const &operator[]( size_t pos ) const {
		return m_signatures[pos];
	}

private:
	std::vector<Signature> m_signatures;

	static constexpr uint64_t MAGIC = 0xC000FFEE;
};

class HashService
{
public:
	/**
	 * @brief compute the hash value for the given data
	 *
	 * @param data input buffer
	 * @param size size of the buffer we want to calcute the hash
	 * @return uint64_t hash value
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
	 * @return uint64_t hash value
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
  		hashValue -= (power * data[0]) % M;
  		hashValue <<= BSHIFT;
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

		uint64_t wholeChunks = size / chunkSize;
		uint8_t *lastChunk = data + wholeChunks * chunkSize;
		uint64_t lastChunkSize = size % chunkSize;

		for (uint8_t *currentChunk = data, *lastChunk = data + wholeChunks * chunkSize; currentChunk < lastChunk; currentChunk += chunkSize)
		{
			Signature entry = {hash(currentChunk, chunkSize), chunkSize};
			signatures->push_back(entry);
		}

		Signature entry = {hash(lastChunk, lastChunkSize), lastChunkSize};
		signatures->push_back(entry);

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

	static uint32_t search(uint8_t *data, uint32_t size, uint8_t *chunk, uint32_t chunkSize)
	{
		uint32_t hashChunk = hash(chunk, chunkSize);
		uint32_t hashData  = hash(data, chunkSize);

		uint32_t i = 0; do
		{
			if (hashChunk == hashData)
			 	if(compare(&data[i], chunk, chunkSize))
					return i;

			hashData = rolling_hash(&data[++i], chunkSize, hashData);
		}
		while (i < (size - chunkSize));

		return size;
	}

	static constexpr uint32_t B = 256;
	static constexpr uint32_t BSHIFT = 8;
	static constexpr uint32_t M = 4294967291;
};

struct FileHandle
{
	uint64_t size;
	std::unique_ptr<uint8_t[]> data;
};

class FileService
{
public:
	static FileHandle load(std::string filename) throw()
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

static constexpr uint32_t CHUNKSIZ = 0xFFFF;

int main(int argc, const char **argv)
{
	FileHandle fileHandle1 = FileService::load("starwars_a_new_hope.txt");
	FileHandle fileHandle2 = FileService::load("starwars_a_new_hope_modified.txt");
	std::unique_ptr<std::vector<Signature>> signatures1 = HashService::getSignatures(fileHandle1.data.get(), fileHandle1.size, CHUNKSIZ);
	std::unique_ptr<std::vector<Signature>> signatures2 = HashService::getSignatures(fileHandle2.data.get(), fileHandle2.size, CHUNKSIZ);

	SignatureService sig1(signatures1);
	SignatureService sig2(signatures2);

	sig1.print();
	sig2.print();

	sig1.save("starwars_a_new_hope.sig.bin");
	sig2.save("starwars_a_new_hope_modified.sig.bin");

#if 0
	sig1.load("starwars_a_new_hope.sig.bin");
	sig2.load("starwars_a_new_hope_modified.sig.bin");

	uint32_t hash = HashService::hash(fileHandle1.data.get(), CHUNKSIZ);
	uint32_t hashNext = HashService::hash(fileHandle1.data.get() + 1, CHUNKSIZ);
	uint32_t hashRoll = HashService::rolling_hash(fileHandle1.data.get(), CHUNKSIZ, hash);

	printf("hash: %u\n", hash);
	printf("hashNext: %u\n", hashNext);
	printf("hashRoll: %u\n", hashRoll);
#endif
}
