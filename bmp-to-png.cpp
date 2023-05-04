// bmp parser.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "bmp.h"
#include "png.h"

#include <iostream>
#include <fstream>

using std::cout;
using std::cerr;
using std::endl;
using std::string;

int main()
{

    try {
        string fileName;// = std::string("bmpFile.bmp");

        std::cout << "Convert .bmp file to .png" << std::endl;
        cout << "Enter file name: ";

        std::cin >> fileName;
        

        cout << endl;

        BMP bmp(const_cast<char*>(fileName.c_str()));

        cout << "File size: " << bmp.file_header.file_size << " bytes" << endl;
        cout << "Image width: " << bmp.info_header.width << "px" << endl;
        cout << "Image height: " << bmp.info_header.height << "px" << endl;
        cout << "Image length (raw bitmap data): " << bmp.info_header.image_size << " bytes" << endl;
        cout << "Color Depth: " << bmp.info_header.bits_per_pixel << "bit" << endl;
        cout << "Compression: " << bmp.getCompression() << endl << endl;

        PNG png("outfile.png", bmp);
        
    }
    catch (const std::runtime_error& error)
    {
        cerr << error.what() << endl;
    }

    system("pause");
}
