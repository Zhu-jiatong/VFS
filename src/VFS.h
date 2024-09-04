/*
 Name:		VFS.h
 Created:	7/5/2024 10:57:11 AM
 Author:	zhuji
 Editor:	http://www.visualmicro.com
*/

#ifndef _VFS_h
#define _VFS_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

#include "SQLitePlusPlus.h"
#include <string>
#include "DiskMap.h"
#include <variant>

namespace vfs {
	class Filesystem;
}

class vfs::Filesystem
{
public:
	struct FileMetadata
	{
		std::int64_t fileID;
		std::string name;
		bool isDirectory;
		std::uint64_t size;
		std::time_t lastModified;
		std::int64_t ownerID;
	};

	Filesystem(SQLite::DbConnection& db);

	DiskMap& getDiskMap() { return m_diskMap; }

	bool hasPermission(std::int64_t fileID, std::int64_t userID, const char* mode);
	bool hasOwnership(std::int64_t fileID, std::int64_t userID);
	std::vector<std::int64_t> getVirtualPath(std::int64_t fileID);
	std::int64_t getRootDirectoryID(std::int64_t userID);

	void createNewDirectoryEntry(std::int64_t parentID, const std::string entryName, std::int64_t ownerID);
	void createRootDirectoryEntry(std::int64_t ownerID);
	void removeFileEntry(std::int64_t fileID, std::int64_t userID);
	void renameFileEntry(std::int64_t fileID, const std::string& newName, std::int64_t userID);

	File openFile(std::int64_t fileID, const char* mode);
	File openFile(std::int64_t parentID, const std::string& filename, std::uint64_t size, std::int64_t userID);

	std::vector<FileMetadata> listDirectory(std::int64_t directoryID);

	static std::string getExtension(const std::string& filename);
	static void preupdateCallback(vfs::Filesystem& vfs, SQLite::DbConnection& db, SQLite::DbConnection::ColumnUpdateType operationType, const std::string& dbName, const std::string& tableName, std::int64_t rowid, std::optional<std::int64_t> newRowid);

private:
	SQLite::DbConnection& m_db;
	DiskMap m_diskMap;

	File m_open(Disk& disk, const std::string& filename, const char* mode);
	std::int64_t m_createEntry(std::int64_t parentID, const std::string& filename, std::int64_t ownerID, std::int64_t diskID);
	void m_removeFileEntry(std::int64_t fileID); // Only removes the entry from the database, not the file itself
	void m_removePhysicalFile(std::int64_t diskID, const std::string& internalFilename);
};

#endif

