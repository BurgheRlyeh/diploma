#include "CSVIterator.h"

CSVIterator CSVIterator::begin() const {
	return *this;
}

CSVIterator CSVIterator::end() const {
	return CSVIterator{};
}

CSVIterator& CSVIterator::operator++() {
	if (m_str && !((*m_str) >> m_row))
		m_str = nullptr;
	return *this;
}

CSVIterator CSVIterator::operator++(int) {
	CSVIterator tmp(*this);
	++(*this);
	return tmp;
}

CSVRow const& CSVIterator::operator*() const {
	return m_row;
}
CSVRow const* CSVIterator::operator->() const {
	return &m_row;
}

bool CSVIterator::operator==(CSVIterator const& rhs) {
	return (this == &rhs) || (!this->m_str && !rhs.m_str);
}
bool CSVIterator::operator!=(CSVIterator const& rhs) {
	return !((*this) == rhs);
}
