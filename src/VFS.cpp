/*
 Name:		VFS.cpp
 Created:	7/5/2024 10:57:11 AM
 Author:	zhuji
 Editor:	http://www.visualmicro.com
*/

#include "VFS.h"
#include "VFSError.h"
#include "FileError.h"
#include <iostream>
#include <stack>

vfs::Filesystem::Filesystem(SQLite::DbConnection& db) :m_db(db), m_diskMap(db)
{
}

File vfs::Filesystem::m_open(Disk& disk, const std::string& internalFilename, const char* mode)
{
	FS& fs = disk.getFS();
	const std::string internalPath = '/' + internalFilename;

	File file = fs.open(internalPath.c_str(), mode);
	if (!file)
		throw FileError("Failed to open file");

	return file;
}

File vfs::Filesystem::openFile(std::int64_t fileID, const char* mode)
{
	SQLite::SQLStatement stmt = m_db.prepare(
		"SELECT DiskID FROM FileEntries WHERE ID = :fileID"
	).bind(fileID);

	if (!stmt.evaluate())
		throw FileError("File not found", FileAccessInfo(fileID, mode));

	std::int64_t diskID = stmt.getColumnValue<std::int64_t>(0);
	std::string internalFilename = std::to_string(fileID);

	Disk& disk = m_diskMap.getDiskByID(diskID);

	if (!disk.getFS().exists(internalFilename.c_str()))
		throw FileError("File not found on disk", FileAccessInfo(fileID, mode)); // TODO: add optional Disk info to FileError?

	try
	{
		return m_open(disk, internalFilename, mode);
	}
	catch (const FileError& e)
	{
		throw FileError(e.what(), FileAccessInfo(fileID, mode));
	}
}

File vfs::Filesystem::openFile(std::int64_t parentID, const std::string& filename, std::uint64_t size, std::int64_t userID)
{
	const char* mode = FILE_WRITE;
	auto& [diskID, disk] = m_diskMap.getDiskByRequiredSize(size);

	std::int64_t fileID = m_createEntry(parentID, filename, userID, diskID);
	std::string internalFilename = std::to_string(fileID);

	try
	{
		return m_open(disk, internalFilename, mode);
	}
	catch (const FileError& e)
	{
		try
		{
			m_removeFileEntry(fileID);
		}
		catch (const FileError&) {}
		throw FileError(e.what(), FileAccessInfo(fileID, mode), userID);
	}
}

std::vector<vfs::Filesystem::FileMetadata> vfs::Filesystem::listDirectory(std::int64_t directoryID)
{
	std::vector<FileMetadata> result;

	SQLite::SQLStatement stmt = m_db.prepare(
		"SELECT ID, Name, OwnerID, typeof(DiskID), DiskID FROM FileEntries WHERE ParentID = :directoryID"
	);

	stmt.bind(directoryID);

	for (bool hasRow = stmt.evaluate(); hasRow; hasRow = stmt.evaluate())
	{
		FileMetadata metadata;
		metadata.fileID = stmt.getColumnValue<std::int64_t>(0);
		metadata.name = stmt.getColumnValue<std::string>(1);
		metadata.ownerID = stmt.getColumnValue<std::int64_t>(2);
		metadata.isDirectory = stmt.getColumnValue<std::string>(3) == "null"; // TODO: use generated column (https://www.sqlite.org/gencol.html).
		if (!metadata.isDirectory)
		{
			Disk& disk = m_diskMap.getDiskByID(stmt.getColumnValue<std::int64_t>(4));
			FS& fs = disk.getFS();
			std::string internalFilename = std::to_string(metadata.fileID);
			File file = m_open(disk, internalFilename, FILE_READ);
			metadata.size = file.size();
			metadata.lastModified = file.getLastWrite();
		}
		result.emplace_back(metadata);
	}

	return result;
}

std::int64_t vfs::Filesystem::m_createEntry(std::int64_t parentID, const std::string& filename, std::int64_t ownerID, std::int64_t diskID)
{
	// NOTE: enforce composite unique for (parentID, Name)
	SQLite::SQLStatement stmt = m_db.prepare(
		"INSERT INTO FileEntries (ParentID, Name, OwnerID, DiskID) "
		"VALUES (:parentID, :filename, :ownerID, :diskID) "
		"RETURNING ID"
	);
	stmt.bind(parentID, filename, ownerID, diskID);

	if (!stmt.evaluate())
		throw FileError("Failed to acquire file ID", FileAccessInfo(parentID, FILE_WRITE));

	return stmt.getColumnValue<std::int64_t>(0);
}

void vfs::Filesystem::m_removeFileEntry(std::int64_t fileID)
{
	m_db.prepare(
		"DELETE FROM FileEntries WHERE ID = :fileID"
	).bind(fileID).evaluate();
}

void vfs::Filesystem::m_removePhysicalFile(std::int64_t diskID, const std::string& internalFilename)
{
	Disk& disk = m_diskMap.getDiskByID(diskID);
	FS& fs = disk.getFS();
	const std::string internalPath = '/' + internalFilename;
	if (!fs.remove(internalPath.c_str()))
		throw FileError("Failed to remove file", FileAccessInfo(diskID, FILE_WRITE));
}

// checks recursively
bool vfs::Filesystem::hasPermission(std::int64_t fileID, std::int64_t userID, const char* mode)
{
	//SQLite::SQLStatement stmt = m_db.prepare(
	//	"WITH RECURSIVE FileTree (FileID, OwnerID, ParentID) AS ("
	//	"SELECT ID AS FileID, OwnerID, ParentID FROM FileEntries "
	//	"UNION ALL "
	//	"SELECT fe.ID AS FileID, fe.OwnerID, fe.ParentID FROM FileEntries fe "
	//	"JOIN FileTree ft ON fe.ID = ft.ParentID"
	//	") "
	//	"SELECT CASE "
	//	"WHEN EXISTS ("
	//	"SELECT 1 FROM FileTree ft "
	//	"WHERE ft.OwnerID = :userID "
	//	"OR EXISTS ("
	//	"SELECT 1 FROM UserFilePermissions ufp "
	//	"WHERE ufp.FileID = ft.FileID AND ufp.UserID = :userID AND ufp.Permission = :permission"
	//	")"
	//	") THEN 1 ELSE 0 END"
	//);
	//stmt.bind(userID, mode);

	//if (!stmt.evaluate())
	//	throw FileError("No permission", FileAccessInfo(fileID, mode), userID);

	//return stmt.getColumnValue<bool>(0);

	SQLite::SQLStatement stmtOwner = m_db.prepare(
		"SELECT ParentID, OwnerID, typeof(ParentID) FROM FileEntries WHERE ID = :fileID"
	);

	SQLite::SQLStatement stmtPermissionRecord = m_db.prepare(
		"SELECT 1 FROM UserFilePermissions WHERE UserID = :userID AND FileID = :fileID AND Permission = :permission"
	);

	//std::int64_t currentFileID = fileID;
	//bool hasParent;
	//do
	//{
	//	stmtOwner.bind(currentFileID);
	//	if (!stmtOwner.evaluate())
	//		throw FileError("File not found", FileAccessInfo(fileID, mode), userID);

	//	if (stmtOwner.getColumnValue<std::int64_t>(1) == userID)
	//		return true;

	//	stmtPermissionRecord.bind(userID, currentFileID, mode);
	//	if (stmtPermissionRecord.evaluate())
	//		return true;

	//	hasParent = stmtOwner.getColumnValue<std::string>(2) == "integer";
	//	currentFileID = stmtOwner.getColumnValue<std::int64_t>(0);
	//	stmtOwner.reset();
	//	stmtPermissionRecord.reset();
	//} while (hasParent);

	for (auto [hasParent, currentFileID] = std::pair(true, fileID); hasParent; hasParent = stmtOwner.getColumnValue<std::string>(2) == "integer", currentFileID = stmtOwner.getColumnValue<std::int64_t>(0), stmtOwner.reset(), stmtPermissionRecord.reset())
	{
		stmtOwner.bind(currentFileID);
		if (!stmtOwner.evaluate())
			throw FileError("File not found", FileAccessInfo(fileID, mode), userID);

		if (stmtOwner.getColumnValue<std::int64_t>(1) == userID)
			return true;

		stmtPermissionRecord.bind(userID, currentFileID, mode);
		if (stmtPermissionRecord.evaluate())
			return true;
	}

	return false;
}

bool vfs::Filesystem::hasOwnership(std::int64_t fileID, std::int64_t userID)
{
	SQLite::SQLStatement stmt = m_db.prepare(
		"SELECT ParentID, OwnerID, typeof(ParentID) FROM FileEntries WHERE ID = :fileID"
	);

	for (auto [hasParent, currentFileID] = std::pair(true, fileID); hasParent; hasParent = stmt.getColumnValue<std::string>(2) == "integer", currentFileID = stmt.getColumnValue<std::int64_t>(0), stmt.reset())
	{
		stmt.bind(currentFileID);
		if (!stmt.evaluate())
			throw FileError("File not found", FileAccessInfo(fileID, FILE_WRITE), userID);

		if (stmt.getColumnValue<std::int64_t>(1) == userID)
			return true;
	}

	return false;
}

std::vector<std::int64_t> vfs::Filesystem::getVirtualPath(std::int64_t fileID)
{
	std::vector<std::int64_t> result;

	SQLite::SQLStatement stmt = m_db.prepare(
		"SELECT ParentID, typeof(ParentID) FROM FileEntries WHERE ID = :fileID"
	);

	//bool hasParent;
	//std::int64_t currentFileID = fileID;
	//do
	//{
	//	stmt.bind(currentFileID);
	//	if (!stmt.evaluate())
	//		throw FileError("File not found", FileAccessInfo(fileID, FILE_READ));
	//	result.emplace_back(currentFileID);
	//	currentFileID = stmt.getColumnValue<std::int64_t>(0);
	//	hasParent = stmt.getColumnValue<std::string>(1) == "integer";
	//	stmt.reset();
	//} while (hasParent);

	for (auto [hasParent, currentFileID] = std::pair(true, fileID); hasParent; hasParent = stmt.getColumnValue<std::string>(1) == "integer", currentFileID = stmt.getColumnValue<std::int64_t>(0), stmt.reset())
	{
		stmt.bind(currentFileID);
		if (!stmt.evaluate())
			throw FileError("File not found", FileAccessInfo(fileID, FILE_READ));

		result.emplace(result.begin(), currentFileID);
	}

	return result;
}

std::int64_t vfs::Filesystem::getRootDirectoryID(std::int64_t userID)
{
	SQLite::SQLStatement stmt = m_db.prepare(
		"SELECT ID FROM FileEntries WHERE ParentID IS NULL AND OwnerID = :userID"
	);

	stmt.bind(userID);

	if (!stmt.evaluate())
		throw FileError("Root directory not found", std::nullopt, userID);

	return stmt.getColumnValue<std::int64_t>(0);
}

vfs::Disk& vfs::Filesystem::getDisk(std::int64_t fileID)
{
	SQLite::SQLStatement stmt = m_db.prepare(
		"SELECT DiskID, typeof(DiskID) FROM FileEntries WHERE ID = :fileID"
	);
	stmt.bind(fileID);

	if (!stmt.evaluate())
		throw FileError("File not found", FileAccessInfo(fileID, FILE_READ));

	bool isNull = stmt.getColumnValue<std::string>(1) == "null";
	if (isNull)
		throw FileError("File is not a physical file", FileAccessInfo(fileID, FILE_READ));

	std::int64_t diskID = stmt.getColumnValue<std::int64_t>(0);
	return m_diskMap.getDiskByID(diskID);
}

bool vfs::Filesystem::isDirectory(std::int64_t fileID)
{
	SQLite::SQLStatement stmt = m_db.prepare(
		"SELECT typeof(DiskID) FROM FileEntries WHERE ID = :fileID" // TODO: use generated column (https://www.sqlite.org/gencol.html)
	);
	stmt.bind(fileID);

	if (!stmt.evaluate())
		throw FileError("File not found", FileAccessInfo(fileID, FILE_READ));

	return stmt.getColumnValue<std::string>(0) == "null";
}

std::string vfs::Filesystem::getInternalPath(std::int64_t fileID)
{
	return '/' + std::to_string(fileID);
}

void vfs::Filesystem::createNewDirectoryEntry(std::int64_t parentID, const std::string entryName, std::int64_t ownerID)
{
	if (!hasPermission(parentID, ownerID, FILE_WRITE))
		throw FileError("Permission denied", FileAccessInfo(parentID, FILE_WRITE), ownerID);

	m_db.prepare(
		"INSERT INTO FileEntries (ParentID, Name, OwnerID) "
		"VALUES (:parentID, :entryName, :ownerID)"
	).bind(parentID, entryName, ownerID).evaluate();
}

void vfs::Filesystem::createRootDirectoryEntry(std::int64_t ownerID)
{
	// check if root directory already exists
	SQLite::SQLStatement stmt = m_db.prepare(
		"SELECT 1 FROM FileEntries WHERE OwnerID = :ownerID AND ParentID IS NULL"
	);

	stmt.bind(ownerID);

	if (stmt.evaluate())
		throw FileError("Root directory already exists", std::nullopt, ownerID);

	m_db.prepare(
		"INSERT INTO FileEntries (Name, OwnerID) "
		"VALUES ('/', :ownerID)"
	).bind(ownerID).evaluate();
}

void vfs::Filesystem::removeFileEntry(std::int64_t fileID, std::int64_t userID)
{
	if (!hasOwnership(fileID, userID))
		throw FileError("Permission denied", FileAccessInfo(fileID, FILE_WRITE), userID);

	m_removeFileEntry(fileID);
}

void vfs::Filesystem::renameFileEntry(std::int64_t fileID, const std::string& newName, std::int64_t userID)
{
	if (!hasOwnership(fileID, userID))
		throw FileError("Permission denied", FileAccessInfo(fileID, FILE_WRITE), userID);

	m_db.prepare(
		"UPDATE FileEntries SET Name = :newName WHERE ID = :fileID"
	).bind(newName, fileID).evaluate();
}

std::string vfs::Filesystem::getExtension(const std::string& filename)
{
	size_t pos = filename.find_last_of('.');
	if (pos == std::string::npos)
		return "";

	return filename.substr(pos);
}

void vfs::Filesystem::preupdateCallback(vfs::Filesystem& vfs, SQLite::DbConnection& db, SQLite::DbConnection::ColumnUpdateType operationType, const std::string& dbName, const std::string& tableName, std::int64_t rowid, std::optional<std::int64_t> newRowid)
{
	if (operationType != SQLite::DbConnection::ColumnUpdateType::DELETE) // Not a delete operation
		return;

	SQLite::SQLValue oldDiskID = db.preupdateOld(2);
	if (oldDiskID.type() == SQLite::SQLValue::Type::NULL_) // Not a physical file
		return;

	SQLite::SQLValue oldID = db.preupdateOld(0);

	std::string internalFilename = std::to_string(oldID.get<std::int64_t>());
	vfs.m_removePhysicalFile(oldDiskID.get<std::int64_t>(), internalFilename);
}
