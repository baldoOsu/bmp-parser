#include <cstdint>
#include <fstream>
#include <iostream>

#pragma pack(push, 1)
struct BMPFileHeader {
    uint16_t file_type{0x4D42};       // signature of the file = "bm"
    uint32_t file_size{ 0 };          // size of file
    uint32_t reserved{ 0 };           // reserved, always =0
    uint32_t data_offset{ 0 };        // offset from beginning to file to beginning of bitmap data
};

struct BMPInfoHeader {
    uint32_t size{ 0 };  // size of info header
    int32_t width{ 0 };  // horizontal width of image
    int32_t height{ 0 }; // vertical height of image


    uint16_t planes{ 1 };           // planes of image, is always 1
    uint16_t bits_per_pixel{ 0 };   // bits per pixel, ÁKA quality
    uint32_t compression{ 0 };      // type of compression; 0 = no compression
    uint32_t image_size{ 0 };       // size of image
    int32_t x_pixels_per_m{ 0 };    // horisontal resolution: pixels/meter
    int32_t y_pixels_per_m{ 0 };    // vertical resolution: pixels/meter
    uint32_t colors_used{ 0 };      // number of used colors
    uint32_t important_colors{ 0 }; // number of important colors
};

struct BMPColorHeader {
    uint32_t red_mask{ 0x00ff0000 };                 // Bit mask for red channel
    uint32_t green_mask{ 0x0000ff00 };               // Bit mask for green channel
    uint32_t blue_mask{ 0x000000ff };                // Bit mask for blue channel
    uint32_t alpha_mask{ 0xff000000 };               // Bit mask for alpha channel 
    uint32_t color_space_type[4]{ 's','R','G','B' }; // Default "sRGB" (0x73524742)
    uint32_t unused[16]{ 0 };                        // unused for sRGB
};

#pragma pack(pop)

struct BMP {
public:
    BMPFileHeader  file_header;
    BMPInfoHeader  info_header;
    BMPColorHeader color_header;

    const char* getCompression() { return compressions[info_header.compression]; }


    BMP(const char *fname)
    {
        try 
        {
            read(fname);
        }
        catch (const std::runtime_error& error)
        {
            throw error;
        }
    }

    void read(const char *fname)
    {
        std::ifstream inp{ fname, std::ios_base::binary };

        if (inp)
        {
            inp.read((char*)&file_header, sizeof(file_header));
            if (file_header.file_type != 0x4D42)
            {
                throw std::runtime_error("File is not of format Bitmap");
            }
            inp.read((char*)&info_header, sizeof(info_header));

        }
        else
        {
            throw std::runtime_error("Could not open the input bitmap file");
        }

        inp.close();
    }
private:

    // For simplicity, this does not support Windows Metafile compression
    const char* compressions[7] = {
        "BI_RGB", "BI_RLE8", "BI_RLE4", "BI_BITFIELDS", 
        "BI_JPEG", "BI_PNG", "BI_ALPHABITFIELDS"
    };

};