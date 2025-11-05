#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <filesystem>
#include <chrono>
#include <map>

using namespace std;



// 1. Повертає таймстамп у форматі YYYY-MM-DD_HH-MM-SS
std::string getTimestamp() {
    std::ostringstream oss;
    std::time_t now = std::time(nullptr);
    std::tm tmNow{};
#ifdef _WIN32
    localtime_s(&tmNow, &now);
#else
    localtime_r(&now, &tmNow);
#endif
    oss << std::put_time(&tmNow, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

// 2. Створює лог-файл у папці "logs" і повертає відкритий ofstream
std::unique_ptr<std::ofstream> createLogFile() {
    filesystem::create_directories("logs"); // створює папку, якщо її нема
    std::string filename = "logs/log_" + getTimestamp() + ".txt";

    auto file = std::make_unique<std::ofstream>(filename, std::ios::out);

    if (!file->is_open()) {
        std::cerr << "Не вдалося створити файл: " << filename << std::endl;
    }
    else {
        std::cout << "Створено лог: " << filename << std::endl;
    }

    return file; // повертаємо унікальний вказівник
}

// 3. Записує рядок у лог-файл
void writeToLog(std::ofstream& file, const std::string& text) {
    if (file.is_open()) {
        file << text << std::endl;
    }
}

enum LogLevel {
	TRACE,
    INFO,
	WARNING,
	LE_ERROR,
	DEBUG
};
enum endStaus {
    NOT_FINISHED,
	SUCCESS, 
	FAILURE,
};

struct handleData_LogEye {
    int id;
    char msg[2][256];
	LogLevel level;
    int hash[3];
};

class Log {
	
    int id;
    string name;
    // vector map <string, string>
	vector <map<string, string>> logEntries;  // msg type : text  << if bool == true | else write msg2 only
	LogLevel level;
	endStaus status = NOT_FINISHED;
    string Timestamp;

public:
    Log(int logId, LogLevel logLevel) : id(logId), level(logLevel) {
		// get current timestamp
		Timestamp = getTimestamp();
    };

    string textTrace() {
        // to be implemented
    };

	int getId() {
		return id;
	};
    void pushBackMsg(map<string, string> entry) {
		logEntries.push_back(entry);
    }
    void setStatus(endStaus newStatus) {
        status = newStatus;
        return;
    };
};

class LogEye {
    vector<Log> logs;
    unique_ptr<ofstream> logFile;

    // parameters
    bool fileSink = true;
    bool consoleSink = true;
    // more to come...


public:
    LogEye() {
        if (fileSink) {
            logFile = createLogFile();
        }
    }

    ~LogEye() {
        if (logFile && logFile->is_open()) {
            logFile->close();
        }
    }

    void LogTrace(int id, string name) {
		Log newLog(id, TRACE);
		logs.push_back(newLog);

        return;
    };
	void msgTrace(int logId, string msg0, string msg1, bool isTrace) {
        for (auto& log : logs) {
			if (log.getId() == logId) {
                map<string, string> entry = {
                    {"msg0", msg0},
                    {"msg1", msg1},
                    {"isTrace", isTrace ? "true" : "false"}
                };
				log.pushBackMsg(entry);
				return;
			}
        };
	};
    void endTrace(int logId, endStaus status, string Comment = "#") {
        for (auto& log : logs) {
            if (log.getId() == logId) {
                log.setStatus(status);
                map<string, string> entry = {
                    {"Comment", Comment}
                };
				log.pushBackMsg(entry);
                return;
            };
        };

    }
    void log(int id, LogLevel level, string msg) {  // NOT TRACE
		Log newLog(id, level);
		map<string, string> entry = {
	        {"msg", msg}
		};
		newLog.setStatus(SUCCESS); // default status
		newLog.pushBackMsg(entry);
    };
};