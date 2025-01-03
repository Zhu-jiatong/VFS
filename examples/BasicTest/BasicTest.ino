/*
 Name:		BasicTest.ino
 Created:	7/16/2024 3:23:03 PM
 Author:	zhuji
*/

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SD.h>
#include "VFS.h"
#include <FileError.h>
#include <iostream>

// the setup function runs once when you press reset or power the board
void setup()
{
	Serial.begin(115200);

	try
	{
		if (!SD.begin())
			throw std::runtime_error("SD card initialization failed!");

		if (SD.exists("/vfs.db"))
			SD.remove("/vfs.db");

		SQLite::DbConnection db("/sd/vfs.db");
		initialiseDatabase(db);

		vfs::Filesystem vfs(db);
		db.beforeRowUpdate<vfs::Filesystem>(vfs::Filesystem::preupdateCallback, vfs);
		vfs::Disk Disk1(SD, "/sd", []() {return SD.totalBytes(); }, []() {return SD.usedBytes(); });

		vfs.getDiskMap().mountDisk(0, Disk1);

		db.prepare("INSERT INTO Users VALUES (?, ?, ?)").bind<std::int64_t, std::string, std::vector<uint8_t>>(0, "Tony", std::vector<uint8_t>({ 0, 1 })).evaluate();

		vfs.createRootDirectoryEntry(0);
		std::int64_t rootID = vfs.getRootDirectoryID(0);

		std::cout << "Root ID: " << rootID << std::endl;

		vfs.createNewDirectoryEntry(rootID, "personal", 0);
		{
			File file = vfs.openFile(rootID, "personal.txt", 0, 0);
			file.println("Hello, world!");
		}

		printDirectory(rootID, vfs);

		vfs.renameFileEntry(3, "personal2", 0);

		printDirectory(rootID, vfs);

		try
		{
			std::cout << vfs.getDisk(3).getMountpoint() << std::endl;
			std::cout << vfs.getDisk(2).getMountpoint() << std::endl; // should throw an exception
		}
		catch (const vfs::FileError& e)
		{
			std::cerr << e.what() << " FILE: " << e.getFile().fileID << std::endl;
		}
		catch (const std::exception& e)
		{
			std::cerr << "This should not happen: " << e.what() << std::endl; // should not be printed
		}

		std::cout << vfs.isDirectory(3) << std::endl; // false
		std::cout << vfs.isDirectory(2) << std::endl; // true

		std::vector<std::int64_t> path = vfs.getVirtualPath(2);
		for (auto& i : path)
			std::cout << i << '/';
		std::cout << '\n';

		std::vector<std::int64_t> path2 = vfs.getVirtualPath(3);
		for (auto& i : path2)
			std::cout << i << '/';
		std::cout << '\n';

		bool hasPermission = vfs.hasPermission(3, 0, FILE_READ);
		std::cout << "Has permission: " << hasPermission << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
}

// the loop function runs over and over again until power down or reset
void loop()
{

}


void initialiseDatabase(SQLite::DbConnection& db)
{
	db.prepare(
		"PRAGMA foreign_keys = ON"
	).evaluate();

	db.prepare(
		"CREATE TABLE IF NOT EXISTS Disks ("
		"ID INTEGER PRIMARY KEY,"
		"Mountpoint TEXT NOT NULL UNIQUE"
		")"
	).evaluate();

	db.prepare(
		"CREATE TABLE IF NOT EXISTS Users ("
		"ID INTEGER PRIMARY KEY,"
		"Username TEXT NOT NULL UNIQUE,"
		"PasswordHash BLOB NOT NULL"
		")"
	).evaluate();

	db.prepare(
		"CREATE TABLE IF NOT EXISTS FileEntries ("
		"ID INTEGER PRIMARY KEY AUTOINCREMENT,"
		"OwnerID INTEGER NOT NULL,"
		"DiskID INTEGER,"
		"ParentID INTEGER,"
		"Name TEXT,"

		"UNIQUE (ParentID, Name),"
		"FOREIGN KEY (OwnerID) REFERENCES Users(ID) ON DELETE CASCADE,"
		"FOREIGN KEY (DiskID) REFERENCES Disks(ID) ON DELETE CASCADE,"
		"FOREIGN KEY (ParentID) REFERENCES FileEntries(ID) ON DELETE CASCADE"
		")"
	).evaluate();

	db.prepare(
		"CREATE TABLE IF NOT EXISTS UserFilePermissions ("
		"UserID INTEGER NOT NULL,"
		"FileID INTEGER NOT NULL,"
		"Permission TEXT NOT NULL,"

		"PRIMARY KEY (UserID, FileID, Permission),"
		"FOREIGN KEY (UserID) REFERENCES Users(ID) ON DELETE CASCADE,"
		"FOREIGN KEY (FileID) REFERENCES FileEntries(ID) ON DELETE CASCADE"
		")"
	).evaluate();
}

void printDirectory(std::int64_t parentID, vfs::Filesystem& vfs)
{
	std::vector<vfs::Filesystem::FileMetadata> files = vfs.listDirectory(parentID);

	for (auto& file : files)
	{
		std::cout << file.fileID << ' ' << file.name << ' ' << file.ownerID << ' ' << std::boolalpha << file.isDirectory << ' ';
		if (!file.isDirectory)
			std::cout << file.size << ' ' << file.lastModified;
		std::cout << '\n';
	}
}