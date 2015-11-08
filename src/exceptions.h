/*
exceptions.h
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef EXCEPTIONS_HEADER
#define EXCEPTIONS_HEADER

#include <exception>
#include <string>


class BaseException : public std::exception
{
public:
BaseException(const std::string &s) throw() : m_s(s) 
	{}
	~BaseException() throw() {}
	virtual const char * what() const throw()
	{
		return m_s.c_str();
	}
protected:
	std::string m_s;
};

class AsyncQueuedException : public BaseException {
public:
	AsyncQueuedException(const std::string &s): BaseException(s) {}
};

class NotImplementedException : public BaseException {
public:
	NotImplementedException(const std::string &s): BaseException(s) {}
};

class AlreadyExistsException : public BaseException {
public:
	AlreadyExistsException(const std::string &s): BaseException(s) {}
};

class VersionMismatchException : public BaseException {
public:
	VersionMismatchException(const std::string &s): BaseException(s) {}
};

class FileNotGoodException : public BaseException {
public:
	FileNotGoodException(const std::string &s): BaseException(s) {}
};

class SerializationError : public BaseException {
public:
	SerializationError(const std::string &s): BaseException(s) {}
};

class PacketError : public BaseException {
public:
	PacketError(const std::string &s): BaseException(s) {}
};

class LoadError : public BaseException {
public:
	LoadError(const std::string &s): BaseException(s) {}
};

class ContainerFullException : public BaseException {
public:
	ContainerFullException(const std::string &s): BaseException(s) {}
};

class SettingNotFoundException : public BaseException {
public:
	SettingNotFoundException(const std::string &s): BaseException(s) {}
};

class InvalidFilenameException : public BaseException {
public:
	InvalidFilenameException(const std::string &s): BaseException(s) {}
};

class ProcessingLimitException : public BaseException {
public:
	ProcessingLimitException(const std::string &s): BaseException(s) {}
};

class CommandLineError : public BaseException {
public:
	CommandLineError(const std::string &s): BaseException(s) {}
};

class ItemNotFoundException : public BaseException {
public:
	ItemNotFoundException(const std::string &s): BaseException(s) {}
};

class ServerError : public BaseException {
public:
	ServerError(const std::string &s): BaseException(s) {}
};

class KeyValueStorageException : public BaseException
{
public:
	KeyValueStorageException(const std::string &s) :
		BaseException("KeyValueStorageException: " + s) {}
};

class ClientStateError : public BaseException {
public:
	ClientStateError(std::string s): BaseException(s) {}
};

class PrngException : public BaseException {
public:
	PrngException(std::string s): BaseException(s) {}
};

class ModError : public BaseException {
public:
	ModError(const std::string &s): BaseException(s) {}
};


/*
	Some "old-style" interrupts:
*/

class InvalidNoiseParamsException : public BaseException {
public:
	InvalidNoiseParamsException():
		BaseException("One or more noise parameters were invalid or require "
			"too much memory")
	{}

	InvalidNoiseParamsException(const std::string &s):
		BaseException(s)
	{}
};

class InvalidPositionException : public BaseException
{
public:
	InvalidPositionException():
		BaseException("Somebody tried to get/set something in a nonexistent position.")
	{}
	InvalidPositionException(const std::string &s):
		BaseException(s)
	{}
};

class TargetInexistentException : public std::exception
{
	virtual const char * what() const throw()
	{
		return "Somebody tried to refer to something that doesn't exist.";
	}
};

class NullPointerException : public std::exception
{
	virtual const char * what() const throw()
	{
		return "NullPointerException";
	}
};

#endif
