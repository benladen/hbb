#pragma once
#if defined _WIN32 && !defined _WIN32_WINNT
	#define _WIN32_WINNT 0x0501
#endif

#define NOCRYPT //For minizip
#define FILE_CHUNK_SIZE 3670016

class Server;
class Client;
class Http;
class Database;

struct categoryItem;
struct fileListData;
struct rawImage;

#include <iostream>
#include <string>
#include <ctime>
#include <cmath>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <zlib.h>
#if __APPLE__
    #include "libpng.framework/Headers/png.h"
#else
	#include <png.h>
#endif
#include "unzip.h"
#include "sqlite3.h"
#include "database.h"
#include "server.h"
#include "client.h"
#include "http.h"

struct categoryItem {
    char id;
    std::string name;
};

struct fileListData {
    uint32_t fileId;
    bool isDir;
    std::string filename;
    uint64_t fileSize;
    uint32_t CRC;
};

struct rawImage {
	unsigned int width;
	unsigned int height;
	unsigned char bpp;
	char *data;
	size_t dataSize;
};

extern bool testing;
extern boost::asio::io_service ioService;

template <typename T> void print(int level, T text, std::string eol = "\n", bool printTimeLevel = true);
std::string getSetting(std::string setting, std::string default_value);
void addDataToVector(std::vector<unsigned char> &v, const void *data, const size_t len);
struct rawImage* readPNG(char *data, size_t size);
void addAlphaChannelToImage(struct rawImage *img);
