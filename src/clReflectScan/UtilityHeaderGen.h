#pragma once

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

	static std::vector<cldb::Name> GetBaseClassesName(const cldb::Name& searchDerivedClassName, cldb::Database& db);
	
	
	// return BaseChainList in baseChainList
	// RootClass is stored at first pos of baseChainList
	// return true if root class is found in class hierarchy, or return false
	//
	// this function doesn't support multiple inheritance
	bool GenerateBaseChainList
	(
		const cldb::Name& targetClassName,
		const cldb::Name& targetRootClassName,
		cldb::Database& db,
		std::vector<cldb::Name>& baseChainList
	);
	void WriteBaseChainList(CodeGen& cg, const std::vector<cldb::Name>& baseChainList);
	
public:

	UtilityHeaderGen();

	//output of UtilityHeader should be placed at first line of header file
	void GenUtilityHeader(const std::string& sourceFilePath, const std::string& outputFilePath, cldb::Database& db);

};