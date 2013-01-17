/*
** file.cpp
**
**---------------------------------------------------------------------------
** Copyright 2011 Braden Obrzut
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
**
*/

#ifdef WINDOWS
#define USE_WINDOWS_DWORD
#define USE_WINDOWS_BOOLEAN
#include <windows.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#endif

#include <cstdio>
#include "file.h"
#include "zstring.h"

File::File(const FString &filename)
{
	init(filename);
}

File::File(const File &dir, const FString &filename)
{
#ifdef WINDOWS
	init(dir.getDirectory() + '\\' + filename);
#else
	init(dir.getDirectory() + '/' + filename);
#endif
}

void File::init(const FString &filename)
{
	this->filename = filename;
	directory = false;
	existing = false;
	writable = false;

#ifdef WINDOWS
	/* Windows, why must you be such a pain?
	 * Why must you require me to add a '*' to my filename?
	 * Why must you design your API around the do...while loop?
	 * Why???
	 */
	DWORD fAttributes = GetFileAttributes(filename);
	if(fAttributes != INVALID_FILE_ATTRIBUTES)
	{
		existing = true;
		if(fAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			directory = true;
			WIN32_FIND_DATA fdata;
			HANDLE hnd = INVALID_HANDLE_VALUE;
			hnd = FindFirstFile((filename + "\\*").GetChars(), &fdata);
			if(hnd != INVALID_HANDLE_VALUE)
			{
				do
				{
					files.Push(fdata.cFileName);
				}
				while(FindNextFile(hnd, &fdata) != 0);
			}
		}
	}

	// Can't find an easy way to test writability on Windows so
	writable = true;
#else
	struct stat statRet;
	if(stat(filename, &statRet) == 0)
		existing = true;

	if(existing)
	{
		if((statRet.st_mode & S_IFDIR))
		{
			directory = true;

			// Populate a base list.
			DIR *direct = opendir(filename);
			if(direct != NULL)
			{
				dirent *file = NULL;
				while((file = readdir(direct)) != NULL)
					files.Push(file->d_name);
			}
			closedir(direct);
		}

		// Check writable
		if(access(filename, W_OK) == 0)
			writable = true;
	}
#endif
}

FString File::getDirectory() const
{
	if(directory)
	{
		if(filename[filename.Len()-1] == '\\' || filename[filename.Len()-1] == '/')
			return filename.Left(filename.Len()-1);
		return filename;
	}

	long dirSepPos = MAX(filename.LastIndexOf('/'), filename.LastIndexOf('\\'));
	if(dirSepPos != -1)
		return filename.Left(dirSepPos);
	return FString(".");
}

FString File::getInsensitiveFile(const FString &filename, bool sensitiveExtension) const
{
#if defined(WINDOWS) || defined(__APPLE__)
	// Insensitive filesystem, so just return the filename
	return filename;
#else
	const TArray<FString> &files = getFileList();
	FString extension = filename.Mid(filename.LastIndexOf('.')+1);

	for(unsigned int i = 0;i < files.Size();++i)
	{
		if(files[i].CompareNoCase(filename) == 0)
		{
			if(!sensitiveExtension || files[i].Mid(files[i].LastIndexOf('.')+1).Compare(extension) == 0)
				return files[i];
		}
	}
	return filename;
#endif
}

void File::rename(const FString &newname)
{
#ifdef WINDOWS
	FString dirName = getDirectory() + '\\';
#else
	FString dirName = getDirectory() + '/';
#endif

	::rename(filename, dirName + newname);
}
