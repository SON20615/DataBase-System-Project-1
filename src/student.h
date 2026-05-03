#ifndef STUDENT_H
#define STUDENT_H

#include <string>
#include <vector>

struct Student {
    int         id;
    std::string name;
    std::string gender;
    double      gpa;
    double      height;
    double      weight;
};

bool loadStudentsCSV(const std::string& path, std::vector<Student>& out);

#endif
