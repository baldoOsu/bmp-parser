#include <fstream>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <cstdlib>
#include <cstdint>

#include "bmp.h"
#include "zlib.h"

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define CHUNK 262144 // 256K bytes


#pragma pack(push, 1)
struct sPNGFileHeader
{
	uint64_t	signature{ 0x89504E470D0A1A0A };
};

struct sChunkHeader
{
	uint32_t	length { 0 };
	char		type[4]{0,0,0,0};
};

struct sIHDRChunk
{
	int32_t		width{ 0 };
	int32_t		height{ 0 };
	uint8_t		bit_depth{ 0 };
	uint8_t		color_type{ 0 };
	uint8_t		compression_method{ 0 };
	uint8_t		filter_method{ 0 };
	uint8_t		interlace_method{ 0 };
};

struct sScanline
{
	uint8_t					filter_type{ 0 };
	RawPixel				rawPixels[200];
};

// billede data før compression
struct sIDATChunk_precompression
{
	std::vector<sScanline>	scanlines{};
};

// billede data efter compresison
struct sIDATChunk
{
	std::vector<unsigned char>	data {};
};

typedef uint32_t crc;

#pragma pack(pop)

class PNG
{
public:
	sPNGFileHeader	fheader;
	sChunkHeader	IHDRHeader{ 13, { 'I', 'H', 'D', 'R' }};
	sIHDRChunk		IHDRChunk;

	PNG(const char* fname, BMP bmp)
	{
		this->IHDRChunk.width					= bmp.info_header.width;
		this->IHDRChunk.height					= bmp.info_header.height;
		this->IHDRChunk.bit_depth				= 8; // 8 bits per farve kanal
		this->IHDRChunk.color_type				= 6; // RGBA
		this->IHDRChunk.interlace_method		= 0; // ingen interlacing


		write(fname, bmp);
	}

	void write(const char* fname, BMP bmp)
	{

		sIDATChunk_precompression precompressed = preparePixels(bmp.pixels);

		sIDATChunk IDATChunk;
		generateIDATChunk(precompressed, &IDATChunk);

		sChunkHeader IDATHeader{ IDATChunk.data.size(), { 'I', 'D', 'A', 'T' } };
		sChunkHeader IENDHeader{ 0, { 'I', 'E', 'N', 'D '} };

		crc crc_IHDR	= crc32(0L, Z_NULL, 0);
		crc_IHDR		= crc32(crc_IHDR, (unsigned char*)&this->IHDRChunk, sizeof(this->IHDRChunk));

		crc crc_IDAT	= crc32(0L, Z_NULL, 0);
		crc_IDAT		= crc32(crc_IDAT, (unsigned char*)&IDATChunk, IDATChunk.data.size() * sizeof(char[CHUNK]));

		std::ofstream outfile{ fname, std::ios::out | std::ios::binary };

		outfile.write((char*)&this->fheader, sizeof(this->fheader));

		outfile.write((char*)&this->IHDRHeader, sizeof(this->IHDRHeader));
		outfile.write((char*)&this->IHDRChunk, sizeof(this->IHDRChunk));
		outfile.write((char*)&crc_IHDR, sizeof(crc_IHDR));

		outfile.write((char*)&IDATHeader, sizeof(IDATHeader));
		outfile.write((char*)(IDATChunk.data.data()), IDATChunk.data.size() * sizeof(unsigned char[CHUNK]));
		outfile.write((char*)&crc_IDAT, sizeof(crc_IDAT));

		outfile.write((char*)&IENDHeader, sizeof(IENDHeader));

		outfile.close();
	}

private:
	int generateIDATChunk(sIDATChunk_precompression precompressed, sIDATChunk* IDATChunk)
	{
		// https://www.zlib.net/zlib_how.html
		int ret, flush;
		unsigned have;
		z_stream strm;
		unsigned char in[CHUNK];
		unsigned char out[CHUNK];
		unsigned int  idx = 0;

		/* allocate deflate state */
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		ret = deflateInit(&strm, -1);
		if (ret != Z_OK)
			return ret;

		/* compress until end of file */
		do {
			strm.avail_in = precompressed.scanlines.size() - CHUNK * (idx + 1) > CHUNK
				? CHUNK
				: precompressed.scanlines.size() - CHUNK * (idx + 1);

			flush = precompressed.scanlines.size() - CHUNK * (idx + 1) < 1 ? Z_FINISH : Z_NO_FLUSH;

			memcpy(in, precompressed.scanlines.data()[CHUNK * idx].filter_type + precompressed.scanlines.data()[CHUNK * idx].rawPixels, CHUNK);

			strm.next_in = in;

			/* run deflate() on input until output buffer not full, finish
		   compression if all of source has been read in */
			do {
				strm.avail_out = CHUNK;
				strm.next_out = out;
				
				ret = deflate(&strm, flush);    /* no bad return value */
				assert(ret != Z_STREAM_ERROR);  /* state not clobbered */

				have = CHUNK - strm.avail_out;
				/*if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
					(void)deflateEnd(&strm);
					return Z_ERRNO;
				}*/
				(*IDATChunk).data.push_back((unsigned char)out);


			} while (strm.avail_out == 0);
			assert(strm.avail_in == 0);     /* all input will be used */

			idx++;

			/* done when last data in file processed */
		} while (flush != Z_FINISH);
		assert(ret == Z_STREAM_END);        /* stream will be complete */

		/* clean up and return */
		(void)deflateEnd(&strm);
		return Z_OK;

		return 0;
	}

	sIDATChunk_precompression preparePixels(std::vector<RawPixel> rawPixels)
	{
		sIDATChunk_precompression precompressed;
		// af en eller anden grund ændrer højden og bredden sig med tiden???????
		// så de bliver lagret her
		uint32_t height = std::abs(this->IHDRChunk.height);
		uint32_t width = std::abs(this->IHDRChunk.width);
		for (int i = 0; i < height; i++)
		{
			sScanline scanline;
			for (int x = 0; x < width; x++)
			{
				scanline.rawPixels[i * width + x] = rawPixels.data()[i * width + x];
			}
			precompressed.scanlines.push_back(scanline);
		}

		return precompressed;
	}
//crc generateCRC()
//{
//	crc checksum;
//
//	return checksum;
//}
};

