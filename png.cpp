#include <fstream>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <cstdlib>
#include <cstdint>

#include "bmp.h"
#include "zlib.h"

// til deflate kompression
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

// data chunks til deflate kompression
#define CHUNK 262144 // 256K bytes

// dette her skal ændres for hvert bredde bmp filen man vil konvertere har
#define SCANLINE_LEN 1280


#pragma pack(push, 1)
struct sPNGFileHeader
{
	uint64_t	signature{ 0x89504E470D0A1A0A }; // png signatur
};

// Alle chunks har en header af denne type
struct sChunkHeader
{
	uint32_t	length { 0 };		// Længde af chunk data
	char		type[4]{0,0,0,0};	// typen af chunk
};

struct sIHDRChunk
{
	int32_t		width{ 0 };					// bredden af billede
	int32_t		height{ 0 };				// højden af billede
	uint8_t		bit_depth{ 0 };				// antallet af bits brugt til at repræsentere en enkel kanal
	uint8_t		color_type{ 0 };			// farve typen (f.eks. RGB eller RGBA)
	uint8_t		compression_method{ 0 };	// kompression metode
	uint8_t		filter_method{ 0 };			// filter metode
	uint8_t		interlace_method{ 0 };		// interlace metode
};

struct sScanline
{
	uint8_t					filter_type{ 0 };
	RGBARawPixel				rawPixels[SCANLINE_LEN];
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
		this->IHDRChunk.width					= bmp.info_header.width;
		this->IHDRChunk.height					= std::abs(bmp.info_header.height);

		// programmet virker kun for bitmaps med følgende opsætning:
		this->IHDRChunk.bit_depth				= 8; // 8 bits per farve kanal
		this->IHDRChunk.color_type				= 6; // RGBA
		this->IHDRChunk.interlace_method			= 0; // ingen interlacing

		write(fname, bmp);
	}

	void write(const char* fname, BMP bmp)
	{
		// scanlines bygges
		sIDATChunk_precompression precompressed = preparePixels(bmp.pixels);

		// IDATChunk bygges
		sIDATChunk IDATChunk;
		generateIDATChunk(precompressed, &IDATChunk);

		sChunkHeader IDATHeader{ IDATChunk.data.size(), { 'I', 'D', 'A', 'T' } };
		sChunkHeader IENDHeader{ 0, { 'I', 'E', 'N', 'D' } };

		// alle nødvendige bytes behandles
		reverse_bytes(&this->fheader.signature);

		reverse_bytes(&this->IHDRHeader.length);
		reverse_bytes(&this->IHDRChunk.width);
		reverse_bytes(&this->IHDRChunk.height);

		reverse_bytes(&IDATHeader.length);
		reverse_bytes(&IENDHeader.length);

		// CRC32 beregnes for IHDR og IDAT chunks
		crc crc_IHDR	= crc32(0L, Z_NULL, 0);
		crc_IHDR	= crc32(crc_IHDR, (unsigned char*)&this->IHDRChunk, sizeof(this->IHDRChunk));

		crc crc_IDAT	= crc32(0L, Z_NULL, 0);
		crc_IDAT	= crc32(crc_IDAT, (unsigned char*)IDATChunk.data.data(), IDATChunk.data.size());

		reverse_bytes(&crc_IHDR);
		reverse_bytes(&crc_IDAT);


		// til sidst bliver alt skrevet ud til filen
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

		std::cout << "Converted " << fname << " to .png\n\n";
	}

private:
	// her er hele sovsen
	int generateIDATChunk(sIDATChunk_precompression precompressed, sIDATChunk* IDATChunk)
	{
		// her er der kopieret fra https://www.zlib.net/zlib_how.html, og så tilpasset til eget forbrug
		// virker ikke med store filer
		int ret, flush;
		unsigned have;
		z_stream strm;
		unsigned char*	in  		= new unsigned char[CHUNK];	// disse to arrays bliver allokeret på heap for ikke at fylde 512KB på stack
		unsigned char*	out 		= new unsigned char[CHUNK];
		int		idx 		= 0;
		const int 	pixels_pr_chunk = CHUNK / sizeof(sScanline); // så mange rå pixels kan der være pr CHUNK

		/* allocate deflate state */
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		ret = deflateInit(&strm, -1); // initialisering af deflate stream
		if (ret != Z_OK)
			return ret;

		// deflate() køres i en do while loop, således at deflate får en 
		// input af størrelsen <= CHUNK hver gang den køres
		do {
			// her beregnes hvor mange bytes kan bruges til næste kompression
			strm.avail_in = precompressed.scanlines.size() - (idx) > pixels_pr_chunk
				? pixels_pr_chunk * sizeof(sScanline)
				: (precompressed.scanlines.size() % pixels_pr_chunk) * sizeof(sScanline);

			// flush bestemmes, for at vide om programmet skal gå videre eller ikke
			flush = (int)(precompressed.scanlines.size() - idx) < 1 ? Z_FINISH : Z_NO_FLUSH;

			if (flush != Z_FINISH) {
				// memcpy bruges til at kopiere data fra scanlines til deflate stream input
				try {
					memcpy(in, &precompressed.scanlines.data()[idx], strm.avail_in);
				}
				catch (std::runtime_error err) {
					std::cout << "Memcpy fejlede";
				}

				strm.next_in = in;

				/* run deflate() on input until output buffer not full, finish
			   compression if all of source has been read in */
				do {
					// output buffer størrelse sættes til CHUNK
					strm.avail_out = CHUNK;
					strm.next_out = out;

					// jeg havde nogle få problemer med hvis flush var sat til Z_FINISH
					// derfor sat jeg det til Z_SYNC_FLUSH for at deflate() kører al data igennem på en gang
					flush = Z_SYNC_FLUSH;

					ret = deflate(&strm, flush);    /* no bad return value */
					assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
					
					// hvor mange bytes der er outputtet
					have = CHUNK - strm.avail_out;

					// her beregnes pointers for starten og slutningen af output arrayet
					const unsigned char* start	= out;
					const unsigned char* end	= out + have;

					// bruges til for loop for ikke at ændre på out
					unsigned char* outIterator = out;

					// data i selve IDATChunk bliver nu opdateret
					try {
						for (unsigned char c = *outIterator; outIterator < end; c = *++outIterator) {
							(*IDATChunk).data.push_back(c);
						}
					}
					catch (std::runtime_error err) {
						std::cout << "err";
					}
					

				// dette kører på tro, håb, og kærlighed
				// så længe man tror det ikke bliver et infinite loop, bliver det ikke en infinite loop
				} while (strm.avail_out == 0);
				assert(strm.avail_in == 0); // al input skal være brugt

				idx += pixels_pr_chunk;
			}

		// færdig når den har været igennem alle scanlines
		} while (flush != Z_FINISH);

		/* clean up and return */
		(void)deflateEnd(&strm);
		return Z_OK;

		return 0;
	}

	// scanlines bygges her
	sIDATChunk_precompression preparePixels(std::vector<RGBARawPixel> rawPixels)
	{
		// højden og bredden lagres her, std::abs() bruges fordi tallene er negative nogle gange
		uint32_t height = std::abs(this->IHDRChunk.height);
		uint32_t width = std::abs(this->IHDRChunk.width);

		sIDATChunk_precompression precompressed;
		
		for (int i = 0; i < height; i++)
		{
			sScanline scanline;
			for (int x = 0; x < width; x++)
			{
				// virker kun når antal px per scanline == bredde
				if((x+1) % SCANLINE_LEN == 0)
					precompressed.scanlines.push_back(scanline);

				scanline.rawPixels[x % SCANLINE_LEN] = rawPixels.data()[i * height + x];
				//std::cout << "x is " << x << "\n";
			}
		}

		return precompressed;
	}
};

// Lille hjælper funktion til at konvertere stor indianer (memory) til lille indianer (disk)
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
