#pragma once

#include <vector>
#include <string>
#include <Delta.h>
#include <cstdint>
#include <FileService.h>
#include <SignatureFile.h>

struct DeltaFileHeader {
	uint32_t magic;
	uint32_t deltas;
	uint32_t len;
};

class OrderDeltaById
{
public:
    inline bool operator() (const Delta& d1, const Delta& d2)
    {
        return (d1.id < d2.id);
    }
};

class OrderDeltaByCommand
{
public:
    inline bool operator() (const Delta& d1, const Delta& d2)
    {
        return (d1.command < d2.command);
    }
};

class OrderDeltaByPos
{
public:
    inline bool operator() (const Delta& d1, const Delta& d2)
    {
        return (d1.pos < d2.pos);
    }
};

class OrderDeltaBySize
{
public:
    inline bool operator() (const Delta& d1, const Delta& d2)
    {
        return (d1.size < d2.size);
    }
};

class DeltaFile
{
public:

	DeltaFile() { }

	DeltaFile(const std::string &filename, const std::string &sigFilename) throw ();

	~DeltaFile() { }

    /**
     * @brief generate delta chunks in memory
     * 
     */
	void generateDeltas();

    /**
     * @brief save delta chunks in a file
     * 
     * @param filename 
     */
	void save(const std::string &filename) throw();

    /**
     * @brief load delta chunks from a file
     * 
     * @param filename 
     */
	void load(const std::string &filename) throw();

    /**
     * @brief print delta chunks
     * 
     */
	void print();

    /**
     * @brief clear delta chunks
     * 
     */
	void clear();

	/**
	 * @brief overloading of the subscript operator
	 * 
	 * @param pos 
	 * @return Delta& 
	 */
	Delta &operator[](size_t pos);

	/**
	 * @brief overloading of the subscript operator
	 * 
	 * @param pos 
	 * @return Signature const& 
	 */
	Delta const &operator[](size_t pos) const;

    /**
     * @brief return the number of the delta chunks
     * 
     * @return uint32_t 
     */
	uint32_t size() const;

    /**
     * @brief sort the delta chunks by different Delta attributes
     * 
     * @tparam Comparator 
     * @param comp 
     */
	template <class Comparator>
	void sort(const Comparator comp);

private:
	SignatureFile signatures;
	FileHandle    fileHandle;
	std::vector<Delta> deltas;
    std::unique_ptr<uint8_t []> deltaBuffer;

	static constexpr uint32_t MAGIC = 0xDEADBEEF;
};
