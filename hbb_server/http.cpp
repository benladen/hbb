#include "main.h"

Http::Http(void) : socket(ioService) {
	print(4, "Http::Http");
    buffer.resize(1024);
}

void Http::start(Server *srv) {
    print(4, "Client connection opened. IP: " + socket.remote_endpoint().address().to_string());
    server = srv;
    disconnected = false;
    ipAddress = socket.remote_endpoint().address().to_string();
    std::string acceptIPs = getSetting("Config.HTTP_AcceptIPs", "127.0.0.1");
    std::string ipTemp = "";
    bool addrFound = false;
	acceptIPs += ",";
    for (std::string::size_type i = 0; i < acceptIPs.length(); i++) {
        if (acceptIPs.at(i) == ',') {
            if (ipAddress == ipTemp) {
                addrFound = true;
            }
            ipTemp = "";
        }
        else {
            ipTemp += acceptIPs.at(i);
        }
    }
    if (addrFound) {
        buffer.resize(1048576);
        recv();
    }
    else {
        disconnect("E");
    }
}

void Http::disconnect(std::string reason) {
    if (disconnected) {
        return;
    }
    print(4, "Disconnect reason: " + reason);
    socket.close();
    if (reason == "E") {
        disconnected = true;
        delete this;
    }
}

void Http::recv(void) {
   socket.async_read_some(boost::asio::buffer(buffer), boost::bind(&Http::handleRecv, this,
        boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void Http::handleRecv(const boost::system::error_code& error, size_t size) {
    std::vector<std::string> lines;
    std::string line = "";
    if (!error) {
        for (unsigned int i = 0; i < size; i++) {
            if (buffer[i] == '\r' && buffer[i+1] == '\n') {
                if (line == "") {
                    processData(lines);
                    return;
                }
                lines.push_back(line);
                line = "";
                ++i;
            }
            else {
                line += buffer[i];
            }
        }
        disconnect("End of handleRecv");
    }
    else {
        disconnect("E");
    }
}

void Http::send(std::vector<unsigned char> sd) {
    if (outBuffer.size() == 0) {
        outBuffer = sd;
        boost::asio::async_write(socket, boost::asio::buffer(outBuffer),
                                 boost::bind(&Http::handleSend, this,
                                 boost::asio::placeholders::error));
    }
    else {
        outBufferPending.push_back(sd);
    }
}

void Http::handleSend(const boost::system::error_code &error) {
    if (!error) {
        if (outBufferPending.size() > 0) {
            outBuffer = outBufferPending[0];
            outBufferPending.erase(outBufferPending.begin());
            boost::asio::async_write(socket, boost::asio::buffer(outBuffer),
                                     boost::bind(&Http::handleSend, this,
                                     boost::asio::placeholders::error));
        }
        else {
            outBuffer.clear();
            disconnect("E");
        }
    }
    else {
        disconnect("E");
    }
}

void Http::processData(std::vector<std::string> lines) {
    std::string getLine = "";
    std::string lower = "";
    for (unsigned int i = 0; i < lines.size(); i++) {
        lower = lines[i];
        boost::algorithm::to_lower(lower);
        if (lower.find("get ") != std::string::npos) {
            getLine = lines[i];
        }
    }
    if (getLine == "") {
        disconnect("Could not find GET");
    }
    getLine.erase(0, 4);
    for (unsigned int i = 0; i < getLine.length(); i++) {
        if (getLine.at(i) == ' ') {
            getLine.erase(i, getLine.length());
            break;
        }
    }
    if (getLine == "/") {
        sendPage("<html>"
            "<head><title>Index</title></head>"
            "<body>"
            "<h2><i>Administrator Control Panel</i></h2>"
            "<hr>"
            "Server:<br>"
            "<a href=\"/server/stop\">Stop the server program.</a><br>"
            "<a href=\"/server/disconnectAll\">Disconnect all clients.</a><br>"
            //"<a href=\"/server/clientToggle\">Enable/disable accepting new clients.</a><br>"
            "<br>View Database:<br>"
            "<a href=\"/dbView/app/get/AllEntries\">Display all in the app table.</a><br>"
            "<a href=\"/dbView/ratings/get/AllEntries\">Display all in the ratings table.</a><br>"
            "<br>Modify Database:<br>"
            "<a href=\"/dbEdit/app/add/form\">Add entry to the app table.</a><br>"
            "<a href=\"/dbEdit/app/edit/form\">Edit entry in the app table.</a><br>"
            "<a href=\"/dbEdit/app/delete/form\">Delete entry from the app table.</a><br>"
            "</body>"
            "</html>");
        server->httpSessionRandom.clear();
        for (int i = 0; i < 25; i++) {
            char c = rand() % 10 + 48;
            server->httpSessionRandom += c;
        }
        return;
    }
    
    std::vector<std::string> list;
    std::vector<std::string> getParams;
    char section = 0; /* 0 = list, 1 = params */
    std::string tmp;
    for (unsigned int i = 1; i < getLine.length(); i++) {
        if (i+1 == getLine.length()) {
            tmp += getLine.at(i);
        }
        if (getLine.at(i) == '?' && section == 0) {
            list.push_back(tmp);
            tmp.clear();
            section = 1;
        }
        else if ((getLine.at(i) == '/' || i+1 == getLine.length()) && section == 0) {
            list.push_back(tmp);
            tmp.clear();
        }
        else if ((getLine.at(i) == '&' || i+1 == getLine.length()) && section == 1) {
            getParams.push_back(tmp);
            tmp.clear();
        }
        else {
            tmp += getLine.at(i);
        }
    }
    
    if (list.size() >= 1 && list[0] == "server") {
        if (list.size() >= 2 && list[1] == "stop") {
            if (list.size() == 3 && list[2] == server->httpSessionRandom) {
                ioService.stop();
                return;
            }
            else if (list.size() == 2) {
                sendPage("<html>"
                    "<head><title>Server</title></head>"
                    "<body>"
                    "<h2><i>Stop server</i></h2>"
                    "<hr><br>"
                    "Are you sure?<br>"
                    "<a href=\"/server/stop/"+server->httpSessionRandom+"\">Yes</a> "
                    "<a href=\"/\">No</a>"
                    "</body>"
                    "</html>");
                return;
            }
        }
        else if (list.size() >= 2 && list[1] == "disconnectAll") {
            if (list.size() == 3 && list[2] == server->httpSessionRandom) {
                for (unsigned int i = 0; i < server->clients.size(); i++) {
                    server->clients[i]->socket.close();
                }
                sendPage("<html>"
                    "<head><title>Server</title>"
                    "<meta http-equiv=\"refresh\" content=\"0; url=/\" /></head>"
                    "<body>"
                    "<h2><i>Disconnect all clients</i></h2>"
                    "<hr><br>"
                    "<a href=\"/\">Click here if you are not redirected.</a>"
                    "</body>"
                    "</html>");
                return;
            }
            else if (list.size() == 2) {
                sendPage("<html>"
                    "<head><title>Server</title></head>"
                    "<body>"
                    "<h2><i>Disconnect all clients</i></h2>"
                    "<hr><br>"
                    "Are you sure?<br>"
                    "<a href=\"/server/disconnectAll/"+server->httpSessionRandom+"\">Yes</a> "
                    "<a href=\"/\">No</a>"
                    "</body>"
                    "</html>");
                return;
            }
        }
        else if (list.size() >= 2 && list[1] == "clientToggle") {
        }
    }
    else if (list.size() >= 1 && list[0] == "dbView") {
        if (list.size() >= 2) {
            std::string table = list[1];
            if (list.size() >= 3 && list[2] == "get") {
                if (list.size() == 4 && list[3] == "AllEntries") {
                    std::string resultPage = "<html>"
                        "<head><title>dbView</title></head>"
                        "<body>"
                        "<h2><i>"+table+" table</i></h2>"
                        "<hr><br>"
                        "<table border=1>";
                    std::vector<std::vector<std::string>> results;
                    std::vector<std::string> params;
                    params.push_back(table);
                    results = server->db->SQLiteExecute("select sql from sqlite_master where tbl_name = ? and type = 'table'", params);
                    if (results.size() != 0) {
                        resultPage += "<tr>";
                        int nameCount = 0;
                        bool inName = false;
                        std::string name;
                        for (unsigned int i = 0; i < results[0][0].length(); i++) {
                            if (results[0][0].at(i) == '\"') {
                                if (inName) {
                                    if (nameCount > 0) {
                                        resultPage += "<td>";
                                        resultPage += name;
                                        resultPage += "</td>";
                                    }
                                    name.clear();
                                    inName = false;
                                    ++nameCount;
                                }
                                else {
                                    inName = true;
                                }
                            }
                            else if (inName) {
                                name += results[0][0].at(i);
                            }
                        }
                        resultPage += "</tr>";
                    }
                    if (table == "app") {
                        results = server->db->SQLiteExecute("select * from app");
                    }
                    else if (table == "ratings") {
                        results = server->db->SQLiteExecute("select * from ratings");
                    }
                    else {
                        results.clear();
                    }
                    for (unsigned int i = 0; i < results.size(); i++) {
                        resultPage += "<tr>";
                        for (unsigned int j = 0; j < results[i].size(); j++) {
                            resultPage += "<td>";
                            resultPage += results[i][j];
                            resultPage += "</td>";
                        }
                        resultPage += "</tr>";
                    }
                    resultPage += "</table></body></html>";
                    sendPage(resultPage);
                    return;
                }
            }
        }
    }
    else if (list.size() >= 1 && list[0] == "dbEdit") {
        if (list.size() >= 2) {
            std::string table = list[1];
            if (list.size() >= 3 && list[2] == "add") {
                if (list.size() == 4 && list[3] == "form" && table == "app") {
                std::string select;
                for (unsigned int i = 0; i < server->categories.size(); i++) {
                    std::string idstr = boost::lexical_cast<std::string>((int)(server->categories[i].id));
                    select += "<option value=\""+idstr+"\">"+idstr+" - "+server->categories[i].name+"</option>";
                }
                sendPage("<html>"
                    "<head><title>dbEdit</title></head>"
                    "<body>"
                    "<h2><i>Add entry to app</i></h2>"
                    "<hr><br>Notes:<ul>"
                    "<li>If the file is a vpk, Run vpk_info.py to obtain title_id and displayname.</li>"
                    "<li>If the file is not a vpk, Use 000000000 as the title_id.</li>"
                    "<li>The date field is in <a href=\"https://en.wikipedia.org/wiki/Unix_time\">Unix time</a>.</li>"
                    "<li>The extra field may be left blank.</li>"
                    "<li>Existing entries in the File ID list will not be modified.</li>"
                    "<li>The file must already be in the data directory.</li>"
                    "</ul><br><table>"
                    "<form action=\"/dbEdit/"+table+"/add/confirm/"+server->httpSessionRandom+"\" method=\"get\">"
                    "<tr><td>title_id:</td><td><input type=\"text\" name=\"title_id\" maxlength=\"9\"></td></tr>"
                    "<tr><td>date:</td><td><input type=\"number\" name=\"date\" min=\"0\" max=\"4294967295\"></td></tr>"
                    "<tr><td>category:</td><td><select name=\"category\">"+select+"</select></td></tr>"
                    "<tr><td>displayname:</td><td><input type=\"text\" name=\"displayname\"></td></tr>"
                    "<tr><td>filename:</td><td><input type=\"text\" name=\"filename\"></td></tr>"
                    "<tr><td>author:</td><td><input type=\"text\" name=\"author\"></td></tr>"
                    "<tr><td>version:</td><td><input type=\"text\" name=\"version\"></td></tr>"
                    "<tr><td>weburl:</td><td><input type=\"text\" name=\"weburl\"></td></tr>"
                    "<tr><td>description:</td><td><textarea name=\"description\" rows=\"10\" cols=\"68\"></textarea></td></tr>"
                    "<tr><td>dlcount:</td><td><input type=\"number\" name=\"dlcount\" min=\"0\" max=\"4294967295\"></td></tr>"
                    "<tr><td>extra:</td><td><input type=\"text\" name=\"extra\"></td></tr>"
                    "<tr><td></td><td><input type=\"submit\" value=\"Submit\"></td></tr>"
                    "</table></body>"
                    "</html>");
                return;
                }
                else if (list.size() == 5 && list[3] == "confirm" && list[4] == server->httpSessionRandom && table == "app") {
                    std::map<std::string, std::string> paramsMap;
                    for (unsigned int i = 0; i < getParams.size(); i++) {
                        std::string paramName;
                        unsigned int j = 0;
                        for (; j < getParams[i].length(); j++) {
                            if (getParams[i].at(j) == '=') {
                                break;
                            }
                            paramName += getParams[i].at(j);
                        }
                        getParams[i].erase(0, j+1);
                        
                        //Decode getParams[i]
                        std::string dec;
                        boost::replace_all(getParams[i], "+", " ");
                        for (unsigned int j = 0 ; j < getParams[i].length(); j++) {
                            char ch;
                            int ii;
                            if (int(getParams[i][j])==37) {
                                sscanf(getParams[i].substr(j+1, 2).c_str(), "%x", &ii);
                                ch = static_cast<char>(ii);
                                dec += ch;
                                j = j+2;
                            }
                            else {
                                dec+=getParams[i][j];
                            }
                        }
                        boost::replace_all(dec, "\r\n", "\n");
                        getParams[i] = dec;

                        paramsMap[paramName] = getParams[i];
                    }
                    
                    std::vector<std::string> paramNamesCheck;
                    char error = 0;
                    paramNamesCheck.push_back("title_id");
                    paramNamesCheck.push_back("date");
                    paramNamesCheck.push_back("category");
                    paramNamesCheck.push_back("displayname");
                    paramNamesCheck.push_back("filename");
                    paramNamesCheck.push_back("author");
                    paramNamesCheck.push_back("version");
                    paramNamesCheck.push_back("weburl");
                    paramNamesCheck.push_back("description");
                    paramNamesCheck.push_back("dlcount");
                    paramNamesCheck.push_back("extra");
                    for (unsigned int i = 0; i < paramNamesCheck.size(); i++) {
                        if (paramsMap.count(paramNamesCheck[i]) == 0) {
                            error = 1;
                        }
                    }
                    if (error == 1) {
                        print(4, "A name is missing");
                    }
                    else {
                        for (std::map<unsigned int, std::vector<struct fileListData>>::iterator it = server->fileIds.begin(); it != server->fileIds.end(); ++it) {
                            for (unsigned int i = 0; i < it->second.size(); i++) {
                                if (it->second[i].filename == paramsMap["filename"]) {
                                    error = 1;
                                }
                            }
                        }
                        if (error == 0) {
                            if (paramsMap["date"] == "") {
                                paramsMap["date"] = "0";
                            }
                            if (paramsMap["category"] == "") {
                                paramsMap["category"] = "0";
                            }
                            if (paramsMap["dlcount"] == "") {
                                paramsMap["dlcount"] = "0";
                            }
                            paramsMap["cachedrating"] = "0";
                            std::vector<std::string> params;
                            params.push_back(paramsMap["title_id"]);
                            params.push_back(paramsMap["date"]);
                            params.push_back(paramsMap["category"]);
                            params.push_back(paramsMap["displayname"]);
                            params.push_back(paramsMap["filename"]);
                            params.push_back(paramsMap["author"]);
                            params.push_back(paramsMap["version"]);
                            params.push_back(paramsMap["weburl"]);
                            params.push_back(paramsMap["description"]);
                            params.push_back(paramsMap["dlcount"]);
                            params.push_back(paramsMap["cachedrating"]);
                            params.push_back(paramsMap["extra"]);
                            server->db->SQLiteExecute("insert into app (title_id, date, category, displayname, "
                                "filename, author, version, weburl, description, dlcount, cachedrating, extra) "
                                "values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", params);
                            server->readArchives(paramsMap["filename"]);
                            sendPage("<html>"
                                "<head><title>dbEdit</title>"
                                "<meta http-equiv=\"refresh\" content=\"0; url=/\" /></head>"
                                "<body>"
                                "<h2><i>Add entry to app</i></h2>"
                                "<hr><br>"
                                "<a href=\"/\">Click here if you are not redirected.</a>"
                                "</body>"
                                "</html>");
                            return;
                        }
                    }
                }
            }
            else if (list.size() >= 3 && list[2] == "edit") {
                if (list.size() == 4 && list[3] == "form" && table == "app") {
                    std::string select;
                    std::vector<std::vector<std::string>> results;
                    results = server->db->SQLiteExecute("select id,displayname,version from app");
                    for (unsigned int i = 0; i < results.size(); i++) {
                        select += "<option value=\""+results[i][0]+"\">"+results[i][0]+" - "+results[i][1]+" ("+results[i][2]+")</option>";
                    }
                    sendPage("<html>"
                        "<head><title>dbEdit</title></head>"
                        "<body>"
                        "<h2><i>Edit entry in app</i></h2>"
                        "<hr><br>Notes:<ul>"
                        "<li>File ID list will not be modified.</li>"
                        "<li>Do not edit any existing files in the data directory while the server is running.</li>"
                        "<li>You will need to select the category as it defaults to the first category.</li>"
                        "</ul><form action=\"/dbEdit/"+table+"/edit/form2\" method=\"get\">"
                        "<select name=\"id\">"+select+"</select> "
                        "<input type=\"submit\" value=\"Edit\">"
                        "</body>"
                        "</html>");
                    return;
                }
                else if (list.size() == 4 && list[3] == "form2" && table == "app") {
                    std::string id = "N";
                    for (unsigned int i = 0; i < getParams.size(); i++) {
                        std::string fnlower = getParams[i];
                        boost::algorithm::to_lower(fnlower);
                        if (fnlower.find("id=") != std::string::npos) {
                            getParams[i].erase(0, 3);
                            id = getParams[i];
                            break;
                        }
                    }
                    if (id != "N") {
                        std::vector<std::vector<std::string>> results;
                        std::vector<std::string> params;
                        std::string select;
                        params.push_back(id);
                        results = server->db->SQLiteExecute("select * from app where id = ?", params);
                        boost::replace_all(results[0][9], "\n", "\r\n");
                        for (unsigned int i = 0; i < server->categories.size(); i++) {
                            std::string idstr = boost::lexical_cast<std::string>((int)(server->categories[i].id));
                            select += "<option value=\""+idstr+"\">"+idstr+" - "+server->categories[i].name+"</option>";
                        }
                        if (results.size() > 0) {
                            sendPage("<html>"
                                "<head><title>dbEdit</title></head>"
                                "<body>"
                                "<h2><i>Edit entry in app</i></h2>"
                                "<hr><br><table>"
                                "<form action=\"/dbEdit/"+table+"/edit/confirm/"+server->httpSessionRandom+"\" method=\"get\">"
                                "<tr><td>id:</td><td><input type=\"text\" name=\"id\" value=\""+results[0][0]+"\" readonly></td></tr>"
                                "<tr><td>title_id:</td><td><input type=\"text\" name=\"title_id\" value=\""+results[0][1]+"\" maxlength=\"9\"></td></tr>"
                                "<tr><td>date:</td><td><input type=\"number\" name=\"date\" value=\""+results[0][2]+"\" min=\"0\" max=\"4294967295\"></td></tr>"
                                "<tr><td>category:</td><td><select name=\"category\">"+select+"</select><br>"
                                    "&nbsp;<font size=\"2\">Current value: "+results[0][3]+"</font></td></tr>"
                                "<tr><td>displayname:</td><td><input type=\"text\" name=\"displayname\" value=\""+results[0][4]+"\"></td></tr>"
                                "<tr><td>filename:</td><td><input type=\"text\" name=\"filename\" value=\""+results[0][5]+"\"></td></tr>"
                                "<tr><td>author:</td><td><input type=\"text\" name=\"author\" value=\""+results[0][6]+"\"></td></tr>"
                                "<tr><td>version:</td><td><input type=\"text\" name=\"version\" value=\""+results[0][7]+"\"></td></tr>"
                                "<tr><td>weburl:</td><td><input type=\"text\" name=\"weburl\" value=\""+results[0][8]+"\"></td></tr>"
                                "<tr><td>description:</td><td><textarea name=\"description\" rows=\"10\" cols=\"68\">"+results[0][9]+"</textarea></td></tr>"
                                "<tr><td>dlcount:</td><td><input type=\"number\" name=\"dlcount\" value=\""+results[0][10]+"\" min=\"0\" max=\"4294967295\"></td></tr>"
                                //"<tr><td>cachedrating:</td><td></td></tr>"
                                "<tr><td>extra:</td><td><input type=\"text\" name=\"extra\" value=\""+results[0][12]+"\"></td></tr>"
                                "<tr><td></td><td><input type=\"submit\" value=\"Submit\"></td></tr>"
                                "</table></body>"
                                "</html>");
                            return;
                        }
                    }
                }
                else if (list.size() == 5 && list[3] == "confirm" && list[4] == server->httpSessionRandom && table == "app") {
                    std::map<std::string, std::string> paramsMap;
                    for (unsigned int i = 0; i < getParams.size(); i++) {
                        std::string paramName;
                        unsigned int j = 0;
                        for (; j < getParams[i].length(); j++) {
                            if (getParams[i].at(j) == '=') {
                                break;
                            }
                            paramName += getParams[i].at(j);
                        }
                        getParams[i].erase(0, j+1);
                        
                        //Decode getParams[i]
                        std::string dec;
                        boost::replace_all(getParams[i], "+", " ");
                        for (unsigned int j = 0 ; j < getParams[i].length(); j++) {
                            char ch;
                            int ii;
                            if (int(getParams[i][j])==37) {
                                sscanf(getParams[i].substr(j+1, 2).c_str(), "%x", &ii);
                                ch = static_cast<char>(ii);
                                dec += ch;
                                j = j+2;
                            }
                            else {
                                dec+=getParams[i][j];
                            }
                        }
                        boost::replace_all(dec, "\r\n", "\n");
                        getParams[i] = dec;

                        paramsMap[paramName] = getParams[i];
                    }

                    std::string id = "N";
                    for (std::map<std::string, std::string>::iterator it = paramsMap.begin(); it != paramsMap.end(); ++it) {
                        std::string param = it->first;
                        std::string value = it->second;
                        if (param == "id") {
                            id = value;
                            break;
                        }
                    }
                    std::vector<std::string> colNames;
                    std::vector<std::vector<std::string>> results;
                    results = server->db->SQLiteExecute("select sql from sqlite_master where tbl_name = 'app' and type = 'table'");
                    if (results.size() != 0) {
                        int nameCount = 0;
                        bool inName = false;
                        std::string name;
                        for (unsigned int i = 0; i < results[0][0].length(); i++) {
                            if (results[0][0].at(i) == '\"') {
                                if (inName) {
                                    if (nameCount > 0) {
                                        colNames.push_back(name);
                                    }
                                    name.clear();
                                    inName = false;
                                    ++nameCount;
                                }
                                else {
                                    inName = true;
                                }
                            }
                            else if (inName) {
                                name += results[0][0].at(i);
                            }
                        }
                    }
                    if (id != "N") {
                        std::vector<std::string> params;
                        for (std::map<std::string, std::string>::iterator it = paramsMap.begin(); it != paramsMap.end(); ++it) {
                            std::string param = it->first;
                            std::string value = it->second;
                            std::string paramLow = param;
                            std::string colName = "";
                            boost::algorithm::to_lower(paramLow);
                            params.clear();
                            for (int i = 0; colNames.size(); i++) {
                                std::string cnl = colNames[i];
                                boost::algorithm::to_lower(cnl);
                                if (paramLow == cnl) {
                                    colName = colNames[i];
                                    break;
                                }
                            }
                            if (colName != "") {
                                params.push_back(value);
                                params.push_back(id);
                                server->db->SQLiteExecute("UPDATE app SET "+colName+" = ? WHERE id = ?", params);
                            }
                        }
                    }
                    sendPage("<html>"
                        "<head><title>dbEdit</title>"
                        "<meta http-equiv=\"refresh\" content=\"0; url=/\" /></head>"
                        "<body>"
                        "<h2><i>Edit entry in app</i></h2>"
                        "<hr><br>"
                        "<a href=\"/\">Click here if you are not redirected.</a>"
                        "</body>"
                        "</html>");
                    return;
                }
            }
            else if (list.size() >= 3 && list[2] == "delete") {
                if (list.size() == 4 && list[3] == "form" && table == "app") {
                    std::string select;
                    std::vector<std::vector<std::string>> results;
                    results = server->db->SQLiteExecute("select id,displayname,version from app");
                    for (unsigned int i = 0; i < results.size(); i++) {
                        select += "<option value=\""+results[i][0]+"\">"+results[i][0]+" - "+results[i][1]+" ("+results[i][2]+")</option>";
                    }
                    sendPage("<html>"
                        "<head><title>dbEdit</title></head>"
                        "<body>"
                        "<h2><i>Delete entry from app</i></h2>"
                        "<hr><br>Notes:<ul>"
                        "<li>This will not delete the associated file from the data directory.</li>"
                        "<li>All clients currently downloading the selected item will be disconnected.</li>"
                        "<li>File ID list will not be modified.</li>"
                        "</ul><form action=\"/dbEdit/"+table+"/delete/confirm/"+server->httpSessionRandom+"\" method=\"get\">"
                        "<select name=\"id\">"+select+"</select> "
                        "<input type=\"submit\" value=\"Delete\">"
                        "</body>"
                        "</html>");
                    return;
                }
                else if (list.size() == 5 && list[3] == "confirm" && list[4] == server->httpSessionRandom && table == "app") {
                    std::string id = "N";
                    unsigned int idInt;
                    for (unsigned int i = 0; i < getParams.size(); i++) {
                        std::string fnlower = getParams[i];
                        boost::algorithm::to_lower(fnlower);
                        if (fnlower.find("id=") != std::string::npos) {
                            getParams[i].erase(0, 3);
                            id = getParams[i];
                            break;
                        }
                    }
                    if (id != "N") {
                        idInt = stoi(id);
                        for (unsigned int i = 0; i < server->clients.size(); i++) {
                            if (server->clients[i]->zipFileIsOpen && server->clients[i]->zipItemID == idInt) {
                                server->clients[i]->socket.close();
                            }
                        }
                        std::vector<std::string> params;
                        params.push_back(id);
                        server->db->SQLiteExecute("delete from app where id = ?", params);
                        sendPage("<html>"
                            "<head><title>dbEdit</title>"
                            "<meta http-equiv=\"refresh\" content=\"0; url=/\" /></head>"
                            "<body>"
                            "<h2><i>Delete entry from app</i></h2>"
                            "<hr><br>"
                            "<a href=\"/\">Click here if you are not redirected.</a>"
                            "</body>"
                            "</html>");
                        return;
                    }
                }
            }
        }
    }
    //Send error page
    sendPage("<html>"
        "<head><title>Error</title></head>"
        "<body>"
        "Bad URL or an error occurred."
        "</body>"
        "</html>");
}

void Http::sendPage(std::string text) {
    std::vector<unsigned char> sd;
    std::string head = "HTTP/1.0 200 OK\r\n"
                       "Content-Type: text/html\r\n";
    
    head += "Content-Length: " + boost::lexical_cast<std::string>((int)(text.length())) + "\r\n";
    text += "\r\n\r\n";
    head += "\r\n";
    
    addDataToVector(sd, head.c_str(), head.length());
    addDataToVector(sd, text.c_str(), text.length());
    send(sd);
}

Http::~Http(void) {
	print(4, "Http::~Http");
}
