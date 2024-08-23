#pragma once

#include <exception>

namespace vfs {
	class VFSError;
}

class vfs::VFSError : public std::runtime_error
{
public:
	VFSError(const std::string& message) : std::runtime_error(message) {}
};