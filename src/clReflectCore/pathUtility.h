#pragma once

#include <string>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/Path.h>

// convert path to preferred path
// replace double backslash to one backslash
// replace double slash to one slash
// conver to os path style
std::string converToPreferredPath(const llvm::StringRef path)
{
	std::string preferredPath;
	preferredPath.reserve(path.size());
	for (size_t i = 0; i < path.size(); i++)
	{
		if (
			(path[i] == '\\' && path[i + 1] == '\\') ||
			(path[i] == '/' && path[i + 1] == '/')
			)
		{
			preferredPath.push_back('/');
			i++;
		}
		else
		{
			preferredPath.push_back(path[i]);
		}
	}


	llvm::SmallVector<char, 250> nativePath{ }; // 250 is enough to store path
	memset(nativePath.data(), '\0', 250);
	memcpy(nativePath.data(), preferredPath.data(), preferredPath.size() + 1);
	nativePath.set_size(preferredPath.size() + 1);

	assert(nativePath.empty() == false);
	llvm::sys::path::native(nativePath, llvm::sys::path::Style::native);
	//nativePath = sourcePath.data();

	return nativePath.data();
}
