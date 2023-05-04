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

// funktion til at lave store indianer om til lille indianer
// det er kun variabler der optager mere end 1 byte denne funktion skal bruges på
template <typename T>
void reverse_bytes(T* value);

class PNG
{
public:
	sPNGFileHeader	fheader;
	sChunkHeader	IHDRHeader{ 13, { 'I', 'H', 'D', 'R' }};
	sIHDRChunk		IHDRChunk;

	PNG(const char* fname, BMP bmp)
	{
		reverse_bytes(&this->fheader.signature);

		this->IHDRChunk.width					= bmp.info_header.width;
		this->IHDRChunk.height					= std::abs(bmp.info_header.height);
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
		sChunkHeader IENDHeader{ 0, { 'I', 'E', 'N', 'D' } };

		reverse_bytes(&this->IHDRHeader.length);
		reverse_bytes(&this->IHDRChunk.width);
		reverse_bytes(&this->IHDRChunk.height);

		reverse_bytes(&IDATHeader.length);
		reverse_bytes(&IENDHeader.length);

		crc crc_IHDR	= crc32(0L, Z_NULL, 0);
		crc_IHDR		= crc32(crc_IHDR, (unsigned char*)&this->IHDRChunk, sizeof(this->IHDRChunk));

		crc crc_IDAT	= crc32(0L, Z_NULL, 0);
		crc_IDAT		= crc32(crc_IDAT, (unsigned char*)IDATChunk.data.data(), IDATChunk.data.size());

		reverse_bytes(&crc_IHDR);
		reverse_bytes(&crc_IDAT);


		std::ofstream outfile{ fname, std::ios::out | std::ios::binary };

		outfile.write((char*)&this->fheader, sizeof(this->fheader));

		outfile.write((char*)&this->IHDRHeader, sizeof(this->IHDRHeader));
		outfile.write((char*)&this->IHDRChunk, sizeof(this->IHDRChunk));
		outfile.write((char*)&crc_IHDR, sizeof(crc_IHDR));

		outfile.write((char*)&IDATHeader, sizeof(IDATHeader));
		outfile.write((char*)IDATChunk.data.data(), IDATChunk.data.size());
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
		unsigned char*	in	= new unsigned char[CHUNK];
		unsigned char*	out = new unsigned char[CHUNK];
		int				idx = 0;
		const			int bytes = CHUNK / sizeof(sScanline);

		/* allocate deflate state */
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		ret = deflateInit(&strm, -1);
		if (ret != Z_OK)
			return ret;

		// deflate() køres i en do while loop, således at deflate får en input af størrelsen <= CHUNK
		// hvert gang den køres
		do {
			strm.avail_in = precompressed.scanlines.size() - (idx) > bytes
				? bytes * sizeof(sScanline)
				: (precompressed.scanlines.size() % bytes) * sizeof(sScanline);
			flush = (int)(precompressed.scanlines.size() - idx) < 1 ? Z_FINISH : Z_NO_FLUSH;

			if (flush != Z_FINISH) {
				// memcpy bruges til at kopiere
				memcpy(in, &precompressed.scanlines.data()[idx], strm.avail_in);

				strm.next_in = in;

				/* run deflate() on input until output buffer not full, finish
			   compression if all of source has been read in */
				do {
					strm.avail_out = CHUNK;
					strm.next_out = out;

					flush = Z_SYNC_FLUSH;

					ret = deflate(&strm, flush);    /* no bad return value */
					assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
					
					// hvor mange 
					have = CHUNK - strm.avail_out;

					// her beregnes 
					const void* start	= out;
					const void* end		= out + have;

					for (unsigned char c = *out; out < end ; c = *++out) {
						(*IDATChunk).data.push_back(c);
					}

				// dette kører på tro, håb, og kærlighed
				// så længe man tror det ikke bliver et infinite loop, bliver det ikke en infinite loop
				} while (strm.avail_out == 0);
				assert(strm.avail_in == 0); // al input skal være brugt

				idx += bytes;
			}

		// færdig når den har været igennem alle scanlines
		} while (flush != Z_FINISH);

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
				auto b = i * width + x;
				scanline.rawPixels[x] = rawPixels.data()[i * width + x];
				//std::cout << "x is " << x << "\n";
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

// Helper function to reverse bytes of integer types
template <typename T>
void reverse_bytes(T* value) {
	T result = 0;
	size_t size = sizeof(T);

	for (size_t i = 0; i < size; ++i) {
		result <<= 8;
		result |= (*value & 0xFF);
		*value >>= 8;
	}
	*value = result;
}