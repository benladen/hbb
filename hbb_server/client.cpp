#include "main.h"

Client::Client(void) : socket(ioService), pingTimer(ioService, boost::posix_time::seconds(3)) {
	print(4, "Client::Client");
    zipFileIsOpen = false;
    zipHasFileOpen = false;
    zipFile = NULL;
    zipItemID = 0;
    zipFileID = 0;
    zipFileLength = 0;
    zipFileChunkCount = 0;
    zipFileCurrentChunk = 0;
}

void Client::start(Server *srv) {
    print(4, "Client connection opened. IP: " + socket.remote_endpoint().address().to_string());
    server = srv;
    readSize = 0;
    initEvent = true;
    disconnected = false;
    ipAddress = socket.remote_endpoint().address().to_string();
    recv();
    pingTimer.cancel();
    pingTimer.expires_from_now(boost::posix_time::seconds(3));
    pingTimer.async_wait(boost::bind(&Client::pingTimeout, this, boost::asio::placeholders::error));
    server->clients.push_back(this);
}

void Client::disconnect(std::string reason) {
    if (disconnected) {
        return;
    }
    print(4, "Disconnect reason: " + reason);
    if (server != NULL) {
        for (unsigned int i = 0; i < server->clients.size(); i++) {
            if (server->clients[i] == this) {
                std::swap(server->clients[i], server->clients.back());
                server->clients.pop_back();
                break;
            }
        }
    }
    if (zipFileIsOpen) {
        if (zipHasFileOpen) {
            unzCloseCurrentFile(zipFile);
        }
        unzClose(zipFile);
        zipFileIsOpen = false;
        zipHasFileOpen = false;
    }
    socket.close();
    if (reason == "E") {
        disconnected = true;
        delete this;
    }
}

void Client::recv(void) {
    if (readSize == 0) {
        buffer.resize(4);
        data.resize(0);
    }
    else {
        buffer.resize(readSize);
    }
    socket.async_read_some(boost::asio::buffer(buffer), boost::bind(&Client::handleRecv, this,
        boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void Client::handleRecv(const boost::system::error_code &error, size_t size) {
    if (!error) {
        if (readSize == 0) {
            int sz;
            memcpy(&sz, &buffer[0], sizeof(sz));
            sz = ntohl(sz);
            if (sz > 1 * (1024 * 1024)) { /* 1MiB */
                disconnect(ipAddress+": Data size too large");
            }
            readSize = sz-4;
        }
        else {
            readSize -= size;
            data.insert(data.end(), buffer.cbegin(), buffer.cend());
            if (readSize == 0) {
                processData();
            }
        }
        recv();
    }
    else {
        disconnect("E");
    }
}

void Client::send(char ev1, char ev2, std::vector<unsigned char> sd) {
    std::vector<unsigned char> head;
    int32_t size = htonl(sd.size()+6);
    addDataToVector(head, &size, sizeof(size));
    addDataToVector(head, &ev1, sizeof(ev1));
    addDataToVector(head, &ev2, sizeof(ev2));
    send(head);
    send(sd);
}

void Client::send(char ev1, char ev2) {
    std::vector<unsigned char> head;
    int32_t size = htonl(6);
    addDataToVector(head, &size, sizeof(size));
    addDataToVector(head, &ev1, sizeof(ev1));
    addDataToVector(head, &ev2, sizeof(ev2));
    send(head);
}

void Client::send(std::vector<unsigned char> sd) {
    if (outBuffer.size() == 0) {
        outBuffer = sd;
        boost::asio::async_write(socket, boost::asio::buffer(outBuffer),
                                 boost::bind(&Client::handleSend, this,
                                 boost::asio::placeholders::error));
    }
    else {
        outBufferPending.push_back(sd);
    }
}

void Client::handleSend(const boost::system::error_code &error) {
    if (!error) {
        if (outBufferPending.size() > 0) {
            outBuffer = outBufferPending[0];
            outBufferPending.erase(outBufferPending.begin());
            boost::asio::async_write(socket, boost::asio::buffer(outBuffer),
                                     boost::bind(&Client::handleSend, this,
                                     boost::asio::placeholders::error));
        }
        else {
            outBuffer.clear();
        }
    }
    else {
        disconnect("E");
    }
}

void Client::pingTimeout(const boost::system::error_code &error) {
    if (!error) {
        disconnect("OpenTimerKick");
    }
}

void Client::processData(void) {
    std::vector<unsigned char>::size_type pos = 2;
    unsigned char ev1;
    unsigned char ev2;
    ev1 = data[0];
    ev2 = data[1];
    if (initEvent) {
        if (ev1 != 2 && ev2 != 3) {
            disconnect("Client did not send version");
            return;
        }
    }
    if (ev1 == 2) {
        if (ev2 == 2) { /* Ping */
            pingTimer.cancel();
            pingTimer.expires_from_now(boost::posix_time::seconds(120));
            pingTimer.async_wait(boost::bind(&Client::pingTimeout, this, boost::asio::placeholders::error));
            sendPingReply(1);
            return;
        }
        else if (ev2 == 3) { /* Version */
            if (data.size() >= 3) {
                uint8_t strLen = data[pos];
                ++pos;
                if (strLen+3 == data.size()) {
                    std::string verStr(data.begin()+pos, data.begin()+(pos+strLen));
                    if (verStr != server->clientVersion) {
                        sendUpdateInfo();
                    }
                    else {
                        initEvent = false;
                        sendPingReply(0);
                        if (testing) {
                            sendMessage("This is the test server.");
                        }
                    }
                }
                else {
                    disconnect("2,3 length incorrect. [0]");
                }
            }
            else {
                disconnect("2,3 length incorrect. [1]");
            }
            return;
        }
        else if (ev2 == 5) { /* Command */
            if (data.size() >= 4) {
                uint16_t strLen;
                memcpy(&strLen, &data[pos], sizeof(strLen));
                strLen = ntohs(strLen);
                pos += 2;
                if (strLen+4 == (uint16_t)(data.size())) {
                    std::string cmdStr(data.begin()+pos, data.begin()+(pos+strLen));
					print(4, cmdStr);
                }
                else {
                    disconnect("2,5 length incorrect. [0]");
                }
            }
            else {
                disconnect("2,5 length incorrect. [1]");
            }
        }
        else {
            disconnect("2 Event code 2 invalid.");
        }
    }
    else if (ev1 == 3) {
        if (ev2 == 2) { /* Category List Request */
            if (data.size() == 3) {
                uint8_t mode = data[pos];
                ++pos;
                sendCategoryList(mode);
            }
            else {
                disconnect("3,2 length incorrect.");
            }
            return;
        }
        else if (ev2 == 3) { /* Item List Request */
            if (data.size() == 3) {
                uint8_t c = data[pos];
                ++pos;
                sendItemList(c);
            }
            else {
                disconnect("3,3 length incorrect.");
            }
            return;
        }
        else if (ev2 == 4) { /* Item Icon Request */
            uint32_t item = 0;
            if (data.size() == 6) {
                memcpy(&item, &data[pos], sizeof(item));
                item = ntohl(item);
                pos+=4;
                sendItemIcon(item);
            }
            else {
                disconnect("3,4 length incorrect.");
            }
        }
        else if (ev2 == 5) { /* Item Details Request */
            uint32_t item = 0;
            if (data.size() == 6) {
                memcpy(&item, &data[pos], sizeof(item));
                item = ntohl(item);
                pos+=4;
                sendItemDetails(item);
            }
            else {
                disconnect("3,5 length incorrect.");
            }
        }
        else if (ev2 == 10) { /* Item Vote */
            return;
        }
        else {
            disconnect("3 Event code 2 invalid.");
        }
    }
    else if (ev1 == 4) {
        if (ev2 == 2) { /* Package Information Request */
            uint32_t item = 0;
            if (data.size() == 6) {
                memcpy(&item, &data[pos], sizeof(item));
                item = ntohl(item);
                pos+=4;
                sendPackageInformation(item);
            }
            else {
                disconnect("4,2 length incorrect.");
            }
        }
        else if (ev2 == 3) { /* File Request */
            uint32_t item = 0;
            uint32_t fileid = 0;
            if (data.size() == 10) {
                memcpy(&item, &data[pos], sizeof(item));
                item = ntohl(item);
                pos+=4;
                memcpy(&fileid, &data[pos], sizeof(fileid));
                fileid = ntohl(fileid);
                pos+=4;

                if (zipFileIsOpen) {
                    if (zipHasFileOpen) {
                        unzCloseCurrentFile(zipFile);
                    }
                    unzClose(zipFile);
                    zipFile = NULL;
                }
                zipFileIsOpen = false;
                zipHasFileOpen = false;
                zipItemID = 0;
                zipFileID = 0;
                zipFileLength = 0;
                zipFileChunkCount = 0;
                zipFileCurrentChunk = 0;
                
                std::vector<std::vector<std::string>> results;
                std::vector<std::string> params;
                params.push_back(boost::lexical_cast<std::string>((int)(item)));
                results = server->db->SQLiteExecute("select filename from app where id = ?", params);
                if (results.size() == 0) {
                    disconnect("4,3 id invalid. [0]");
                    return;
                }
                if (server->fileIds.count(item) == 0) {
                    disconnect("4,3 id invalid. [1]");
                    return;
                }
                
                std::vector<struct fileListData> fldvec = server->fileIds[item];
                for (unsigned int i = 0; i < fldvec.size(); i++) {
                    struct fileListData fld = fldvec[i];
                    if (fld.fileId == fileid) {
                        std::string zipName = server->dataDir + results[0][0];
                        double fileSizeDbl = (double)(fld.fileSize);
                        double chunkSizeDbl = FILE_CHUNK_SIZE;
                        
                        zipFile = unzOpen64(zipName.c_str());
                        if (zipFile == NULL) {
                            print(1, "Error on unzOpen64");
                        }
                        zipFileIsOpen = true;
                        zipItemID = item;
                        zipFileID = fileid;
                        zipFileLength = fld.fileSize;
                        zipFileChunkCount = (unsigned int)(ceil(fileSizeDbl/chunkSizeDbl));
                        sendFileStart(item, fileid, zipFileChunkCount);
                        sendFileData();
                        break;
                    }
                }
            }
            else {
                disconnect("4,3 length incorrect.");
            }
        }
        else if (ev2 == 4) { /* Chunk Recv OK */
            if (zipFileCurrentChunk == zipFileChunkCount) {
                unsigned int item = zipItemID;
                unsigned int file = zipFileID;
                if (zipFileIsOpen) {
                    if (zipHasFileOpen) {
                        unzCloseCurrentFile(zipFile);
                    }
                    unzClose(zipFile);
                    zipFile = NULL;
                    zipFileIsOpen = false;
                    zipHasFileOpen = false;
                    zipItemID = 0;
                    zipFileID = 0;
                    zipFileLength = 0;
                    zipFileChunkCount = 0;
                    zipFileCurrentChunk = 0;
                }
                sendFileEnd(item, file);
            }
            else {
                sendFileData();
            }
        }
        else {
            disconnect("4 Event code 2 invalid.");
        }
    }
    else {
        disconnect("Event code 1 invalid.");
    }
}

void Client::sendPingReply(char n) { /* 2,2 */
    std::vector<unsigned char> sd;
    addDataToVector(sd, &n, sizeof(n));
    send(2, 2, sd);
}

void Client::sendMessageDebug(std::string str) { /* 2,3 */
    std::vector<unsigned char> sd;
    int16_t strLen = htons(str.length());
    addDataToVector(sd, &strLen, sizeof(strLen));
    addDataToVector(sd, str.c_str(), str.length());
    send(2, 3, sd);
}

void Client::sendMessageError(std::string str) { /* 2,4 */
    std::vector<unsigned char> sd;
    int16_t strLen = htons(str.length());
    addDataToVector(sd, &strLen, sizeof(strLen));
    addDataToVector(sd, str.c_str(), str.length());
    send(2, 4, sd);
}

void Client::sendMessage(std::string str) { /* 2,5 */
    std::vector<unsigned char> sd;
    int16_t strLen = htons(str.length());
    addDataToVector(sd, &strLen, sizeof(strLen));
    addDataToVector(sd, str.c_str(), str.length());
    send(2, 5, sd);
}

void Client::sendServerShutdown(char type, unsigned char time) { /* 2,6 */
    std::vector<unsigned char> sd;
    addDataToVector(sd, &type, sizeof(type));
    addDataToVector(sd, &time, sizeof(time));
    send(2, 6, sd);
}

void Client::sendBanInfo(short time, std::string message) { /* 2,7 */
    std::vector<unsigned char> sd;
    int16_t t = htons(time);
    int16_t strLen = htons(message.length());
    addDataToVector(sd, &t, sizeof(t));
    addDataToVector(sd, &strLen, sizeof(strLen));
    addDataToVector(sd, message.c_str(), message.length());
    send(2, 7, sd);
}

void Client::sendUpdateInfo(void){ /* 2,8 */
    send(2, 8);
}

void Client::sendCategoryList(char mode) { /* 3,2 */
    std::vector<unsigned char> sd;
    if (mode == 0) {
        int32_t len = htonl(server->categories.size());
        addDataToVector(sd, &len, sizeof(len));
        for (unsigned int i = 0; i < server->categories.size(); i++) {
            int16_t strLen = htons(server->categories[i].name.length());
            addDataToVector(sd, &server->categories[i].id, sizeof(server->categories[i].id));
            addDataToVector(sd, &strLen, sizeof(strLen));
            addDataToVector(sd, server->categories[i].name.c_str(), server->categories[i].name.length());
        }
        send(3, 2, sd);
    }
    else {
        /* 
         if mode == 1:
            skip all that have "old versions" in the name
         else if mode == 2:
            skip all that have "test[" in the name
         else if mode == 3:
            1 and 2
         else:
            invalid
        */
    }
}

void Client::sendItemList(char category) { /* 3,3 */
    std::vector<unsigned char> sd;
    std::vector<std::vector<std::string>> results;
    std::vector<std::string> params;
    params.push_back(boost::lexical_cast<std::string>((int)(category)));
    
    results = server->db->SQLiteExecute("select id,title_id,date,displayname,author,version,cachedrating,extra from app where category = ?", params);
    int32_t rsize = htonl((int32_t)(results.size()));
    addDataToVector(sd, &rsize, sizeof(rsize));
    for (unsigned int i = 0; i < results.size(); i++) {
        uint32_t id = htonl(stoi(results[i][0]));
        uint16_t titleIdLen = htons(results[i][1].length());
        std::string titleId = results[i][1];
        uint32_t date = htonl(stoi(results[i][2]));
        uint16_t displayNameLen = htons(results[i][3].length());
        std::string displayName = results[i][3];
        uint16_t authorLen = htons(results[i][4].length());
        std::string author = results[i][4];
        uint16_t versionLen = htons(results[i][5].length());
        std::string version = results[i][5];
        uint32_t cachedRating = htonl(stoi(results[i][6]));
        uint16_t extraLen = htons(results[i][7].length());
        std::string extra = results[i][7];
        
        addDataToVector(sd, &id, sizeof(id));
        addDataToVector(sd, &titleIdLen, sizeof(titleIdLen));
        addDataToVector(sd, titleId.c_str(), titleId.length());
        addDataToVector(sd, &date, sizeof(date));
        addDataToVector(sd, &displayNameLen, sizeof(displayNameLen));
        addDataToVector(sd, displayName.c_str(), displayName.length());
        addDataToVector(sd, &authorLen, sizeof(authorLen));
        addDataToVector(sd, author.c_str(), author.length());
        addDataToVector(sd, &versionLen, sizeof(versionLen));
        addDataToVector(sd, version.c_str(), version.length());
        addDataToVector(sd, &cachedRating, sizeof(cachedRating));
        addDataToVector(sd, &extraLen, sizeof(extraLen));
        addDataToVector(sd, extra.c_str(), extra.length());
    }
    send(3, 3, sd);
}

void Client::sendItemIcon(unsigned int itemid) { /* 3,4 */
    std::vector<unsigned char> sd;
    std::vector<std::vector<std::string>> results;
    std::vector<std::string> params;
    uint32_t id = htonl(itemid);
    uint32_t dataSize = 0;
    int res;
    
    params.push_back(boost::lexical_cast<std::string>((int)(itemid)));
    results = server->db->SQLiteExecute("select filename from app where id = ?", params);
    if (results.size() == 0) {
        addDataToVector(sd, &id, sizeof(id));
        addDataToVector(sd, &dataSize, sizeof(dataSize));
        send(3, 4, sd);
        return;
    }
    
    std::string zipName = server->dataDir + results[0][0];
    unzFile archive = unzOpen64(zipName.c_str());
    unz_global_info64 gi;
    if (archive == NULL) {
        addDataToVector(sd, &id, sizeof(id));
        addDataToVector(sd, &dataSize, sizeof(dataSize));
        send(3, 4, sd);
        return;
    }
    
    res = unzGetGlobalInfo64(archive, &gi);
    if (res != UNZ_OK) {
        unzClose(archive);
        addDataToVector(sd, &id, sizeof(id));
        addDataToVector(sd, &dataSize, sizeof(dataSize));
        send(3, 4, sd);
        return;
    }
    for (unsigned long i = 0; i < gi.number_entry; i++) {
        unz_file_info64 fileInfo;
        char filename[256];
        std::string sfilename;
        std::string fnlower;
        res = unzGetCurrentFileInfo64(archive, &fileInfo, filename, sizeof(filename), NULL, 0, NULL, 0);
        if (res != UNZ_OK) {
            unzClose(archive);
            addDataToVector(sd, &id, sizeof(id));
            addDataToVector(sd, &dataSize, sizeof(dataSize));
            send(3, 4, sd);
            return;
        }
        sfilename = std::string(filename);
        fnlower = sfilename;
        boost::algorithm::to_lower(fnlower);
        if (fnlower == "sce_sys/icon0.png") {
                std::vector<char> buffer((unsigned int)(fileInfo.uncompressed_size));
                std::vector<char>::size_type totalBytesRead = 0;
                struct rawImage* pngData;
                
                res = unzOpenCurrentFile(archive);
                if (res != UNZ_OK) {
                    unzClose(archive);
                    addDataToVector(sd, &id, sizeof(id));
                    addDataToVector(sd, &dataSize, sizeof(dataSize));
                    send(3, 4, sd);
                    return;
                }
                while(totalBytesRead < fileInfo.uncompressed_size) {
                    int bytesRead = unzReadCurrentFile(archive, &buffer[totalBytesRead], (unsigned int)(buffer.size() - totalBytesRead));
                    if(bytesRead < 0) {
                        unzCloseCurrentFile(archive);
                        unzClose(archive);
                        addDataToVector(sd, &id, sizeof(id));
                        addDataToVector(sd, &dataSize, sizeof(dataSize));
                        send(3, 4, sd);
                        return;
                    }
                    if(bytesRead == 0) {
                        break;
                    }
                    totalBytesRead += bytesRead;
                }
                unzCloseCurrentFile(archive);
                unzClose(archive);

                pngData = readPNG(&buffer[0], buffer.size());
                if (pngData == NULL) {
                    addDataToVector(sd, &id, sizeof(id));
                    addDataToVector(sd, &dataSize, sizeof(dataSize));
                    send(3, 4, sd);
                    return;
                }
                addAlphaChannelToImage(pngData);
                dataSize = htonl(pngData->dataSize);
                addDataToVector(sd, &id, sizeof(id));
                addDataToVector(sd, &dataSize, sizeof(dataSize));
                addDataToVector(sd, pngData->data, pngData->dataSize);
                send(3, 4, sd);
                free(pngData->data);
                free(pngData);
                return;
        }
        if ((i+1)<gi.number_entry) {
            res = unzGoToNextFile(archive);
            if (res != UNZ_OK) {
                unzClose(archive);
                addDataToVector(sd, &id, sizeof(id));
                addDataToVector(sd, &dataSize, sizeof(dataSize));
                send(3, 4, sd);
                return;
            }
        }
    }
    unzClose(archive);
    addDataToVector(sd, &id, sizeof(id));
    addDataToVector(sd, &dataSize, sizeof(dataSize));
    send(3, 4, sd);
}

void Client::sendItemDetails(unsigned int itemid) { /* 3,5 */
    std::vector<unsigned char> sd;
    std::vector<std::vector<std::string>> results;
    std::vector<std::string> params;
    params.push_back(boost::lexical_cast<std::string>((int)(itemid)));
    results = server->db->SQLiteExecute("select id,category,webUrl,description,dlcount from app where id = ?", params);
    if (results.size() == 0) {
        disconnect("3,5 id invalid.");
        return;
    }
    uint32_t id = htonl(stoi(results[0][0]));
    uint8_t category = stoi(results[0][1]);
    uint16_t webUrlLen = htons(results[0][2].length());
    std::string webUrl = results[0][2];
    uint32_t descriptionLen = htonl(results[0][3].length());
    std::string description = results[0][3];
    uint32_t dlcount = htonl(stoi(results[0][4]));
    uint8_t isSafeHomebrew = 0;
    if (server->itemSafemode.count(itemid) != 0) {
        isSafeHomebrew = server->itemSafemode[itemid];
    }
    
    addDataToVector(sd, &id, sizeof(id));
    addDataToVector(sd, &category, sizeof(category));
    addDataToVector(sd, &webUrlLen, sizeof(webUrlLen));
    addDataToVector(sd, webUrl.c_str(), webUrl.length());
    addDataToVector(sd, &descriptionLen, sizeof(descriptionLen));
    addDataToVector(sd, description.c_str(), description.length());
    addDataToVector(sd, &dlcount, sizeof(dlcount));
    addDataToVector(sd, &isSafeHomebrew, sizeof(isSafeHomebrew));
    send(3, 5, sd);
}

void Client::sendPackageInformation(unsigned int itemid) { /* 4,2 */
    std::vector<unsigned char> sd;
    std::vector<std::vector<std::string>> results;
    std::vector<std::string> params;
    params.push_back(boost::lexical_cast<std::string>((int)(itemid)));
    results = server->db->SQLiteExecute("select filename,dlcount from app where id = ?", params);
    if (results.size() == 0) {
        disconnect("4,2 id invalid. [0]");
        return;
    }
    if (server->fileIds.count(itemid) == 0) {
        disconnect("4,2 id invalid. [1]");
        return;
    }
    std::string fnlower = results[0][0];
    uint32_t sendItemid = htonl(itemid);
    uint8_t pkgType = 0;
    uint32_t fileCount = htonl(server->fileIds[itemid].size());
    std::vector<struct fileListData> fldvec = server->fileIds[itemid];
    boost::algorithm::to_lower(fnlower);
    if (boost::algorithm::ends_with(fnlower, ".vpk")) {
        pkgType = 1;
    }
    else if (boost::algorithm::ends_with(fnlower, ".p1")) {
        pkgType = 2;
    }
    else if (boost::algorithm::ends_with(fnlower, ".p2")) {
        pkgType = 3;
    }
    addDataToVector(sd, &sendItemid, sizeof(sendItemid));
    addDataToVector(sd, &pkgType, sizeof(pkgType));
    addDataToVector(sd, &fileCount, sizeof(fileCount));
    for (unsigned int i = 0; i < fldvec.size(); i++) {
        struct fileListData fld = fldvec[i];
        uint32_t fileId = htonl(fld.fileId);
        uint32_t fileSize = htonl((uint32_t)(fld.fileSize));
        uint16_t filenameLen = htons(fld.filename.length());
        std::string filename = fld.filename;
        uint32_t fileCrc = htonl(fld.CRC);
        uint8_t isDir = 0;
        addDataToVector(sd, &fileId, sizeof(fileId));
        addDataToVector(sd, &fileSize, sizeof(fileSize));
        addDataToVector(sd, &filenameLen, sizeof(filenameLen));
        addDataToVector(sd, filename.c_str(), fld.filename.length());
        addDataToVector(sd, &fileCrc, sizeof(fileCrc));
        if (fld.isDir) {
            isDir = 1;
        }
        addDataToVector(sd, &isDir, sizeof(isDir));
    }
    
    params.clear();
    params.push_back(boost::lexical_cast<std::string>((int)(stoi(results[0][1])+1)));
    params.push_back(boost::lexical_cast<std::string>((int)(itemid)));
    server->db->SQLiteExecute("UPDATE app SET dlcount = ? WHERE id = ?", params);
    send(4, 2, sd);
}

void Client::sendFileStart(unsigned int item, unsigned int fileid, unsigned int chunkCount) { /* 4,3 */
    std::vector<unsigned char> sd;
    uint32_t sendItem = htonl(item);
    uint32_t sendFileid = htonl(fileid);
    uint32_t sendChunkCount = htonl(chunkCount);
    addDataToVector(sd, &sendItem, sizeof(sendItem));
    addDataToVector(sd, &sendFileid, sizeof(sendFileid));
    addDataToVector(sd, &sendChunkCount, sizeof(sendChunkCount));
    send(4, 3, sd);
}

void Client::sendFileData(void) { /* 4,4 */
    if (zipFileIsOpen == false) {
        disconnect("zipFileIsOpen is false");
        return;
    }
    if (zipHasFileOpen == false) {
        /* Open file */
        if (server->fileIds.count(zipItemID) == 0) {
            disconnect("zipItemID invalid");
            return;
        }
        std::vector<struct fileListData> fldvec = server->fileIds[zipItemID];
        for (unsigned int i = 0; i < fldvec.size(); i++) {
            struct fileListData fld = fldvec[i];
            if (fld.fileId == zipFileID) {
                int res;
                unz_global_info64 gi;
                res = unzGetGlobalInfo64(zipFile, &gi);
                if (res != UNZ_OK) {
                    disconnect("Failure on unzGetGlobalInfo64");
                    return;
                }

                for (unsigned long j = 0; j < gi.number_entry; j++) {
                    unz_file_info64 fileInfo;
                    char filename[256];
                    std::string sfilename;
                    res = unzGetCurrentFileInfo64(zipFile, &fileInfo, filename, sizeof(filename), NULL, 0, NULL, 0);
                    if (res != UNZ_OK) {
                        disconnect("Failure on unzGetCurrentFileInfo64");
                        return;
                    }
                    sfilename = std::string(filename);
                    if (sfilename == fld.filename) {
                        res = unzOpenCurrentFile(zipFile);
                        if (res != UNZ_OK) {
                            disconnect("Failure on unzOpenCurrentFile");
                            return;
                        }
                        zipHasFileOpen = true;
                        break;
                    }
                    if ((j+1) < gi.number_entry) {
                        res = unzGoToNextFile(zipFile);
                        if (res != UNZ_OK) {
                            disconnect("Failure on unzGoToNextFile");
                            break;
                        }
                    }
                }
                break;
            }
        }
    }
    if (zipHasFileOpen == false) {
        disconnect("File still not open.");
        return;
    }
    /* Read data from ZIP */
    ++zipFileCurrentChunk;
    uint64_t readSize = FILE_CHUNK_SIZE;
    if (zipFileCurrentChunk == zipFileChunkCount) {
        readSize = zipFileLength % FILE_CHUNK_SIZE;
    }
    std::vector<char> buffer((uint32_t)(readSize));
    std::vector<char>::size_type totalBytesRead = 0;
	while(totalBytesRead < readSize) {
        int bytesRead = unzReadCurrentFile(zipFile, &buffer[totalBytesRead], (unsigned int)(buffer.size() - totalBytesRead));
        if(bytesRead < 0) {
            disconnect("Error while reading zip entry");
            return;
		}
        if(bytesRead == 0) {
            break;
		}
        totalBytesRead += bytesRead;
    }

    sendFileData(zipItemID, zipFileID, zipFileCurrentChunk, buffer);
}

void Client::sendFileData(unsigned int item, unsigned int fileid, unsigned int chunkNum, std::vector<char> data) { /* 4,4 */
    std::vector<unsigned char> sd;
    uint32_t sendItem = htonl(item);
    uint32_t sendFileid = htonl(fileid);
    uint32_t sendChunkNum = htonl(chunkNum);
    uint32_t dataLen = htonl(data.size());
    addDataToVector(sd, &sendItem, sizeof(sendItem));
    addDataToVector(sd, &sendFileid, sizeof(sendFileid));
    addDataToVector(sd, &sendChunkNum, sizeof(sendChunkNum));
    addDataToVector(sd, &dataLen, sizeof(dataLen));
    addDataToVector(sd, &data[0], data.size());
    send(4, 4, sd);
}

void Client::sendFileEnd(unsigned int item, unsigned int fileid) { /* 4,5 */
    std::vector<unsigned char> sd;
    uint32_t sendItem = htonl(item);
    uint32_t sendFileid = htonl(fileid);
    addDataToVector(sd, &sendItem, sizeof(sendItem));
    addDataToVector(sd, &sendFileid, sizeof(sendFileid));
    send(4, 5, sd);
}

Client::~Client(void) {
	print(4, "Client::~Client");
    try {
        pingTimer.cancel();
    }
    catch(...) {
    }
}
