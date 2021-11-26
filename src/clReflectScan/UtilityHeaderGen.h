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
		std::vector<cldb::TypeInheritance> TypeInheritanceList;
		bool isInitialized = false;
	};
}

class ASTConsumer;

class UtilityHeaderGen
{
private:

	static const int PRIMITIVE_KIND_TYPE_GENERATING_GENERATED_H_FILE =
		cldb::Primitive::Kind::KIND_CLASS | // struct or class
		cldb::Primitive::Kind::KIND_ENUM |
		cldb::Primitive::Kind::KIND_TEMPLATE // template type
		//cldb::Primitive::Kind::KIND_TEMPLATE_TYPE // this is template instantiation
		;

	thread_local static std::map<cldb::u32, ksj::BaseTypeList> BaseTypeList; // key : Derived Class's Name Hash, Value : Base Classes of Derived Class's Name Hash

	static const std::vector<cldb::TypeInheritance>& GetBaseTypesName(const cldb::u32 searchDerivedClassNameHash, cldb::Database& db);

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
		ASTConsumer& astConsumer,
		std::vector<cldb::Name>& baseChainTypeNameList
	);
	
	//return macros name
	std::string WriteInheritanceInformationMacros
	(
		CodeGen& cg,
		const std::vector<cldb::Name>& baseChainList,
		const cldb::Name& targetClassFullName,
		const cldb::Name& rootclass_typename,
		const std::string& macrobableClassFullTypeName,
		cldb::Database& db,
		const bool isClass
	);
	
	// Write Macros of Class Type to CodeGen
	void WriteClassMacros
	(
		CodeGen& cg, 
		const cldb::Type* const targetClassPrimitive, 
		const std::string& rootclass_typename, 
		const bool isLastType, 
		cldb::Database& db,
		ASTConsumer& astConsumer
	);

	//return macros name
	std::string WriteCurrentTypeAliasMacros(CodeGen& cg, const cldb::Name& targetClassFullName, const std::string& macrobableClassFullTypeName, const bool isClass);
	std::string WriteCurrentTypeStaticHashValueAndFullName(CodeGen& cg, const cldb::Name& targetClassFullName, const std::string& targetClassShortName, const std::string& macrobableClassFullTypeName, const bool isClass);
	std::string WriteTypeCheckFunction(CodeGen& cg, const cldb::Name& targetClassFullName, const std::string& macrobableClassFullTypeName, const bool isClass);
	std::string WriteCloneObject(CodeGen& cg, const cldb::Name& targetClassFullName, const std::string& macrobableClassFullTypeName, const std::string& rootclass_typename, const bool isClass);
	std::string WriteAbstractCloneObject(CodeGen& cg, const cldb::Name& targetClassFullName, const std::string& macrobableClassFullTypeName, const std::string& rootclass_typename, const bool isClass);

	std::vector<cldb::Primitive*> FindTargetTypesName(const std::string& sourceFilePath, const std::string& headerFilePath, ASTConsumer& astConsumer, cldb::Database& db);
	
	//Why need this? : Writing reflection.h file make compiler recompile sourcefile
	static bool CheckReflectionFileChanged(const std::wstring& outputPath, CodeGen& newlyCreatedReflectionFile);

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