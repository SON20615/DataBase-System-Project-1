#include "student.h"

#include <fstream>
#include <iostream>
#include <sstream>

static bool nextField(std::stringstream& ss, std::string& field) {
    if (!std::getline(ss, field, ',')) return false;
    if (!field.empty() && field.back() == '\r') field.pop_back();
    return true;
}

bool loadStudentsCSV(const std::string& path, std::vector<Student>& out) {
    std::ifstream in(path);
    if (!in.is_open()) {
        std::cerr << "[student] cannot open " << path << '\n';
        return false;
    }

    out.clear();
    out.reserve(100000);

    std::string line;
    bool header = true;
    size_t lineNo = 0;

    while (std::getline(in, line)) {
        ++lineNo;
        if (line.empty()) continue;
        if (header) { header = false; continue; }

        std::stringstream ss(line);
        std::string idStr, name, gender, gpaStr, hStr, wStr;
        if (!nextField(ss, idStr)  || !nextField(ss, name)   ||
            !nextField(ss, gender) || !nextField(ss, gpaStr) ||
            !nextField(ss, hStr)   || !nextField(ss, wStr)) {
            std::cerr << "[student] malformed line " << lineNo << '\n';
            continue;
        }

        Student s;
        try {
            s.id     = std::stoi(idStr);
            s.name   = name;
            s.gender = gender;
            s.gpa    = std::stod(gpaStr);
            s.height = std::stod(hStr);
            s.weight = std::stod(wStr);
        } catch (...) {
            std::cerr << "[student] parse error on line " << lineNo << '\n';
            continue;
        }
        out.push_back(std::move(s));
    }
    return true;
}
