#pragma once

#include "Disk.h"
#include "SQLitePlusPlus.h"
#include <unordered_map>

namespace vfs {
	class DiskMap;
}

class vfs::DiskMap
{
public:
	DiskMap(SQLite::DbConnection& db) :m_db(db)
	{
	}

	void mountDisk(std::int64_t diskID, Disk disk);
	Disk& getDiskByID(std::int64_t diskID);
	std::pair<const int64_t, Disk>& getDiskByRequiredSize(std::int64_t requiredBytes);

private:
	SQLite::DbConnection& m_db;
	std::unordered_map<std::int64_t, Disk> m_fsHandleMap;
};