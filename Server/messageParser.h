#include <json/json.h>
#include <thread>
#include <iostream>
#pragma once

#define LIMIT_FOR_INT 1000000

class msgParser {
private:
    Json::Value root;
    Json::Value::Members listKeys;
    std::map<std::thread::id, std::map<unsigned int, unsigned int>>& sessions;
    std::thread::id th_id;
public:
    msgParser(const std::string& strJSON, std::map<std::thread::id, std::map<unsigned int, unsigned int>>& m_sessions, const std::thread::id& threadID);

    std::string result;

    bool getData(const std::string& strJSON);

    int generateSesNumber();

    const std::pair<unsigned, unsigned> checkAndAddSession();

    bool checkCorrectData(unsigned session, unsigned sendedMsgID, bool delData);

    int cryptMsg(const std::string& str);

    void setData();

    void importData();
};
