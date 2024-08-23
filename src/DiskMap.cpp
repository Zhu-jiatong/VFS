#include "DiskMap.h"
#include <algorithm>
#include "DiskError.h"

void vfs::DiskMap::mountDisk(std::int64_t diskID, Disk disk)
{
	m_db.prepare(
		"INSERT INTO Disks (ID, Mountpoint) "
		"VALUES (:diskID, :mountpoint)"
	).bind(diskID, disk.getMountpoint()).evaluate();

	m_fsHandleMap.try_emplace(diskID, disk);
}

vfs::Disk& vfs::DiskMap::getDiskByID(std::int64_t diskID)
{
	return m_fsHandleMap.at(diskID);
}

std::pair<const int64_t, vfs::Disk>& vfs::DiskMap::getDiskByRequiredSize(std::int64_t requiredBytes)
{
	// find disk with minimum free space greater than requiredBytes
	using DiskEntry = std::pair<const std::int64_t, Disk>;
	std::function<bool(const DiskEntry&, const DiskEntry&)> comp = [requiredBytes](const DiskEntry& current, const DiskEntry& smallest)
		{
			std::uint64_t freeSpaceCurrent = current.second.getFreeSize();
			if (freeSpaceCurrent < requiredBytes)
				return false;

			std::uint64_t freeSpaceSmallest = smallest.second.getFreeSize();
			return freeSpaceCurrent < freeSpaceSmallest;
		};

	std::unordered_map<std::int64_t, Disk>::iterator it = std::ranges::min_element(m_fsHandleMap, comp);

	if (it == m_fsHandleMap.end())
		throw DiskError("No disk mounted");

	if (it->second.getFreeSize() < requiredBytes)
		throw DiskError("No disk with enough free space");

	return *it;
}