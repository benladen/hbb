#include "main.h"

Server::Server(unsigned short port, unsigned short httpPort) : acceptor(ioService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
                                                           httpAcceptor(ioService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), httpPort)) {
            
	print(4, "Server::Server");
    serverVersion = "0.1";
    clientVersion = "0.2";
    dataDir = getSetting("Config.DataDir", "./data/");
    db = new Database();
    db->SQLiteOpen("database.sqlite");
    readCategories();
    readArchives();
    srand((unsigned int)(time(NULL)));
    for (int i = 0; i < 25; i++) {
        char c = rand() % 10 + 48;
        httpSessionRandom += c;
    }
    print(0, "Server running.");
    startAccept();
    startAcceptHttp();
}

void Server::startAccept(void) {
    Client* newClient = new Client();
    acceptor.async_accept(newClient->socket, boost::bind(&Server::handleAccept, this,
                            newClient, boost::asio::placeholders::error));
}

void Server::handleAccept(Client *newClient, const boost::system::error_code &error) {
    print(4, "Server handleAccept");
    if (!error) {
        newClient->start(this);
    }
    else {
        delete newClient;
    }
    startAccept();
}

void Server::startAcceptHttp(void) {
    Http* newClient = new Http();
    httpAcceptor.async_accept(newClient->socket, boost::bind(&Server::handleAcceptHttp, this,
                                newClient, boost::asio::placeholders::error));
}

void Server::handleAcceptHttp(Http *newClient, const boost::system::error_code &error) {
    print(4, "Server handleAcceptHttp");
    if (!error) {
        newClient->start(this);
    }
    else {
        delete newClient;
    }
    startAcceptHttp();
}

int Server::readCategories(void) {
    categories.clear();
    std::string setting = getSetting("Config.CategoryList", "");
    if (setting == "") {
        return 1;
    }
    std::vector<std::string> split;
    boost::split(split, setting, boost::is_any_of("|"));
    for (unsigned int i = 0; i < split.size(); i++) {
        const auto j = split[i].find_first_of(':');
        if (std::string::npos != j) {
            char c = (char)(stoi(split[i].substr(0, j)));
            std::string name = split[i].substr(j + 1);
            categories.push_back(categoryItem());
            categories[i].id = c;
            categories[i].name = name;
        }
        else {
            return 2;
        }
    }
    return 0;
}

int Server::readArchives(void) {
    std::vector<std::vector<std::string>> results;
    fileIds.clear();
    itemSafemode.clear();
    results = db->SQLiteExecute("select id,filename from app");
    for (unsigned int i = 0; i < results.size(); i++) {
        std::vector<struct fileListData> fldvec;
        int res;
        unsigned int fileId = 0;
        unsigned int id = stoi(results[i][0]);
        std::string zipName = dataDir + results[i][1];
        unzFile archive = unzOpen64(zipName.c_str());
        unz_global_info64 gi;
        
        if (itemSafemode.count(id) == 0) {
            itemSafemode[id] = 0;
        }
        
        if (archive == NULL) {
            continue;
        }
        
        res = unzGetGlobalInfo64(archive, &gi);
        if (res != UNZ_OK) {
            unzClose(archive);
            print(1, "Failure on unzGetGlobalInfo64");
            continue;
        }
        for (unsigned long i = 0; i < gi.number_entry; i++) {
            unz_file_info64 fileInfo;
            char filename[256];
            struct fileListData fld;
            res = unzGetCurrentFileInfo64(archive, &fileInfo, filename, sizeof(filename), NULL, 0, NULL, 0);
            if (res != UNZ_OK) {
                unzClose(archive);
                print(1, "Failure on unzGetCurrentFileInfo64");
                break;
            }
            fld.fileId = fileId;
            fld.filename = std::string(filename);
            if (fld.filename.back() == '\\' || fld.filename.back() == '/') {
                fld.isDir = true;
            }
            else {
                fld.isDir = false;
            }
            fld.fileSize = fileInfo.uncompressed_size;
            fld.CRC = (uint32_t)(fileInfo.crc);
            std::string fnlower = fld.filename;
            boost::algorithm::to_lower(fnlower);
            if (fnlower.find("eboot.bin") != std::string::npos) {
                /* Check eboot.bin 0x80 */
                std::vector<char> buf(0x81);
                int rc = 0;
                res = unzOpenCurrentFile(archive);
                if (res != UNZ_OK) {
                    print(1, "Failure on unzOpenCurrentFile");
                }
                else {
                    rc = unzReadCurrentFile(archive, &buf[0], (unsigned int)(buf.size()));
                    if (buf[0x80] == 1 || buf[0x80] == 2) {
                        itemSafemode[id] = buf[0x80];
                    }
                    unzCloseCurrentFile(archive);
                }
            }
            fldvec.push_back(fld);
            if ((i+1)<gi.number_entry) {
                res = unzGoToNextFile(archive);
                if (res != UNZ_OK) {
                    print(1, "Failure on unzGoToNextFile");
                    break;
                }
            }
            ++fileId;
        }
        unzClose(archive);
        fileIds[id] = fldvec;
    }
    return 0;
}

int Server::readArchives(std::string fileName) {
    std::vector<std::vector<std::string>> results;
    results = db->SQLiteExecute("select id,filename from app");
    for (unsigned int i = 0; i < results.size(); i++) {
        if (results[i][1] != fileName) {
            continue;
        }
        std::vector<struct fileListData> fldvec;
        int res;
        unsigned int fileId = 0;
        unsigned int id = stoi(results[i][0]);
        std::string zipName = dataDir + results[i][1];
        unzFile archive = unzOpen64(zipName.c_str());
        unz_global_info64 gi;
        
        if (itemSafemode.count(id) == 0) {
            itemSafemode[id] = 0;
        }
        
        if (archive == NULL) {
            continue;
        }
        
        res = unzGetGlobalInfo64(archive, &gi);
        if (res != UNZ_OK) {
            unzClose(archive);
            print(1, "Failure on unzGetGlobalInfo64");
            continue;
        }
        for (unsigned long i = 0; i < gi.number_entry; i++) {
            unz_file_info64 fileInfo;
            char filename[256];
            struct fileListData fld;
            res = unzGetCurrentFileInfo64(archive, &fileInfo, filename, sizeof(filename), NULL, 0, NULL, 0);
            if (res != UNZ_OK) {
                unzClose(archive);
                print(1, "Failure on unzGetCurrentFileInfo64");
                break;
            }
            fld.fileId = fileId;
            fld.filename = std::string(filename);
            if (fld.filename.back() == '\\' || fld.filename.back() == '/') {
                fld.isDir = true;
            }
            else {
                fld.isDir = false;
            }
            fld.fileSize = fileInfo.uncompressed_size;
            fld.CRC = (uint32_t)(fileInfo.crc);
            std::string fnlower = fld.filename;
            boost::algorithm::to_lower(fnlower);
            if (fnlower.find("eboot.bin") != std::string::npos) {
                /* Check eboot.bin 0x80 */
                std::vector<char> buf(0x81);
                int rc = 0;
                res = unzOpenCurrentFile(archive);
                if (res != UNZ_OK) {
                    print(1, "Failure on unzOpenCurrentFile");
                }
                else {
                    rc = unzReadCurrentFile(archive, &buf[0], (unsigned int)(buf.size()));
                    if (buf[0x80] == 1 || buf[0x80] == 2) {
                        itemSafemode[id] = buf[0x80];
                    }
                    unzCloseCurrentFile(archive);
                }
            }
            fldvec.push_back(fld);
            if ((i+1)<gi.number_entry) {
                res = unzGoToNextFile(archive);
                if (res != UNZ_OK) {
                    print(1, "Failure on unzGoToNextFile");
                    break;
                }
            }
            ++fileId;
        }
        unzClose(archive);
        fileIds[id] = fldvec;
        return 0;
    }
    return 1;
}

Server::~Server(void) {
	print(4, "Server::~Server");
}
