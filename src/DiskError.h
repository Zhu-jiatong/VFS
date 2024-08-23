#pragma once

#include "VFSError.h"
#include <optional>

namespace vfs {
	class Disk;
	class DiskError;
}

class vfs::DiskError :public VFSError
{
public:
	DiskError(const std::string& message) : VFSError(message) {};
	DiskError(const std::string& message, Disk& disk) : VFSError(message), m_disk(disk) {};

	Disk& getDisk() { return m_disk.value(); }
	bool hasDisk() { return m_disk.has_value(); }

private:
	std::optional<std::reference_wrapper<Disk>> m_disk;
};