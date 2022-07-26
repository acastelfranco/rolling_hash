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
#include <algorithm>
#include <cstdlib>

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

class DeltaException : public std::runtime_error
{
public:
	DeltaException(const std::string &msg) : std::runtime_error(msg) {}
	virtual ~DeltaException() {};
};

class SignatureException : public std::runtime_error
{
public:
	SignatureException(const std::string &msg) : std::runtime_error(msg) {}
	virtual ~SignatureException() {}
};

class MalformedFileException : public std::runtime_error
{
public:
	MalformedFileException(const std::string &msg) : std::runtime_error(msg) {}
	virtual ~MalformedFileException() {}
};

struct Signature
{
	uint32_t id;
	uint32_t pos;
	uint32_t hash;
	uint32_t size;
};

struct SignatureFileHeader
{
	uint32_t magic;
	uint32_t chunks;
};


class OrderSignatureById
{
public:
    inline bool operator() (const Signature& sig1, const Signature& sig2)
    {
        return (sig1.id < sig2.id);
    }
};

class OrderSignatureByPos
{
public:
    inline bool operator() (const Signature& sig1, const Signature& sig2)
    {
        return (sig1.pos < sig2.pos);
    }
};

class OrderSignatureByHash
{
public:
    inline bool operator() (const Signature& sig1, const Signature& sig2)
    {
        return (sig1.hash < sig2.hash);
    }
};

class OrderSignatureBySize
{
public:
    inline bool operator() (const Signature& sig1, const Signature& sig2)
    {
        return (sig1.size < sig2.size);
    }
};

/**
 * @brief this class save generated signature for a file
 *
 */
class SignatureFile
{
public:
	SignatureFile() {}

	SignatureFile(const std::vector<Signature> &in)
	{
		m_signatures = in;
	}

	virtual ~SignatureFile() {}

	void append(const Signature &entry)
	{
		m_signatures.push_back(entry);
	}

	/**
	 * @brief load the signature from the given file. It clears previously loaded chunks
	 *
	 * @param filename file name
	 */
	void load(const std::string &filename) throw()
	{
		SignatureFileHeader header = {0};

		std::ifstream ifs(filename, std::ifstream::in | std::ifstream::ate | std::ifstream::binary);
		uint64_t compressedBlobSize = static_cast<uint64_t>(ifs.tellg()) - sizeof(SignatureFileHeader);
		ifs.seekg(std::ifstream::beg);
		ifs.read(reinterpret_cast<char *>(&header), sizeof(SignatureFileHeader));

		header.magic = be32toh(header.magic);
		header.chunks = be32toh(header.chunks);

		if (header.magic != MAGIC)
			throw SignatureException("invalid magic");

		uint64_t len = sizeof(uint64_t) * header.chunks * 2;

		std::unique_ptr<uint8_t[]> in(new uint8_t[len]);
		std::unique_ptr<uint8_t[]> out(new uint8_t[len]);

		if (!ifs.good() || len == 0)
			throw MalformedFileException("unexpected length");

		ifs.read(reinterpret_cast<char *>(in.get()), compressedBlobSize);

		uint32_t decompressedSize = CompressionService::decompress(in.get(), compressedBlobSize, out.get(), len);

		uint32_t *outPtr = reinterpret_cast<uint32_t *>(out.get());

		m_signatures.clear();
		for (int i = 0; i < header.chunks; i++)
		{
			uint32_t id = 0;
			uint32_t pos = 0;
			uint32_t hash = 0;
			uint32_t size = 0;

			uint32_t *idPtr = outPtr++;
			uint32_t *posPtr = outPtr++;
			uint32_t *hashPtr = outPtr++;
			uint32_t *sizePtr = outPtr++;

			id = be32toh(*idPtr);
			pos = be32toh(*posPtr);
			hash = be32toh(*hashPtr);
			size = be32toh(*sizePtr);

			m_signatures.push_back({id, pos, hash, size});
		}

		ifs.close();
	}

	/**
	 * @brief save the signature in the given file
	 *
	 * @param filename file name
	 */
	void save(const std::string &filename) throw()
	{
		uint64_t len = m_signatures.size() * sizeof(Signature);

		std::unique_ptr<uint8_t[]> in(new uint8_t[len]);
		std::unique_ptr<uint8_t[]> out(new uint8_t[len]);

		SignatureFileHeader header = {htobe32(MAGIC), htobe32(m_signatures.size())};
		std::ofstream ofs(filename, std::ofstream::out | std::ofstream::binary);
		ofs.write(reinterpret_cast<char *>(&header), sizeof(SignatureFileHeader));

		uint32_t *inPtr = reinterpret_cast<uint32_t *>(in.get());

		for (Signature entry : m_signatures)
		{
			entry.id = htobe32(entry.id);
			entry.pos = htobe32(entry.pos);
			entry.hash = htobe32(entry.hash);
			entry.size = htobe32(entry.size);
			std::memcpy(inPtr++, &entry.id, sizeof(entry.id));
			std::memcpy(inPtr++, &entry.pos, sizeof(entry.pos));
			std::memcpy(inPtr++, &entry.hash, sizeof(entry.hash));
			std::memcpy(inPtr++, &entry.size, sizeof(entry.size));
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
			printf("chunk %u id: %u\n", i, entry.id);
			printf("chunk %u pos: %u\n", i, entry.pos);
			printf("chunk %u hash: %u\n", i, entry.hash);
			printf("chunk %u size: %u\n", i++, entry.size);
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
	 * @brief overloading of the subscript operator
	 * 
	 * @param pos 
	 * @return Signature& 
	 */
	Signature &operator[](size_t pos) {
		return m_signatures[pos];
	}

	template <class Comparator>
	void sort(const Comparator comp) {
		std::sort(m_signatures.begin(), m_signatures.end(), comp);
	}

	/**
	 * @brief overloading of the subscript operator
	 * 
	 * @param pos 
	 * @return Signature const& 
	 */
	Signature const &operator[](size_t pos) const {
		return m_signatures[pos];
	}

	/**
	 * @brief returns the number of signatures
	 * 
	 * @return uint32_t 
	 */
	uint32_t size() {
		return m_signatures.size();
	}

private:
	std::vector<Signature> m_signatures;

	static constexpr uint32_t MAGIC = 0xC000FFEE;
};

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

struct FileHandle
{
	uint64_t size;
	std::unique_ptr<uint8_t[]> data;
};

class FileService
{
public:
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

enum class DeltaCommand {
	DelChunk,
	AddChunk,
};

struct Delta {
	uint32_t id;
	uint32_t command;
	uint32_t pos;
	uint32_t size;
	uint8_t  *data;
};

struct DeltaFileHeader {
	uint32_t magic;
	uint32_t deltas;
	uint32_t len;
};

class DeltaFile
{
public:

	DeltaFile() { }

	DeltaFile(const std::string &filename, const std::string &sigFilename) throw () {
		signatures.load(sigFilename);
		fileHandle = FileService::load(filename);
	}

	~DeltaFile() { }

	void generateDeltas() {
		uint64_t offset = 0; 
		uint64_t len = fileHandle.size;
		uint8_t *dataPtr = fileHandle.data.get();
		uint8_t *end = dataPtr + len;
		int64_t diff = 0;
		uint32_t deltaCount = 0;

		for (uint32_t i = 0; i < signatures.size(); i++) {
			len = fileHandle.size - (dataPtr - fileHandle.data.get());
			uint32_t pos = HashService::search(dataPtr, len, signatures[i].hash, signatures[i].size);
			
			if (pos < len) {
				if (pos > 0) {
					printf("adding delta %u offset: %lu size: %u\n", deltaCount, offset, pos);
					Delta delta;
					delta.id = deltaCount;
					delta.pos = offset;
					delta.command = static_cast<uint32_t>(DeltaCommand::AddChunk);
					delta.data = fileHandle.data.get() + offset;
					delta.size = pos;
					deltas.push_back(delta);

					deltaCount++;
				}
				offset += pos;
				printf("found signature %u of %u at pos %lu expected %u\n", i, signatures.size(), offset, signatures[i].pos);
				offset += signatures[i].size;
				dataPtr = fileHandle.data.get() + offset;
			} else {
				Delta delta;
				delta.id = deltaCount;
				delta.pos = signatures[i].pos;
				delta.command = static_cast<uint32_t>(DeltaCommand::DelChunk);
				delta.data = nullptr;
				delta.size = signatures[i].pos;
				deltas.push_back(delta);

				deltaCount++;
			}
		}
	}

	void save(const std::string &filename) throw() {

		uint64_t len = deltas.size() * sizeof(Delta);

		for (Delta entry : deltas)
			if (entry.data)
				len += entry.size;

		std::unique_ptr<uint8_t[]> in(new uint8_t[len]);
		std::unique_ptr<uint8_t[]> out(new uint8_t[len]);

		DeltaFileHeader header = {htobe32(MAGIC), htobe32(deltas.size()), htobe32(len)};
		std::ofstream ofs(filename, std::ofstream::out | std::ofstream::binary);
		ofs.write(reinterpret_cast<char *>(&header), sizeof(DeltaFileHeader));

		uint32_t *inPtr = reinterpret_cast<uint32_t *>(in.get());

		for (Delta entry : deltas) {
			uint32_t len = entry.size;
			DeltaCommand cmd = static_cast<DeltaCommand>(entry.command);
			
			entry.id = htobe32(entry.id);
			entry.command = htobe32(entry.command);
			entry.pos = htobe32(entry.pos);
			entry.size = htobe32(entry.size);
			std::memcpy(inPtr++, &entry.id, sizeof(entry.id));
			std::memcpy(inPtr++, &entry.command, sizeof(entry.command));
			std::memcpy(inPtr++, &entry.pos, sizeof(entry.pos));
			std::memcpy(inPtr++, &entry.size, sizeof(entry.size));
			
			if (cmd == DeltaCommand::AddChunk) {
				std::memcpy(inPtr, entry.data, len);
				uint8_t *tmp = reinterpret_cast<uint8_t *>(inPtr);
				tmp += len;
				inPtr = reinterpret_cast<uint32_t *>(tmp);
			} else if (cmd == DeltaCommand::DelChunk) {
				inPtr += 2;
			}
		}

		uint64_t compressedSize = CompressionService::compress(in.get(), len, out.get(), len);
		ofs.write(reinterpret_cast<const char *>(out.get()), compressedSize);

		deltas.clear();
		ofs.close();
	}

	void load(const std::string &filename) throw()
	{
		DeltaFileHeader header = {0};

		std::ifstream ifs(filename, std::ifstream::in | std::ifstream::ate | std::ifstream::binary);
		uint64_t compressedBlobSize = static_cast<uint64_t>(ifs.tellg()) - sizeof(DeltaFileHeader);
		ifs.seekg(std::ifstream::beg);
		ifs.read(reinterpret_cast<char *>(&header), sizeof(DeltaFileHeader));

		header.magic = be32toh(header.magic);
		header.deltas = be32toh(header.deltas);
		header.len = be32toh(header.len);

		if (header.magic != MAGIC)
			throw DeltaException("invalid magic");

		std::unique_ptr<uint8_t[]> in(new uint8_t[header.len]);
		std::unique_ptr<uint8_t[]> out(new uint8_t[header.len]);

		if (!ifs.good() || header.len == 0)
			throw MalformedFileException("unexpected length");

		ifs.read(reinterpret_cast<char *>(in.get()), compressedBlobSize);

		uint32_t decompressedSize = CompressionService::decompress(in.get(), compressedBlobSize, out.get(), header.len);

		uint32_t *outPtr = reinterpret_cast<uint32_t *>(out.get());

		deltas.clear();

		for (int i = 0; i < header.deltas; i++)
		{
			uint32_t id = 0;
			uint32_t command = 0;
			uint32_t pos = 0;
			uint32_t size = 0;
			uint8_t *data = nullptr;

			uint32_t *idPtr = outPtr++;
			uint32_t *commandPtr = outPtr++;
			uint32_t *posPtr = outPtr++;
			uint32_t *sizePtr = outPtr++;

			id = be32toh(*idPtr);
			command = be32toh(*commandPtr);
			pos = be32toh(*posPtr);
			size = be32toh(*sizePtr);
			data = nullptr;

			if (command == static_cast<uint32_t>(DeltaCommand::AddChunk)) {
				data = std::make_unique<uint8_t[]>(size).get();
				std::memcpy(data, outPtr, size);
				uint8_t *tmp = reinterpret_cast<uint8_t *>(outPtr);
				tmp += size;
				outPtr = reinterpret_cast<uint32_t *>(tmp);
			} else if (command == static_cast<uint32_t>(DeltaCommand::DelChunk)) {
				outPtr += 2;
			}

			deltas.push_back({id, command, pos, size, data});
		}

		ifs.close();
	}

	void print()
	{
		uint32_t i = 0;
		for (Delta entry : deltas)
		{
			printf("delta %u id: %u\n", i, entry.id);
			printf("delta %u command: %u\n", i, entry.command);
			printf("delta %u pos: %u\n", i, entry.pos);
			printf("delta %u size: %u\n", i, entry.size);
			printf("delta %u data: %p\n", i++, entry.data);
		}
	}

private:
	SignatureFile signatures;
	FileHandle    fileHandle;
	std::vector<Delta> deltas;

	static constexpr uint32_t MAGIC = 0xDEADBEEF;
};

class BackupService {
public:
	static void backup(const std::string &fileVer1, const std::string &fileVer2, uint32_t chunckSize) {
		FileHandle fileHandle1 = FileService::load(fileVer1);
		FileHandle fileHandle2 = FileService::load(fileVer2);

		std::unique_ptr<std::vector<Signature>> signatures = HashService::getSignatures(fileHandle1.data.get(), fileHandle1.size, chunckSize);

		printf("creating signature file\n");
		SignatureFile sig(*signatures.get());
		printf("saving signature file to disk\n");
		sig.save(fileVer1 + ".sig.bin");

		printf("creating delta file\n");
		DeltaFile file(fileVer2, fileVer1 + ".sig.bin");
		file.generateDeltas();

		printf("saving delta file to disk\n");
		file.save(fileVer2 + ".deltas.bin");
	}
};

static constexpr uint32_t CHUNKSIZ = 0xFF;

int main(int argc, const char **argv)
{
	BackupService::backup("starwars_a_new_hope.txt", "starwars_a_new_hope_modified.txt", CHUNKSIZ);
}
