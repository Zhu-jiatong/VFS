#pragma once

#include "VFSError.h"
#include <optional>
#include <cstdint>

namespace vfs {
	class FileError;
	struct FileAccessInfo;
}

struct vfs::FileAccessInfo
{
	std::int64_t fileID;
	const char* mode;
};

class vfs::FileError :public VFSError
{
public:
	FileError(
		const std::string& message,
		std::optional<FileAccessInfo> file = std::nullopt,
		std::optional<std::int64_t> userID = std::nullopt
	) :
		VFSError(message),
		m_file(file),
		m_userID(userID)
	{
	}

	//getters
	const FileAccessInfo& getFile() const { return m_file.value(); }
	const std::int64_t& getUserID() const { return m_userID.value(); }

	bool hasFile() const { return m_file.has_value(); }
	bool hasUserID() const { return m_userID.has_value(); }

private:
	std::optional<FileAccessInfo> m_file;
	std::optional<std::int64_t> m_userID;
};