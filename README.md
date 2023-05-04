# BMP-to-PNG converter

Currently the bitmaps that this program is able to convert are very limited, only `bmpFile.bmp` works so far. This is due to the program not being able to handle any types of compression, filtering, interlace methods and bpps. The biggest drawback is that bitmap width has to be known, and SCANLINE_LEN must be set to it in `png.cpp`.

## Get started

**Compile:**

1. Download and build as a static library [zlib](https://www.zlib.net/)
2. Add zlib to compiler & linker includes
3. Ready to go
