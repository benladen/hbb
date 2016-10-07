#pragma once
#include "main.h"

class Http {
private:
    Server* server;
    std::vector<unsigned char> buffer;
    std::vector<unsigned char> outBuffer;
    std::vector<std::vector<unsigned char>> outBufferPending;
    bool disconnected;

    void disconnect(std::string reason);
    void recv(void);
    void handleRecv(const boost::system::error_code &error, size_t size);
    void send(std::vector<unsigned char> sd);
    void handleSend(const boost::system::error_code &error);
    void processData(std::vector<std::string> lines);
    void sendPage(std::string text);
	std::string htmlspecialchars(std::string input);
public:
	boost::asio::ip::tcp::socket socket;
    std::string ipAddress;

    Http(void);
    void start(Server*);
    
    ~Http(void);
};
