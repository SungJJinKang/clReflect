#pragma once

//
// ===============================================================================
// clReflect, UtilityHeaderGen.h - First pass traversal of the clang AST for C++,
// locating reflection specifications.
// -------------------------------------------------------------------------------
// Copyright : SungJJinKang
// ===============================================================================
//

#include "clReflectCore/Database.h"
#include "clReflectCore/CodeGen.h"

namespace ksj
{
	struct BaseClassList
	{
		std::vector<cldb::u32> NameHashList;
		bool isInitialized = false;
	};
}

class UtilityHeaderGen
{
private:

	thread_local static std::map<cldb::u32, ksj::BaseClassList> BaseClassList; // key : Derived Class's Name Hash, Value : Base Classes of Derived Class's Name Hash

	static std::vector<cldb::u32> GetBaseClassesName(const cldb::u32 searchDerivedClassNameHash, cldb::Database& db);

	// "::" can't be contained in macros, so we use "__"
	static std::string ConvertNameToMacrobableName(const std::string& name);
	
	// return BaseChainList in baseChainList
	// RootClass is stored at first pos of baseChainList
	// return true if root class is found in class hierarchy, or return false
	//
	// this function doesn't support multiple inheritance
	bool GenerateBaseChainList
	(
		const cldb::u32 targetClassNameHash,
		const cldb::u32 targetRootClassNameHash,
		cldb::Database& db,
		std::vector<cldb::u32>& baseChainList
	);
	void WriteBaseChainList(CodeGen& cg, const std::vector<cldb::u32>& baseChainList);
	cldb::Name FindTargetClass(const std::string& className, cldb::Database& db);
	
public:

	//thread_local static std::map<cldb::u32, 

	UtilityHeaderGen();

	//output of UtilityHeader should be placed at superjacent of target class
	void GenUtilityHeader
	(
		const std::string& sourceFilePath, 
		const std::string& rootclass_typename,
		cldb::Database& db
	);

};