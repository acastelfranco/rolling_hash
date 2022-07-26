#pragma once

#include <stdexcept>

class DeltaException : public std::runtime_error
{
public:
	DeltaException(const std::string &msg) : std::runtime_error(msg) {}
	virtual ~DeltaException() {};
};

class SignatureException : public std::runtime_error
{
public:
	SignatureException(const std::string &msg) : std::runtime_error(msg) {}
	virtual ~SignatureException() {}
};

class MalformedFileException : public std::runtime_error
{
public:
	MalformedFileException(const std::string &msg) : std::runtime_error(msg) {}
	virtual ~MalformedFileException() {}
};