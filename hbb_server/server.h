#pragma once
#include "main.h"

class Server {
private:
	boost::asio::ip::tcp::acceptor acceptor;
    boost::asio::ip::tcp::acceptor httpAcceptor;
public:
    std::string serverVersion;
    std::string clientVersion;
    std::vector<Client*> clients;
    std::string dataDir;
    Database* db;
    std::vector<struct categoryItem> categories;
    std::map<unsigned int, std::vector<struct fileListData>> fileIds;
    std::map<unsigned int, char> itemSafemode;
    std::string httpSessionRandom;

    Server(unsigned short port, unsigned short httpPort);
    void startAccept(void);
    void handleAccept(Client *newClient, const boost::system::error_code &error);
    void startAcceptHttp(void);
    void handleAcceptHttp(Http *newClient, const boost::system::error_code &error);
    int readCategories(void);
    int readArchives(void);
    int readArchives(std::string fileName);
    ~Server(void);
};
