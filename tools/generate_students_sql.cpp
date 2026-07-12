#include <fstream>
#include <iostream>
#include <string>

int main() {
    const std::string output_file_name = "students_10000.sql";
    std::ofstream output(output_file_name);

    if (!output) {
        std::cerr << "Could not open " << output_file_name << " for writing.\n";
        return 1;
    }

    output << "CREATE TABLE students ("
           << "id INTEGER PRIMARY KEY AUTOINCREMENT, "
           << "name STRING, "
           << "index_number INTEGER, "
           << "year INTEGER, "
           << "average_grade INTEGER"
           << ");\n";

    output << "CREATE INDEX students_index_number_idx ON students (index_number);\n";
    output << "CREATE INDEX students_year_idx ON students (year);\n";

    for (int i = 1; i <= 10000; ++i) {
        const int year = ((i - 1) % 4) + 1;
        const int average_grade = 60 + (i % 41);
        const int index_number = 2020000 + i;
        const std::string name = "Student " + std::to_string(i);

        output << "INSERT INTO students (name, index_number, year, average_grade) "
               << "VALUES ('" << name << "', "
               << index_number << ", "
               << year << ", "
               << average_grade << ");\n";
    }

    output << "SELECT * FROM students WHERE index_number = 2024321;\n";

    std::cout << "Generated " << output_file_name << "\n";
    return 0;
}
