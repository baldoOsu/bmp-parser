// bmp parser.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "bmp.h"

#include <iostream>
#include <fstream>

using std::cout;
using std::cerr;
using std::endl;
using std::string;

namespace offsets {
    constexpr std::ptrdiff_t Header = 0x00;
    constexpr std::ptrdiff_t FileSize = 0x02;
}



int main()
{

    try {

        BMP bmp("bmpFile.bmp");

        cout << "Size of file: " << bmp.file_header.file_size << endl;
        cout << "Image width: " << bmp.info_header.width << endl;
        cout << "Image height: " << bmp.info_header.height << endl;
        cout << "Image resolution: " << bmp.info_header.bits_per_pixel << endl;
    }
    catch (const std::runtime_error& error)
    {
        //cerr << error.what() << endl;
        throw error;
    }






    

}
