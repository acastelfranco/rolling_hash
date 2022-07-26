#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <fstream>
#include <iostream>
#include <Signature.h>

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

	SignatureFile(const std::vector<Signature> &in);

	virtual ~SignatureFile() {}

    /**
     * @brief append an entry to the signature file
     * 
     * @param entry 
     */
	void append(const Signature &entry);

	/**
	 * @brief load the signature from the given file. It clears previously loaded chunks
	 *
	 * @param filename file name
	 */
	void load(const std::string &filename) throw();

	/**
	 * @brief save the signature in the given file
	 *
	 * @param filename file name
	 */
	void save(const std::string &filename) throw();

	/**
	 * @brief print the signature content, excluding the header
	 *
	 */
	void print();

	/**
	 * @brief clear signatures vector
	 * 
	 */
	void clear();

	/**
	 * @brief overloading of the subscript operator
	 * 
	 * @param pos 
	 * @return Signature& 
	 */
	Signature &operator[](size_t pos);

	/**
	 * @brief overloading of the subscript operator
	 * 
	 * @param pos 
	 * @return Signature const& 
	 */
	Signature const &operator[](size_t pos) const;

    /**
     * @brief function to sort the entries of the signature file
     * 
     * @tparam Comparator 
     * @param comp 
     */
	template <class Comparator>
	void sort(const Comparator comp);

	/**
	 * @brief returns the number of signatures
	 * 
	 * @return uint32_t 
	 */
	uint32_t size();

private:
	std::vector<Signature> m_signatures;

	static constexpr uint32_t MAGIC = 0xC000FFEE;
};