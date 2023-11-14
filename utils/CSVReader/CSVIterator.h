#pragma once

#include "CSVRow.h"

class CSVIterator {
	std::istream* m_str;
	CSVRow m_row;

public:
	CSVIterator() : m_str(nullptr) {}
	CSVIterator(std::istream& str) : m_str(str.good() ? &str : nullptr) {
		++(*this);
	}

	CSVIterator begin() const;
	CSVIterator end() const;

	CSVIterator& operator++();
	CSVIterator operator++(int);

	CSVRow const& operator*() const;
	CSVRow const* operator->() const;

	bool operator==(CSVIterator const& rhs);
	bool operator!=(CSVIterator const& rhs);
};