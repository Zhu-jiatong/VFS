#pragma once

#include <FS.h>
#include <functional>
#include <string>
#include "DiskError.h"

class vfs::Disk
{
public:
	using SizeGetter = std::function<std::uint64_t()>;

	Disk(FS& fs, const std::string& mountpoint, SizeGetter getTotalSize, SizeGetter getUsedSize) :
		m_fs(fs),
		m_mountpoint(mountpoint),
		m_getTotalSize(getTotalSize),
		m_getUsedSize(getUsedSize)
	{
	}

	// setters
	void setFS(FS& fs) { m_fs = fs; }
	void setMountpoint(const std::string& mountpoint) { m_mountpoint = mountpoint; }
	void setTotalSizeGetter(SizeGetter getter) { m_getTotalSize = getter; }
	void setUsedSizeGetter(SizeGetter getter) { m_getUsedSize = getter; }

	// getters
	FS& getFS() { return m_fs; }
	const std::string& getMountpoint() const { return m_mountpoint; }
	std::uint64_t getTotalSize() const { return m_getTotalSize(); }
	std::uint64_t getUsedSize() const { return m_getUsedSize(); }
	std::uint64_t getFreeSize() const { return getTotalSize() - getUsedSize(); }

private:
	FS& m_fs;
	std::string m_mountpoint;
	SizeGetter m_getTotalSize;
	SizeGetter m_getUsedSize;
};