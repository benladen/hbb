#include "main.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

template void print<std::string>(int level, std::string text, std::string eol, bool printTimeLevel);
template void print<char*>(int level, char *text, std::string eol, bool printTimeLevel);
template void print<unsigned char>(int level, unsigned char text, std::string eol, bool printTimeLevel);
template void print<int>(int level, int text, std::string eol, bool printTimeLevel);
template void print<unsigned int>(int level, unsigned int text, std::string eol, bool printTimeLevel);
template void print<unsigned long>(int level, unsigned long text, std::string eol, bool printTimeLevel);

struct pngReadInfo {
    char *data;
	size_t size;
    size_t pos;
};

static void _pngReadFn(png_structp structPtr, png_bytep data, png_size_t size);

static boost::property_tree::ptree configFile;

bool testing = false;
boost::asio::io_service ioService;

int main(int, char**) {
	unsigned short port = 0;
    unsigned short httpPort = 0;
    try {
        boost::property_tree::ini_parser::read_ini("config.ini", configFile);
    }
    catch (...) {
        print(2, "config.ini not found.");
        return 1;
    }
	if (getSetting("Config.Testing", "0") == "1") {
		testing = true;
	}
	port = std::stoi(getSetting("Config.Port", "40111"));
    httpPort = std::stoi(getSetting("Config.HTTP_Port", "40112"));
    
    try {
        Server *srv = new Server(port, httpPort);
        ioService.run();
        delete srv;
    }
    catch (std::exception const &err) {
        print(2, err.what());
    }
    catch (...) {
        print(2, "Unknown error.");
    }
	return 0;
}

template <typename T> 
void print(int level, T text, std::string eol, bool printTimeLevel) {
	if (level == 4 && testing == false) {
		return;
	}
	if (printTimeLevel) {
		std::time_t time = std::time(0);
		#ifdef _WIN32
			struct tm ti;	
			struct tm *lt = &ti;
			localtime_s(&ti, &time);
		#else
			struct tm *lt = localtime(&time);
		#endif
		if (lt->tm_hour < 10) {
			std::cout << "0";
		}
		std::cout << lt->tm_hour << ":";
		if (lt->tm_min < 10) {
			std::cout << "0";
		}
		std::cout << lt->tm_min << ":";
		if (lt->tm_sec < 10) {
			std::cout << "0";
		}
		std::cout << lt->tm_sec << " ";
	}
	if (!printTimeLevel) {
		std::cout << text << eol;
	}
	else {
		if (level == 0) {
			std::cout << "[INFO]" << " " << text << eol;
		}
		else if (level == 1) {
			std::cout << "[ERROR]" << " " << text << eol;
		}
		else if (level == 2) {
			std::cout << "[FATAL ERROR]" << " " << text << eol;
		}
		else if (level == 3) {
			std::cout << "[WARNING]" << " " << text << eol;
		}
		else if (level == 4) {
			std::cout << "[DEBUG]" << " " << text << eol;
		}
		else {
			std::cout << "[UNKNOWN]" << " " << text << eol;
		}
	}
}

std::string getSetting(std::string setting, std::string default_value) {
    try {
        return configFile.get<std::string>(setting);
    }
    catch (...) {
        return default_value;
    }
}

void addDataToVector(std::vector<unsigned char> &v, const void *data, const size_t len) {
    const unsigned char *ptr = static_cast<const unsigned char*>(data);
    v.insert(v.end(), ptr, ptr + len);
}

struct rawImage* readPNG(char *data, size_t size) {
	struct rawImage *newRawImg = (struct rawImage*)calloc(1, sizeof(struct rawImage));
	png_byte colorType;
	png_structp png_ptr;
	png_infop info_ptr;
	int number_of_passes;
	png_bytep *row_pointers;
	unsigned int y;
	struct pngReadInfo readInfo;
	readInfo.data = data;
	readInfo.size = size;
	readInfo.pos = 8;
    
    if (newRawImg == NULL) {
        return NULL;
    }

	if (png_sig_cmp((png_bytep)data, 0, 8)) {
		free(newRawImg);
		return NULL;
	}

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		free(newRawImg);
		png_destroy_read_struct(&png_ptr, (png_infopp)0, (png_infopp)0);
		return NULL;
	}
	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		free(newRawImg);
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		return NULL;
	}
	if (setjmp(png_jmpbuf(png_ptr))) {
		free(newRawImg);
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		return NULL;
	}
	png_set_read_fn(png_ptr, (png_voidp)&readInfo, _pngReadFn);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);

	newRawImg->width = (unsigned int)(png_get_image_width(png_ptr, info_ptr));
	newRawImg->height = (unsigned int)(png_get_image_height(png_ptr, info_ptr));
	colorType = png_get_color_type(png_ptr, info_ptr);
	if (colorType == PNG_COLOR_TYPE_RGBA) {
		newRawImg->bpp = png_get_bit_depth(png_ptr, info_ptr) * 4;
	}
	else if (colorType == PNG_COLOR_TYPE_RGB) {
		newRawImg->bpp = png_get_bit_depth(png_ptr, info_ptr) * 3;
	}
    else if (colorType == PNG_COLOR_TYPE_PALETTE) {
        png_bytep trans_alpha = NULL;
        int num_trans = 0;
        png_color_16p trans_color = NULL;
        png_get_tRNS(png_ptr, info_ptr, &trans_alpha, &num_trans, &trans_color);
        
        if (trans_alpha != NULL) {
            newRawImg->bpp = png_get_bit_depth(png_ptr, info_ptr) * 4;
        }
        else {
            newRawImg->bpp = png_get_bit_depth(png_ptr, info_ptr) * 3;
        }
        png_set_palette_to_rgb(png_ptr);
    }
	else {
		newRawImg->bpp = 0;
		free(newRawImg);
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		return NULL;
	}

	number_of_passes = png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr, info_ptr);

	if (setjmp(png_jmpbuf(png_ptr))) {
		free(newRawImg);
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		return NULL;
	}

	row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * newRawImg->height);
	if (row_pointers == NULL) {
		free(newRawImg);
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		return NULL;
	}
	for (y = 0; y<newRawImg->height; y++) {
		row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png_ptr,info_ptr));
	}
	png_read_image(png_ptr, row_pointers);
	newRawImg->dataSize = newRawImg->height * newRawImg->width * (newRawImg->bpp/8);
	newRawImg->data = (char*)calloc(newRawImg->dataSize, sizeof(char));
	if (newRawImg->data == NULL) {
		free(newRawImg);
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		return NULL;
	}
	for (y = 0; y<newRawImg->height; y++) {
		memcpy(&newRawImg->data[(newRawImg->width * (newRawImg->bpp/8)) * y],
			row_pointers[y],newRawImg->width*(newRawImg->bpp/8));
	}
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
    for (y = 0; y<newRawImg->height; y++) {
        free(row_pointers[y]);
    }
	free(row_pointers);
	return newRawImg;
}

void addAlphaChannelToImage(struct rawImage *img) {
    size_t srcPos;
    size_t dstPos;
    char *newImageData;
    if (img->bpp == 24) {
        newImageData = (char*)calloc(1, (img->width*img->height)*4);
        dstPos = 0;
        for (srcPos = 0; srcPos < img->dataSize; srcPos += 3) {
            /*memcpy(newImageData+dstPos, img->data+srcPos, 3);*/
			newImageData[dstPos] = img->data[srcPos];
			newImageData[dstPos+1] = img->data[srcPos+1];
			newImageData[dstPos+2] = img->data[srcPos+2];
            newImageData[dstPos+3] = -1;
            dstPos += 4;
        }
        free(img->data);
        img->bpp = 32;
        img->data = newImageData;
        img->dataSize = (img->width*img->height)*4;
    }
}

static void _pngReadFn(png_structp structPtr, png_bytep data, png_size_t size) {
	struct pngReadInfo *readInfo = (struct pngReadInfo*)png_get_io_ptr(structPtr);
	char *src = &readInfo->data[readInfo->pos];
	if (readInfo->pos+size > readInfo->size) {
		print(4, "_pngReadFn error.");
	}
	else {
		memcpy(data, src, size);
		readInfo->pos += size;
	}
}
