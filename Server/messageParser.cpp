#include "messageParser.h"

msgParser::msgParser(const std::string& strJSON, std::map<std::thread::id, std::map<unsigned int, unsigned int>>& m_sessions, const std::thread::id& threadID)
    : sessions(m_sessions), th_id(threadID) {
    getData(strJSON);
    setData();
    importData();
}
std::string result;

bool msgParser::getData(const std::string& strJSON) {
    Json::CharReaderBuilder builder;
    const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (!reader->parse(strJSON.c_str(), strJSON.c_str() + strJSON.length(), &root, NULL)) {
        return false;
    }

    listKeys = root.getMemberNames();
}

int msgParser::generateSesNumber() {
    unsigned int maxSesNumber = 0;
    for (auto& it1 : sessions) {
        for (auto& it2 : it1.second) {
            if (it2.first > maxSesNumber) {
                maxSesNumber = it2.first;
            }
        }
    }

    return maxSesNumber + 1;
}

const std::pair<unsigned, unsigned> msgParser::checkAndAddSession() {
    auto it = sessions.find(th_id);

    int sesNumber = -1;
    if (it == sessions.end()) {
        sesNumber = generateSesNumber();
        sessions.insert({ th_id, { { sesNumber, 2 } } });
        return std::make_pair(sesNumber, 2);
    }

    unsigned int minth_id = 1;
    for (auto& iter : it->second) {
        if (iter.second == root["id"].asUInt()) {
            return std::make_pair(0, 0);
        }

        if (iter.second > minth_id) {
            minth_id = iter.second;
        }
    }

    sesNumber = generateSesNumber();

    it->second.emplace(std::make_pair(sesNumber, minth_id + 1));
    return std::make_pair(sesNumber, minth_id + 1);
}

bool msgParser::checkCorrectData(unsigned session, unsigned sendedMsgID, bool delData) {
    std::thread::id id = th_id;
    auto it1 = sessions.find(id);
    if (it1 != sessions.end()) {
        auto it2 = (it1->second).find(session);
        if (it2 != it1->second.end()) {
            unsigned msgID = it2->second;
            if (msgID == sendedMsgID) {
                if (delData) {
                    it1->second.erase(it2);
                    if (it1->second.empty()) {
                        sessions.erase(it1);
                    }
                }
                return true;
            }
        }
    }
    return false;
}

int msgParser::cryptMsg(const std::string& str) {
    int res = 0;
    for (int i = 0; i < str.length(); ++i) {
        char symbol = str[i];
        res = res + (symbol - '0') * 2;
    }
    res = res % LIMIT_FOR_INT;
    return res;
}

void msgParser::setData() {
    std::thread::id id = th_id;
    std::string command = root["command"].asString();
    if (root["session"].asString() == "") {
        root.removeMember("session");
        if (command == "HELLO") {
            root["auth_method"] = "plain-text";
        }
        else if (command == "login") {
            std::string login = root["login"].asString(), password = root["password"].asString();
            root.removeMember("login");
            root.removeMember("password");
            if (login.size() <= 3 || password.size() <= 3) {
                root["status"] = "failed";
                root["message"] = "Too small data";
            }
            else {
                const std::pair<unsigned, unsigned>& resSession = checkAndAddSession();
                if (resSession.first == 0) {
                    root["status"] = "failed";
                    root["message"] = "Also login with this msg id";
                    return;
                }

                root["id"] = (unsigned int)resSession.second;
                root["status"] = "ok";
                root["session"] = resSession.first;
            }
        }
    }
    else {
        unsigned session = root["session"].asUInt();
        unsigned sendedMsgID = root["id"].asUInt();
        root["command"] = command + "_reply";
        if (command == "ping") {
            root.removeMember("session");
            if (checkCorrectData(session, sendedMsgID, false)) {
                root["status"] = "ok";
            }
            else {
                root["status"] = "failed";
                root["message"] = "no this data in bd";
            }
            return;
        }
        else if (command == "logout") {
            if (checkCorrectData(session, sendedMsgID, true)) {
                root["status"] = "ok";
            }
            else {
                root["status"] = "failed";
                root["message"] = "no this data in bd";
            }
        }
        else if (command == "message") {
            if (checkCorrectData(session, sendedMsgID, false)) {
                root["status"] = "ok";
                root["client_id"] = cryptMsg(root["body"].asString());
            }
            else {
                root["status"] = "failed";
                root["message"] = "no this data in bd";
            }
            root.removeMember("body");
        }
    }
}

void msgParser::importData() {
    Json::StreamWriterBuilder builder;
    result = Json::writeString(builder, root);
}
