#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <windows.h>
#include <sstream>          // для ostringstream
#include <filesystem>       // для create_directories

using namespace std;

enum LogLevel {
    TRACE,
    INFO,
    WARNING,
    LE_ERROR,
    DEBUG
};

enum endStatus {
    NOT_FINISHED,
    SUCCESS,
    FAILURE
};

map<LogLevel, string> logLevelToString = {
    {TRACE, "TRACE"},
    {INFO, "INFO"},
    {WARNING, "WARNING"},
    {LE_ERROR, "ERROR"},
    {DEBUG, "DEBUG"}
};

map<endStatus, string> endStatusToString = {
    {NOT_FINISHED, "NOT_FINISHED"},
    {SUCCESS, "SUCCESS"},
    {FAILURE, "FAILURE"}
};

// Структура одного запису лога
struct LogEntry {
    string msg0;
    string msg1;
    bool isTrace; // true - повідомлення у трейсі, false - коментар
};

// Клас Log
class Log {
    int id;
    string name;
    LogLevel level;
    endStatus status;
    string timestamp;
    vector<LogEntry> entries;
    // час початку та кінця
	

public:
    chrono::time_point<chrono::system_clock> startTime;
    chrono::time_point<chrono::system_clock> endTime;

    Log(int id, LogLevel level, const string& name)
        : id(id), level(level), name(name), status(NOT_FINISHED)
    {
		startTime = chrono::system_clock::now();
        timestamp = getTimestamp();
    }

    static string getTimestamp() {
        ostringstream oss;
        time_t now = time(nullptr);
        tm tmNow{};
        localtime_s(&tmNow, &now);
        oss << put_time(&tmNow, "%Y-%m-%d_%H-%M-%S");
        return oss.str();
    }

    void pushBackMsg(const string& msg0, const string& msg1, bool isTrace) {
        entries.push_back({ msg0, msg1, isTrace });
    }

    void setStatus(endStatus s) { status = s; }
    int getId() const { return id; }
    LogLevel getLevel() const { return level; }
    endStatus getStatus() const { return status; }

    string formatLog() const {
        ostringstream oss;
        if (level == TRACE) {
            oss << "-----------------------------------------------------\n";
            oss << "================ [ " << timestamp << " ] " << name << " ================\n";
            for (const auto& e : entries) {
                if (e.isTrace)
                    oss << "   - " << e.msg0 << " : " << e.msg1 << "\n";
                else
                    oss << "   > " << e.msg1 << "\n";
            }
            oss << "\n    status: " << endStatusToString.at(status) << "\n";
            oss << "=====================================================\n";
        }
        else {
            if (!entries.empty())
                oss << "> [ " << timestamp << " ] [" << logLevelToString.at(level) << "] " << entries[0].msg1 << "\n";
        }
        return oss.str();
    }
};

// Клас LogEye
class LogEye {
    vector<Log> logs;
    queue<Log> logQueue;
    mutex mtx;
    condition_variable cv;
    unique_ptr<ofstream> logFile;
    bool consoleSink = true;
    bool fileSink = true;
    int nextId = 0;
    thread worker;
    bool stopWorker = false;

public:
    LogEye() {
        SetConsoleCP(1251);
        SetConsoleOutputCP(1251);

        if (consoleSink) {
            if (AllocConsole()) {
                FILE* fp;
                freopen_s(&fp, "CONOUT$", "w", stdout); // перенаправлення stdout
                freopen_s(&fp, "CONIN$", "r", stdin);   // перенаправлення stdin
                SetConsoleTitleA("LogEye Console");     // заголовок нового вікна
            }
        };


        if (fileSink) {
            logFile = createLogFile();
        }

        worker = thread([this]() { this->processQueue(); });
    }

    ~LogEye() {
        {
            lock_guard<mutex> lock(mtx);
            stopWorker = true;
            cv.notify_all();
        }
        if (worker.joinable())
            worker.join();

        if (logFile && logFile->is_open())
            logFile->close();
    }

    int logTrace(const string& name) {
        lock_guard<mutex> lock(mtx);
        int id = nextId++;
        logs.push_back(Log(id, TRACE, name));
        return id;
    }

    void msgTrace(int logId, const string& msg0, const string& msg1, bool isTrace) {
        lock_guard<mutex> lock(mtx);
        for (auto& l : logs) {
            if (l.getId() == logId) {
                l.pushBackMsg(msg0, msg1, isTrace);
                return;
            }
        }
    }

    void commentTrace(int logId, const string& comment) {
        msgTrace(logId, "Comment", comment, false);
    }

    void endTrace(int logId, endStatus status, const string& comment = "#") {
        lock_guard<mutex> lock(mtx);
        for (auto& l : logs) {
            if (l.getId() == logId) {
                l.setStatus(status);
				l.endTime = chrono::system_clock::now();
                // Додати рядок про тривалість
                chrono::duration<double> duration = l.endTime - l.startTime;
                ostringstream oss;
                oss << duration.count() << " seconds";
                l.pushBackMsg("Duration", oss.str(), true);
                // ================
                if (comment != "#") l.pushBackMsg("Comment", comment, false);
                logQueue.push(l);
                cv.notify_one();
                return;
            }
        }
    }

    int log(LogLevel level, const string& msg) {
        lock_guard<mutex> lock(mtx);
        int id = nextId++;
        Log l(id, level, "log");
        l.pushBackMsg("", msg, false);
        l.setStatus(SUCCESS);
        logQueue.push(l);
        cv.notify_one();
        return id;
    }

    int info(const string& msg) { return log(INFO, msg); }
    int warning(const string& msg) { return log(WARNING, msg); }
    int debug(const string& msg) { return log(DEBUG, msg); }
    int error(const string& msg) { return log(LE_ERROR, msg); }

private:
    unique_ptr<ofstream> createLogFile() {
        filesystem::create_directories("logs");
        string filename = "logs/log_" + Log::getTimestamp() + ".txt";
        auto f = make_unique<ofstream>(filename, ios::out);
        if (!f->is_open())
            cerr << "Не вдалося створити файл: " << filename << endl;
        else
            cout << "Створено лог: " << filename << endl;
        return f;
    }

    void processQueue() {
        while (true) {
            unique_lock<mutex> lock(mtx);
            cv.wait(lock, [this] { return !logQueue.empty() || stopWorker; });
            if (stopWorker && logQueue.empty()) break;

            while (!logQueue.empty()) {
                Log l = logQueue.front();
                logQueue.pop();
                lock.unlock();

                printLog(l);

                lock.lock();
            }
        }
    }

    void printLog(const Log& l) {
        string text = l.formatLog();
        if (consoleSink) {
            setConsoleColor(l.getLevel());
            cout << text;
            setConsoleColor(INFO); // reset to white
        }
        if (fileSink && logFile && logFile->is_open()) {
            *logFile << text;
        }
    }

    void setConsoleColor(LogLevel level) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        switch (level) {
        case INFO:
        case DEBUG:
            SetConsoleTextAttribute(hConsole, 15); // білий
            break;
        case WARNING:
            SetConsoleTextAttribute(hConsole, 14); // жовтий
            break;
        case LE_ERROR:
            SetConsoleTextAttribute(hConsole, 12); // червоний
            break;
        case TRACE:
            SetConsoleTextAttribute(hConsole, 10); // зелений для статусу
            break;
        }
    }
};

LogEye logEye;