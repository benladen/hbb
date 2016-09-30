#pragma once
#include "main.h"

class Client {
private:
    Server* server;
    size_t readSize;
    std::vector<unsigned char> buffer;
    std::vector<unsigned char> data;
    std::vector<unsigned char> outBuffer;
    std::vector<std::vector<unsigned char>> outBufferPending;

    bool initEvent;
    bool disconnected;
    boost::asio::deadline_timer pingTimer;
    
    void disconnect(std::string reason);
    void recv(void);
    void handleRecv(const boost::system::error_code &error, size_t size);
    void send(char ev1, char ev2, std::vector<unsigned char> sd);
    void send(char ev1, char ev2);
    void send(std::vector<unsigned char> sd);
    void handleSend(const boost::system::error_code &error);
    void pingTimeout(const boost::system::error_code &error);
    void processData(void);
public:
	boost::asio::ip::tcp::socket socket;
    std::string ipAddress;
    bool zipFileIsOpen;
    bool zipHasFileOpen;
    unzFile zipFile;
    unsigned int zipItemID;
    unsigned int zipFileID;
    uint64_t zipFileLength;
    unsigned int zipFileChunkCount;
    unsigned int zipFileCurrentChunk;

    Client(void);
    void start(Server*);
    
    void sendPingReply(char n);
    void sendMessageDebug(std::string str);
    void sendMessageError(std::string str);
    void sendMessage(std::string str);
    void sendServerShutdown(char type, unsigned char time);
    void sendBanInfo(short time, std::string message);
    void sendUpdateInfo(void);
    
    void sendCategoryList(char mode);
    void sendItemList(char category);
    void sendItemIcon(unsigned int itemid);
    void sendItemDetails(unsigned int itemid);
    
    void sendPackageInformation(unsigned int itemid);
    void sendFileStart(unsigned int item, unsigned int fileid, unsigned int chunkCount);
    void sendFileData(void);
    void sendFileData(unsigned int item, unsigned int fileid, unsigned int chunkNum, std::vector<char> data);
    void sendFileEnd(unsigned int item, unsigned int fileid);
    
    ~Client(void);
};
