#pragma once

//#include <iostream>
//#include <iterator>
//#include <fstream>
#include <sstream>
//#include <string>
#include <vector>

class CSVRow {
	std::string m_line;
	std::vector<int> m_data;

public:
	std::string_view operator[](std::size_t index) const;

	std::size_t size() const;

	void next(std::istream& str);
};

std::istream& operator>>(std::istream& str, CSVRow& data);
