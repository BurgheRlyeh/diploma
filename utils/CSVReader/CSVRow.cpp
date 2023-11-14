#include "CSVRow.h"

std::string_view CSVRow::operator[](std::size_t index) const {
	return std::string_view(&m_line[m_data[index] + 1], m_data[index + 1] - (m_data[index] + 1));
}

std::size_t CSVRow::size() const {
	return m_data.size() - 1;
}

void CSVRow::next(std::istream& str) {
	std::getline(str, m_line);

	m_data.clear();
	m_data.emplace_back(-1);
	std::string::size_type pos{};
	while ((pos = m_line.find(',', pos)) != std::string::npos)
		m_data.emplace_back(pos++);

	m_data.emplace_back(m_line.size());
}

std::istream& operator>>(std::istream& str, CSVRow& data) {
	data.next(str);
	return str;
}
