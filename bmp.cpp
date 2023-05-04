#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

// #pragma pack(push, 1) og #pragma pack(pop) er brugt for at signalere til compileren ikke at adde padding
// dette er vigtigt for når man læser og skriver filer ud fra disse structs
#pragma pack(push, 1)
struct BMPFileHeader
{
    uint16_t file_type      { 0x4D42 };     // bmp file signatur ("bm")
    uint32_t file_size      { 0 };          // filens størrelse
    uint32_t reserved       { 0 };          // reserveret, altid 0
    uint32_t data_offset    { 0 };          // offset fra start af fil til data
};

struct BMPInfoHeader
{
    uint32_t    size    { 0 };  // størrelse af info header
    int32_t     width   { 0 };  // bredden af billedet
    int32_t     height  { 0 };  // højden af billedet


    uint16_t    planes              { 1 };      // billedets planer, altid 1
    uint16_t    bits_per_pixel      { 0 };      // bits per pixel, AKA kvalitet
    uint32_t    compression         { 0 };      // type af kompression; 0 = ingen kompression
    uint32_t    image_size          { 0 };      // størrelsen af billedet
    int32_t     x_pixels_per_m      { 0 };      // horisontal resolution:   pixels/meter
    int32_t     y_pixels_per_m      { 0 };      // vertical resolution:     pixels/meter
    uint32_t    colors_used         { 0 };      // antal af brugte farver
    uint32_t    important_colors    { 0 };      // antal af vigtige farver
};

struct BMPColorHeader
{
    uint32_t red_mask{ 0x00ff0000 };                 // Bit mask for rød kanal
    uint32_t green_mask{ 0x0000ff00 };               // Bit mask for grøn kanal
    uint32_t blue_mask{ 0x000000ff };                // Bit mask for blå kanal
    uint32_t alpha_mask{ 0xff000000 };               // Bit mask for alpha kanal
    uint32_t color_space_type[4]{ 's','R','G','B' }; // Color space, default "sRGB" (0x73524742)
    uint32_t unused[16]{ 0 };                        // unused for sRGB
};

struct RGBARawPixel
{
    char r{ 0 };
    char g{ 0 };
    char b{ 0 };
    char a{ 0 };
};

#pragma pack(pop)

struct BMP
{
public:
    BMPFileHeader  file_header;
    BMPInfoHeader  info_header;
    BMPColorHeader color_header;

    std::vector<RGBARawPixel> pixels;

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

    // her loades filen
    void read(const char *fname)
    {
        std::ifstream inp{ fname, std::ios_base::binary };

        if (inp)
        {
            // metadata loades
            inp.read((char*)&file_header, sizeof(file_header));
            if (file_header.file_type != 0x4D42)
            {
                throw std::runtime_error("File is not of format Bitmap");
            }
            inp.read((char*)&info_header, sizeof(info_header));
            inp.read((char*)&color_header, sizeof(color_header));

            // cursor sættes til pixel data offsettet
            inp.seekg(file_header.data_offset);
            
            // al pixel data loades
            for (uint32_t i = 0; i <= info_header.image_size; i += 4)
            {
                RGBARawPixel pixel;
                inp.read((char*)&pixel, sizeof(pixel));
                pixels.push_back(pixel);
                //std::cout << i << " pixels lodaet\n";
            }

            int o = 0;
        }
        else
        {
            throw std::runtime_error("Could not open the input bitmap file");
        }

        inp.close();
    }

    // lille funktion til at kopiere en bmp fil
    void copy(const char* fname)
    {
        std::ofstream outfile{ fname, std::ios::out | std::ios::binary };
        outfile.write((char*)&file_header, sizeof(file_header));
        outfile.write((char*)&info_header, sizeof(info_header));
        outfile.write((char*)&color_header, sizeof(color_header));
        
        for (RGBARawPixel pixel : pixels)
        {
            outfile.write((char*)&pixel, sizeof(pixel));
        }

        outfile.close();
    }

private:

    // Til at omdanne kompression nummer til string
    const char* compressions[7] = {
        "BI_RGB", "BI_RLE8", "BI_RLE4", "BI_BITFIELDS", 
        "BI_JPEG", "BI_PNG", "BI_ALPHABITFIELDS"
    };

};