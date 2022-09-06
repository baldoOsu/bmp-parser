#include <cstdint>
#include <fstream>
#include <new>

#pragma pack(push, 1)
struct PNGHeader
{
	uint64_t signature{ 0x89504E47040A1A0A }; // blah blah blah

};

struct chunkheader
{
	uint32_t length{ 0 };
	uint32_t name{ 0 };
};

struct chunkdata
{
	uint8_t data[1];
	uint32_t crc{ 0 };
};

struct IHDRChunk
{
	chunkheader *chunkheader;

};

#pragma pack(pop)

struct PNG
{
public:

	PNGHeader PNGHeader;

	PNG(const char* fname)
	{
		try
		{
			write(fname);
		}

		catch (const std::runtime_error& error)
		{
			throw error;
		}
	}

	void write(const char* fname)
	{
		std::ofstream of{ fname, std::ios_base::binary };

		of.write((char*)&PNGHeader, sizeof(PNGHeader));

		
	}
};

#pragma pack(pop)