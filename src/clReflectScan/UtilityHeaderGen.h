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
	struct BaseTypeList
	{
		std::vector<cldb::Name> BaseTypeNameList;
		bool isInitialized = false;
	};
}

class ASTConsumer;

class UtilityHeaderGen
{
private:

	static const cldb::Primitive::Kind PRIMITIVE_KIND_TYPE_GENERATING_GENERATED_H_FILE =
		cldb::Primitive::Kind(
		cldb::Primitive::Kind::KIND_CLASS | // struct or class
		cldb::Primitive::Kind::KIND_ENUM |
		cldb::Primitive::Kind::KIND_TEMPLATE // template type
		//cldb::Primitive::Kind::KIND_TEMPLATE_TYPE // this is template instantiation
		);

	thread_local static std::map<cldb::u32, ksj::BaseTypeList> BaseTypeList; // key : Derived Class's Name Hash, Value : Base Classes of Derived Class's Name Hash

	static std::vector<cldb::Name> GetBaseTypesName(const cldb::u32 searchDerivedClassNameHash, cldb::Database& db);

	// "::" can't be contained in macros, so we use "__"
	// '\' or '/' can't be contained in macros, so we use "_"
	static std::string ConvertNameToMacrobableName(const std::string& fullTypeName);

	// return Type Short Name. 
	// Remove all namespace
	// ex) namespaceA::namespaceB::TestClass -> TestClass
	static std::string ConvertFullTypeNameToShortTypeName(const std::string& fullTypeName);
	
	// return BaseChainList in baseChainList
	// RootClass is stored at first pos of baseChainList
	//
	// return true if root class is found in class hierarchy, or return false
	//
	// this function doesn't support multiple inheritance
	bool GenerateBaseChainList_RecursiveFunction
	(
		const cldb::Name targetClassName,
		const cldb::Name targetRootClassName,
		cldb::Database& db,
		std::vector<cldb::Name>& baseChainTypeNameList
	);
	
	//return macros name
	std::string WriteInheritanceInformationMacros
	(
		CodeGen& cg,
		const cldb::Name& targetClassFullName,
		const cldb::Name& rootclass_typename,
		const std::string& macrobableClassFullTypeName,
		cldb::Database& db
	);
	
	// Write Macros of Class Type to CodeGen
	void WriteClassMacros(CodeGen& cg, const cldb::Class* const targetClassPrimitive, const std::string& rootclass_typename, cldb::Database& db);

	//return macros name
	std::string WriteCurrentTypeAliasMacros(CodeGen& cg, const cldb::Name& targetClassFullName, const std::string& macrobableClassFullTypeName);
	std::string WriteCurrentTypeStaticHashValueAndFullName(CodeGen& cg, const cldb::Name& targetClassFullName, const std::string& targetClassShortName, const std::string& macrobableClassFullTypeName);
	std::string WriteTypeCheckFunction(CodeGen& cg, const cldb::Name& targetClassFullName, const std::string& macrobableClassFullTypeName);

	std::vector<cldb::Primitive*> FindTargetTypesName(const std::string& sourceFilePath, const std::string& headerFilePath, ASTConsumer& astConsumer, cldb::Database& db);
	
public:

	//thread_local static std::map<cldb::u32, 

	UtilityHeaderGen();

	// output of UtilityHeader should be placed at superjacent of target class
	// Warning : Evenry header files containing reflected type ( enum, template, class, struct ) must have sourceFile with same file name
	void GenUtilityHeader
	(
		const std::string& sourceFilePath, 
		const std::string& rootclass_typename,
		cldb::Database& db,
		ASTConsumer& astConsumer
	);

};