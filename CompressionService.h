#pragma once

#include <zlib.h>
#include <cstdint>

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
	 * @return uint64_t
	 */
	static uint64_t compress(const uint8_t *in, uint64_t in_len, const uint8_t *out, uint64_t max_out_len)
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
	 * @return uint64_t
	 */
	static uint64_t decompress(const uint8_t *in, uint64_t in_len, uint8_t *out, uint64_t max_out_len)
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