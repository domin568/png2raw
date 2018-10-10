
#include <iostream>
#include <cstddef>
#include <fstream>
#include <inttypes.h>
#include <vector>
#include <map>
#include <exception>
#include <zlib.h>
#include <ctime>

class PNG
{
	private:

	struct ihdr_data
	{
		int width;
		int height;
		std::uint8_t bitdepth;
		std::uint8_t colortype;
		std::uint8_t filter;
		std::uint8_t interlace;
	};
	struct chunk
	{
		int size;
		char name [4];
		void * data;
		//int crc;
	};

	std::vector <uint8_t> rawdata;
	std::vector <uint8_t> IDAT;
	std::vector <uint8_t> IDATraw;
	std::map <int,std::vector<uint8_t>> scanlines;
	std::map <int,std::vector<uint8_t>> rawScanlines;
	std::streampos sizeRaw;

	int bytesPerPixel; 
	std::ifstream pngfile;
	unsigned long pos;
	ihdr_data * IHDR;

	unsigned char paeth (int a,  int b,  int c)
	{
		int p = (a + b - c);
		int pa = abs (p - a);
		int pb = abs (p - b);
		int pc = abs (p - c);
		//std::cout << std::hex;
		//std::cout << "p " << (int)p << " pa " << (int)pa << " pb " << (int)pb << " pc " << (int)pc << std::endl;
		//std::cout << "pa <= pb : " << (pa <= pb) << " pa <= pc " << (pa <= pc) << " pb <= pc " << (pb <= pc )<< std::endl;
		if (pa <= pb && pa <= pc)
		{
			return a;
		}
		if (pb <= pc)
		{
			return b;
		}
		return c;
	}

	int inflateData ()
	{
		if (IHDR->colortype == 2) bytesPerPixel = 3;
		if (IHDR->colortype == 6) bytesPerPixel = 4;

		unsigned long rawBitmapSize = IHDR->width * IHDR->height * bytesPerPixel + IHDR->height;
		
		IDATraw.resize(rawBitmapSize);
		int err = uncompress ((Bytef *)&IDATraw[0], &rawBitmapSize,(Bytef*)IDAT.data(),IDAT.size());
		std::string msg ("");
		switch (err)
	    {
	      case Z_OK:
	      	msg+= "[*] Data succesfully inflated.";
	      	break;

	      case Z_ERRNO:
			msg += "[!] zlib system error.";
			break;

	      case Z_STREAM_ERROR:
			msg += "[!] zlib stream error.";
			break;

	      case Z_DATA_ERROR:
			msg += "[!] zlib data error.";
			break;

	      case Z_MEM_ERROR:
			msg += "[!] zlib memory error.";
			break;

	      case Z_BUF_ERROR:
			msg += "[!] zlib buffer error.";
			break;

		  default:
		  	msg+= "[!] zlib unknown error.";
		  	break;
		}
		std::cout << msg << "\n";
		return err;
	} 

	void createRawBitmap ()
	{
		// scanline contains data inflated
		// rawScanlines contains processed data
		std::vector<uint8_t> temp;
		int scanlineSize = IHDR->width * bytesPerPixel + 1;
		temp.resize (scanlineSize + bytesPerPixel,0); // + bytesPerPixel for compatibility with next scanlines
		rawScanlines[0] = temp;
		scanlines[0] = temp;
		pos = 0; 
		for (int i = 1; i <= IHDR->height; i++) // first scanline is fullfilled with zeros to not check every time if we are out of bounds
		{
			std::vector <uint8_t> scanline;
			scanline.reserve(scanlineSize+bytesPerPixel); // +bytesPerPixel not to check every iter if we are out of bounds
			std::copy (IDATraw.begin()+pos,IDATraw.begin()+pos+scanlineSize,std::back_inserter(scanline));
			for (int j = 0; j < bytesPerPixel; j++)
			{
				scanline.insert(scanline.begin()+1,'\x00'); // insert zeros before every scanline to not check if we are not out of bounds
				rawScanlines[i].push_back('\x00');
			}
			scanlines[i] = scanline;
			pos += scanlineSize;
		}

		// swap with unallocated vector, memory freed
		std::vector<uint8_t>().swap(IDAT);
		std::vector<uint8_t>().swap(IDATraw);
		pos = 0;

		for (int i = 1; i < scanlines.size(); i++)
		{
 			if (scanlines[i][0] == '\x00')
			{
				std::copy (scanlines[i].begin()+1+bytesPerPixel,scanlines[i].begin()+scanlineSize + bytesPerPixel,std::back_inserter(rawScanlines[i]));
				continue;
			}
			
			int j = 4;

			if (bytesPerPixel == 4)
			{
				j = 5;
			}
			for (; j <= scanlineSize; j+= bytesPerPixel)
			{

				int k = j - 1; // for rawScanlines which does not contain scanline type ('\x00' or '\x01' etc)

				uint8_t leftR,leftG,leftB,leftA;
				uint8_t upR,upG,upB,upA;
				uint8_t upLeftR, upLeftG,upLeftB,upLeftA;

				uint8_t r = scanlines[i][j];
				uint8_t g = scanlines[i][j+1];
				uint8_t b = scanlines[i][j+2];
				uint8_t a = scanlines[i][j+3];

				if (bytesPerPixel == 3)
				{
					leftR = rawScanlines[i][k-3];
					leftG = rawScanlines[i][k-2];
					leftB = rawScanlines[i][k-1];
					upLeftR = rawScanlines[i-1][k-3];
					upLeftG = rawScanlines[i-1][k-2];
					upLeftB = rawScanlines[i-1][k-1];
				}
				if (bytesPerPixel == 4)
				{
					leftR = rawScanlines[i][k-4];
					leftG = rawScanlines[i][k-3];
					leftB = rawScanlines[i][k-2];
					leftA = rawScanlines[i][k-1];
					upLeftR = rawScanlines[i-1][k-4];
					upLeftG = rawScanlines[i-1][k-3];
					upLeftB = rawScanlines[i-1][k-2];
					upLeftA = rawScanlines[i-1][k-1];
				}
					upR = rawScanlines[i-1][k];
					upG = rawScanlines[i-1][k+1];
					upB = rawScanlines[i-1][k+2];
					upA = rawScanlines[i-1][k+3];

				if (scanlines[i][0] == '\x01')
				{
					rawScanlines[i].push_back(r+leftR);
					rawScanlines[i].push_back(g+leftG);
					rawScanlines[i].push_back(b+leftB);

					if (bytesPerPixel == 4)
					{
						rawScanlines[i].push_back(a+leftA);
					}
				}
				else if (scanlines[i][0] == '\x02')
				{
					rawScanlines[i].push_back(r + upR);
					rawScanlines[i].push_back(g + upG);
					rawScanlines[i].push_back(b + upB);
					if (bytesPerPixel == 4)
					{
						rawScanlines[i].push_back(a + upA);
					}
				}
				else if (scanlines[i][0] == '\x03')
				{
					rawScanlines[i].push_back (r + (leftR + upR) / static_cast<uint8_t>(2));
					rawScanlines[i].push_back (g + (leftG + upG) / static_cast<uint8_t>(2));
					rawScanlines[i].push_back (b + (leftB + upB) / static_cast<uint8_t>(2));
					if (bytesPerPixel == 4)
					{
						rawScanlines[i].push_back (a + (leftA + upA) / static_cast<uint8_t>(2));
					}
				}
				else if (scanlines[i][0] == '\x04')
				{
					rawScanlines[i].push_back (r + paeth(leftR,upR,upLeftR));
					rawScanlines[i].push_back (g + paeth(leftG,upG,upLeftG));
					rawScanlines[i].push_back (b + paeth(leftB,upB,upLeftB));
					if (bytesPerPixel == 4)
					{
						rawScanlines[i].push_back (a + paeth(leftA,upA,upLeftA));
					}
				}
			}
		}
	}
	bool pngExtractData ()
	{
		unsigned char magic [9];
		memcpy(magic,"\x89\x50\x4e\x47\x0d\x0a\x1a\x0a",8);
		bool magicOk = std::equal (rawdata.begin(),rawdata.begin()+8,magic);
		if (!magicOk)
		{
			std::cout << "[!] Magic value does not match. \n";
			return false;	
		}
		pos += 8;
		chunk * chunkObj;
		do
		{
			chunkObj = reinterpret_cast<chunk*>(&rawdata[pos]);
			chunkObj->size = __builtin_bswap32(chunkObj->size);
			std::cout << "[*] Found "; 
			std::cout.write(chunkObj->name,4); // because there is no null terminator
			std::cout << " chunk.\n";
			if (!strncmp(chunkObj->name,"IHDR",4)) // strncmp because null terminator is not here
			{
				IHDR = reinterpret_cast<ihdr_data*>(&chunkObj->data);
				IHDR->width = __builtin_bswap32 (IHDR->width);
				IHDR->height = __builtin_bswap32 (IHDR->height);
				if (IHDR->colortype == 2)
				{
					std::cout << "[*] Current mode is RGB. \n";
				}
				if (IHDR->colortype == 6)
				{
					std::cout << "[*] Current mode is RGBA. \n";
				}
				if (IHDR->colortype != 2 && IHDR->colortype != 6)
				{
					std::cout << "[!] Unsupported color type (only RGB and RGBA is supported).\n";
					return false;
				}
				std::cout << "[*] PNG file [" << IHDR->width << "x" << IHDR->height << "]\n";
			}

			if (!strncmp(chunkObj->name,"IDAT",4))
			{
				unsigned char * d = reinterpret_cast<unsigned char *>(&chunkObj->data);
				chunkObj->size = chunkObj->size;
				IDAT.reserve(chunkObj->size);
				std::copy(d,d+chunkObj->size,std::back_inserter(IDAT));
			}
			if (!strncmp(chunkObj->name,"IEND",4))
			{
				break;
			}
			pos+=(chunkObj->size + 12); // 12 = chunksize,name,crc

		} while (pos < rawdata.size());

		return true;
	}

	template <class T>
	void saveVectorToFile (std::string path, std::vector<T> vec)
	{
		std::ofstream f (path,std::ios::out | std::ios::binary);
		std::copy (vec.begin(),vec.end(),std::ostream_iterator<T>(f));
		f.close();
	}

	public:
	PNG (const char * path)
	{
		pos = 0;
		try
		{
			pngfile = std::ifstream(path,std::ios::binary); 	
		}
		catch (std::ios_base::failure e)
		{
			std::cout << e.what() << std::endl;
		}
		// Stop eating new lines in binary mode!!!
    	pngfile.unsetf(std::ios::skipws);

		pngfile.seekg(0,std::ios::end);
		sizeRaw = pngfile.tellg();
		pngfile.seekg(0,std::ios::beg);
		rawdata.reserve(sizeRaw);
		// reading contents of png file to rawdata vector
		std::copy(std::istream_iterator<uint8_t>(pngfile),std::istream_iterator<uint8_t>(),std::back_inserter(rawdata));
		if (!pngExtractData())
		{
			throw -1;
		}
		if (inflateData())
		{
			throw -2;
		}

		createRawBitmap();

	}
	void exportRaw (const char * path)
	{
		rawScanlines.erase(rawScanlines.begin());
		std::ofstream rawfile;
		try
		{
			rawfile = std::ofstream (path,std::ios::binary);
		}
		catch (std::ios_base::failure e)
		{
			std::cout << e.what() << std::endl;
		}
		std::for_each(rawScanlines.begin(),rawScanlines.end(), [&](std::pair<const int, std::vector<uint8_t>>& pair)
		{
			std::copy (pair.second.begin()+bytesPerPixel,pair.second.end(),std::ostream_iterator<uint8_t>(rawfile));
		});	
	}
};

int main (int argc, char ** argv)
{
	if (argc < 3)
	{
		std::cout << "[*] Usage ./png2raw <in.png> <out.raw>" << std::endl;
		return -1;
	}

	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

	try
	{
		PNG image (argv[1]);
		image.exportRaw(argv[2]);
	}
	catch (int err)
	{
		std::cout << "[!] Could not export raw bitmap from this png. \n";
		return err;
	}

	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	std::cout << "[*] Operation performed in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << " milliseconds." << std::endl;

	return 0;
}