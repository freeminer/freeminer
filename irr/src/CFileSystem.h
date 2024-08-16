// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IFileSystem.h"
#include "irrArray.h"

namespace irr
{
namespace io
{

class CZipReader;

/*!
	FileSystem which uses normal files and one zipfile
*/
class CFileSystem : public IFileSystem
{
public:
	//! constructor
	CFileSystem();

	//! destructor
	virtual ~CFileSystem();

	//! opens a file for read access
	IReadFile *createAndOpenFile(const io::path &filename) override;

	//! Creates an IReadFile interface for accessing memory like a file.
	IReadFile *createMemoryReadFile(const void *memory, s32 len, const io::path &fileName, bool deleteMemoryWhenDropped = false) override;

	//! Creates an IReadFile interface for accessing files inside files
	IReadFile *createLimitReadFile(const io::path &fileName, IReadFile *alreadyOpenedFile, long pos, long areaSize) override;

	//! Creates an IWriteFile interface for accessing memory like a file.
	IWriteFile *createMemoryWriteFile(void *memory, s32 len, const io::path &fileName, bool deleteMemoryWhenDropped = false) override;

	//! Opens a file for write access.
	IWriteFile *createAndWriteFile(const io::path &filename, bool append = false) override;

	//! Adds an archive to the file system.
	virtual bool addFileArchive(const io::path &filename,
			bool ignoreCase = true, bool ignorePaths = true,
			E_FILE_ARCHIVE_TYPE archiveType = EFAT_UNKNOWN,
			const core::stringc &password = "",
			IFileArchive **retArchive = 0) override;

	//! Adds an archive to the file system.
	virtual bool addFileArchive(IReadFile *file, bool ignoreCase = true,
			bool ignorePaths = true,
			E_FILE_ARCHIVE_TYPE archiveType = EFAT_UNKNOWN,
			const core::stringc &password = "",
			IFileArchive **retArchive = 0) override;

	//! Adds an archive to the file system.
	bool addFileArchive(IFileArchive *archive) override;

	//! move the hirarchy of the filesystem. moves sourceIndex relative up or down
	bool moveFileArchive(u32 sourceIndex, s32 relative) override;

	//! Adds an external archive loader to the engine.
	void addArchiveLoader(IArchiveLoader *loader) override;

	//! Returns the total number of archive loaders added.
	u32 getArchiveLoaderCount() const override;

	//! Gets the archive loader by index.
	IArchiveLoader *getArchiveLoader(u32 index) const override;

	//! gets the file archive count
	u32 getFileArchiveCount() const override;

	//! gets an archive
	IFileArchive *getFileArchive(u32 index) override;

	//! removes an archive from the file system.
	bool removeFileArchive(u32 index) override;

	//! removes an archive from the file system.
	bool removeFileArchive(const io::path &filename) override;

	//! Removes an archive from the file system.
	bool removeFileArchive(const IFileArchive *archive) override;

	//! Returns the string of the current working directory
	const io::path &getWorkingDirectory() override;

	//! Changes the current Working Directory to the string given.
	//! The string is operating system dependent. Under Windows it will look
	//! like this: "drive:\directory\sudirectory\"
	bool changeWorkingDirectoryTo(const io::path &newDirectory) override;

	//! Converts a relative path to an absolute (unique) path, resolving symbolic links
	io::path getAbsolutePath(const io::path &filename) const override;

	//! Returns the directory a file is located in.
	/** \param filename: The file to get the directory from */
	io::path getFileDir(const io::path &filename) const override;

	//! Returns the base part of a filename, i.e. the name without the directory
	//! part. If no directory is prefixed, the full name is returned.
	/** \param filename: The file to get the basename from */
	io::path getFileBasename(const io::path &filename, bool keepExtension = true) const override;

	//! flatten a path and file name for example: "/you/me/../." becomes "/you"
	io::path &flattenFilename(io::path &directory, const io::path &root = "/") const override;

	//! Get the relative filename, relative to the given directory
	path getRelativeFilename(const path &filename, const path &directory) const override;

	EFileSystemType setFileListSystem(EFileSystemType listType) override;

	//! Creates a list of files and directories in the current working directory
	//! and returns it.
	IFileList *createFileList() override;

	//! Creates an empty filelist
	IFileList *createEmptyFileList(const io::path &path, bool ignoreCase, bool ignorePaths) override;

	//! determines if a file exists and would be able to be opened.
	bool existFile(const io::path &filename) const override;

private:
	//! Currently used FileSystemType
	EFileSystemType FileSystemType;
	//! WorkingDirectory for Native and Virtual filesystems
	io::path WorkingDirectory[2];
	//! currently attached ArchiveLoaders
	core::array<IArchiveLoader *> ArchiveLoader;
	//! currently attached Archives
	core::array<IFileArchive *> FileArchives;
};

} // end namespace irr
} // end namespace io
