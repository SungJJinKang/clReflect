//
// ===============================================================================
// clReflect, UtilityHeaderGen.cpp - First pass traversal of the clang AST for C++,
// locating reflection specifications.
// -------------------------------------------------------------------------------
// Copyright : SungJJinKang
// ===============================================================================
//


#include "UtilityHeaderGen.h"

#include <clReflectCore/Logging.h>
#include <clReflectCore/FileUtils.h>
#include <clcpp/clcpp.h>
#include "ASTConsumer.h"
#include <clang/AST/ASTContext.h>

#include <cassert>

using namespace cldb;

thread_local std::map<cldb::u32, ksj::BaseTypeList> UtilityHeaderGen::BaseTypeList{};

UtilityHeaderGen::UtilityHeaderGen()
{
	LOG_TO_STDOUT(utilityHeaderGen, ALL);
	LOG_TO_STDOUT(warnings, INFO);

	
}

std::vector<cldb::Name> UtilityHeaderGen::GetBaseTypesName(const cldb::u32 searchDerivedClassNameHash, cldb::Database& db)
{
	ksj::BaseTypeList& baseTypeList = BaseTypeList[searchDerivedClassNameHash];
	if (baseTypeList.isInitialized == false)
	{
		//This can be optimized if add std::map<u32, std::vector<u32>> variable to cldb::Database
		//It require some work in cldb::Database::AddTypeInheritance function
		for (cldb::DBMap<cldb::TypeInheritance>::const_iterator i = db.m_TypeInheritances.begin(); i != db.m_TypeInheritances.end(); ++i)
		{
			// This code can be slow.
			// But I want to edit existing codes as little as possible.

			// cldb::DBMap<cldb::TypeInheritance>'s TypeInheritance guarantee uniqueness. it never have same typeInheritance
			const cldb::TypeInheritance& inherit = i->second;

			if (searchDerivedClassNameHash == inherit.derived_type.hash) // hash guarantee uniqueness
			{
				baseTypeList.BaseTypeNameList.push_back(inherit.base_type);
			}
		}

		baseTypeList.isInitialized = true;
	}
	
	return baseTypeList.BaseTypeNameList;
}

std::string UtilityHeaderGen::ConvertTypeNameToMacrobableTypeName(const std::string & fullTypeName)
{
	// "::" can't be contained in macros, so we use "__"
	std::string macrobableTypeName = fullTypeName;
	for (size_t i = 0; i < macrobableTypeName.size(); i++)
	{
		if (macrobableTypeName[i] == ':')
		{
			macrobableTypeName[i] = '_';
		}
	}

	return macrobableTypeName;
}

std::string UtilityHeaderGen::ConvertFullTypeNameToShortTypeName(const std::string& fullTypeName)
{
	std::string shorTypeName = fullTypeName;

	const size_t lastNamespaceSpecialCharacter = fullTypeName.find_last_of("::");
	if (lastNamespaceSpecialCharacter != std::string::npos)
	{
		shorTypeName = fullTypeName.substr(lastNamespaceSpecialCharacter + 2);
	}

	return shorTypeName;
}

bool UtilityHeaderGen::GenerateBaseChainList_RecursiveFunction
(
	const cldb::Name targetClassName,
	const cldb::Name targetRootClassName,
	cldb::Database& db,
	std::vector<cldb::Name>& baseChainTypeNameList
)
{
	if (targetClassName == targetRootClassName)
	{
		//If root class
		baseChainTypeNameList.push_back(targetRootClassName);
		return true;
	}
	else
	{
		std::vector<cldb::Name> baseClassList = GetBaseTypesName(targetClassName.hash, db);
		if (baseClassList.empty() == false)
		{
			for (cldb::Name& baseClassName : baseClassList)
			{
				const bool result = GenerateBaseChainList_RecursiveFunction(baseClassName, targetRootClassName, db, baseChainTypeNameList);
				if (result == true)
				{
					// Root class is found while travel recursive function!!
					baseChainTypeNameList.push_back(baseClassName);
					return true;
				}
			}
		}

		// everything fails...
		return false;
	}	
}

/*
cldb::Name UtilityHeaderGen::FindTargetClass(const std::string & className, cldb::Database & db)
{
	std::vector<cldb::Name> targetClassCandidate;

	for (DBMap<Type>::iterator iter = db.m_Types.begin(); iter != db.m_Types.end() ; iter++)
	{
		if (iter->second.name.text.find_last_of(className) != std::string::npos)
		{
			targetClassCandidate.push_back(iter->second.name);
		}
	}
	return cldb::Name();
}
*/

std::string UtilityHeaderGen::WriteInheritanceInformationMacros
(
	CodeGen& cg,
	const cldb::Name& targetClassFullName,
	const cldb::Name& rootclass_typename,
	const std::string& macrobableClassFullTypeName,
	cldb::Database& db
)
{

	std::string baseChainMacros;


	std::vector<cldb::Name> baseChainList;
	// if root class, return false
	const bool isSuccess = GenerateBaseChainList_RecursiveFunction
	(
		targetClassFullName,
		rootclass_typename,
		db,
		baseChainList
	);

	//if class is rootclass or inherited from root class, it never return false and empty list.
	if (isSuccess == true && baseChainList.empty() == false)
	{
		cg.Line();

		std::string baseChainText;
		//baseChainText+=
		for (int index = 0; index < baseChainList.size() - 1; index++) // don't use size_t, it can make underflow
		{
			baseChainText += std::to_string(baseChainList[index].hash);
			baseChainText += ", ";
		}
		baseChainText += std::to_string(baseChainList[baseChainList.size() - 1].hash);

		baseChainMacros = "INHERITANCE_INFORMATION_" + macrobableClassFullTypeName;
		cg.Line("#undef %s", baseChainMacros.c_str());
		cg.Line("#define %s \\", baseChainMacros.c_str());
		cg.Line
		(
			"private: inline static const unsigned long int BASE_CHAIN_LIST[] { %s }; \\", 
			baseChainText.c_str()
		); // unsigned long guarantee 32bit

		if (baseChainList.size() >= 2)
		{
			cg.Line("typedef %s Base", baseChainList[1].text.c_str());
		}
	}

	return baseChainMacros;
}


void UtilityHeaderGen::WriteClassMacros
(
	CodeGen& cg, 
	const cldb::Class* const targetClassPrimitive,
	const std::string& rootclass_typename, 
	cldb::Database & db
)
{
	const std::string targetClassShortTypeName = ConvertFullTypeNameToShortTypeName(targetClassPrimitive->name.text);

	const std::string macrobableClassFullTypeName = ConvertTypeNameToMacrobableTypeName(targetClassPrimitive->name.text); //test
	const std::string macrobableClassShortTypeName = ConvertTypeNameToMacrobableTypeName(targetClassShortTypeName); //test

	// define full name macros. you should wrtie namespace with this. ex) GENERATE_BODY_test_base_chain__G
	const std::string fullNamebodyMacros = "GENERATE_BODY_" + macrobableClassFullTypeName;
	

	cg.Line("#ifdef %s", fullNamebodyMacros.c_str());
	cg.Line("#error \"%s already included....\"", fullNamebodyMacros.c_str());
	cg.Line("#endif");

	std::vector<std::string> macrosNameList;
	
	if (rootclass_typename.empty() == false)
	{
		// 3. generate base chain data ( implemented 100% )

		cldb::Name rootclass_cldb_typename = db.GetName(rootclass_typename.c_str());

		const std::string baseChainListMacros = WriteInheritanceInformationMacros
		(
			cg,
			targetClassPrimitive->name,
			rootclass_cldb_typename,
			macrobableClassFullTypeName,
			db
		);

		if (baseChainListMacros.empty() == false)
		{
			macrosNameList.push_back(baseChainListMacros);
		}

	}

	// Define Current Type Alias
	const std::string CurrentTypeAliasMacrosName = WriteCurrentTypeAliasMacros(cg, targetClassPrimitive->name, macrobableClassFullTypeName);
	macrosNameList.push_back(CurrentTypeAliasMacrosName);

	const std::string CurrentTypeStaticHashValueAndFullName = WriteCurrentTypeStaticHashValueAndFullName(cg, targetClassPrimitive->name, targetClassShortTypeName, macrobableClassFullTypeName);
	macrosNameList.push_back(CurrentTypeStaticHashValueAndFullName);
	// 4. generate reflection variable, function, static functions, static variable

	cg.Line();
	cg.Line();
	cg.Line("#undef %s", fullNamebodyMacros.c_str());
	cg.Line("#define %s \\", fullNamebodyMacros.c_str());

	for (const std::string& macro : macrosNameList)
	{
		cg.Line("%s \\", macro.c_str());
	}

	cg.Line();
	cg.Line();

	// define short name macros for programer. you can except namespace with this. ex) GENERATE_BODY_G
	const std::string shortNamebodyMacros = "GENERATE_BODY_" + macrobableClassShortTypeName;
	cg.Line("#define %s %s", shortNamebodyMacros.c_str(), fullNamebodyMacros.c_str());
}


std::string UtilityHeaderGen::WriteCurrentTypeAliasMacros(CodeGen & cg, const cldb::Name& targetClassFullName, const std::string & macrobableClassFullTypeName)
{
	assert(targetClassFullName.text.empty() == false);
	assert(targetClassFullName.hash != 0);
	assert(macrobableClassFullTypeName.empty() == false);

	cg.Line();
	cg.Line();
	const std::string CurrentTypeAliasMacrosName = "CURRENT_TYPE_ALIAS_" + macrobableClassFullTypeName;
	cg.Line("#undef %s", CurrentTypeAliasMacrosName.c_str());
	cg.Line("#define %s \\", CurrentTypeAliasMacrosName.c_str());
	cg.Line("typedef %s Current", targetClassFullName.text.c_str());

	return CurrentTypeAliasMacrosName;
}

std::string UtilityHeaderGen::WriteCurrentTypeStaticHashValueAndFullName(CodeGen & cg, const cldb::Name & targetClassFullName, const std::string& targetClassShortName, const std::string & macrobableClassFullTypeName)
{
	assert(targetClassFullName.text.empty() == false);
	assert(targetClassFullName.hash != 0);
	assert(macrobableClassFullTypeName.empty() == false);

	cg.Line();
	cg.Line();
	const std::string CurrentTypeStaticHashValueAndFullName = "TYPE_FULLNAME_HASH_VALUE_NAME_STRING_" + macrobableClassFullTypeName;
	cg.Line("#undef %s", CurrentTypeStaticHashValueAndFullName.c_str());
	cg.Line("#define %s \\", CurrentTypeStaticHashValueAndFullName.c_str());
	cg.Line("inline static const unsigned long int TYPE_FULL_NAME_HASH_VALUE = %d; \\", targetClassFullName.hash);
	cg.Line("inline static const char* const TYPE_FULL_NAME = %d; \\", targetClassFullName.text.c_str());
	cg.Line("inline static const char* const TYPE_SHORT_NAME = %d; ", targetClassShortName.c_str());

	return CurrentTypeStaticHashValueAndFullName;
}

std::vector<cldb::Primitive*> UtilityHeaderGen::FindTargetTypesName
(
	const std::string& sourceFilePath,
	const std::string& headerFilePath, 
	ASTConsumer& astConsumer, 
	cldb::Database& db
)
{
	std::vector<cldb::Primitive*> targetTypesNameList;

	const std::map<std::string, std::vector<cldb::Primitive*>>& declLocationMap = astConsumer.GetSourceFilePathOfDeclMap();

	// find declrations in source file
	auto iter = declLocationMap.find(sourceFilePath);
	if (iter != declLocationMap.end())
	{
		for (cldb::Primitive* const primitive : iter->second)
		{
			assert(primitive != nullptr);
			if (UtilityHeaderGen::PRIMITIVE_KIND_TYPE_GENERATING_GENERATED_H_FILE && primitive->kind != 0)
			{
				// find UtilityHeader's target type!
				targetTypesNameList.emplace_back(primitive);
			}
		}
	}
	
	// find declrations in header file
	iter = declLocationMap.find(headerFilePath);
	if (iter != declLocationMap.end())
	{
		for (cldb::Primitive* const primitive : iter->second)
		{
			assert(primitive != nullptr);
			if (UtilityHeaderGen::PRIMITIVE_KIND_TYPE_GENERATING_GENERATED_H_FILE && primitive->kind != 0)
			{
				// find UtilityHeader's target type!
				targetTypesNameList.emplace_back(primitive);
			}
		}

	}

	return targetTypesNameList;
}



void UtilityHeaderGen::GenUtilityHeader
(
	const std::string& sourceFilePath, 
	const std::string& rootclass_typename, 
	cldb::Database & db,
	ASTConsumer& astConsumer
)
{
	if (sourceFilePath.empty() == true || sourceFilePath[0] == ' ')
	{
		return;
	}

	size_t lastBackSlashPos = sourceFilePath.find_last_of('\\');
	if (lastBackSlashPos == std::string::npos)
	{
		lastBackSlashPos = sourceFilePath.find_last_of('/');
	}

	const std::string SourceFileNameWithExtension = (lastBackSlashPos != std::string::npos)
		? sourceFilePath.substr(lastBackSlashPos + 1) : SourceFileNameWithExtension;

	std::string SourceFileNameWithoutExtension = SourceFileNameWithExtension; // TargetClassName
	size_t extensionDotPos = SourceFileNameWithExtension.find_last_of('.');
	if (lastBackSlashPos != std::string::npos)
	{
		SourceFileNameWithoutExtension = std::string(SourceFileNameWithoutExtension.begin(), SourceFileNameWithoutExtension.begin() + extensionDotPos);
	}


	extensionDotPos = sourceFilePath.find_last_of('.');
	if (extensionDotPos != std::string::npos)
	{
		const std::string targetHeaderFilePath = std::string{ sourceFilePath.begin(), sourceFilePath.begin() + extensionDotPos } +".h";
		const std::string outputPath = std::string{ sourceFilePath.begin(), sourceFilePath.begin() + extensionDotPos } +"_generated.h";

		// Check types declared in outputPath
		// Check record_decl->getLocation()


		// 1. check if rootclass_typename exist looping db::types
		// find all classes and enums and structs.. declared in sourceFilePath.h

		// 2. define macros like GENERATED_BODY_DOOMS_GRAPHICS_GRAPHICS_SERVER(). "::" can't be contained in macros, so we use "__"

		// 2. generate hash value and template function like clreflect_compiletime_gettype.cpp


		CodeGen cg;

		// Generate arrays
		cg.Line("#pragma once");
		cg.Line();
		cg.Line("// Utility Header File ( Don't Edit this )");
		cg.Line("SourceFilePath : %s", sourceFilePath.c_str());
		cg.Line();
		cg.Line();

		cg.Line("#ifdef %s", outputPath.c_str());
		cg.Line("#error \"%s already included, missing '#pragma once' in %s\"", outputPath.c_str(), targetHeaderFilePath.c_str());
		cg.Line("#endif");

		std::vector<cldb::Primitive*> UtilityHeaderTargetTypeList = FindTargetTypesName(sourceFilePath, targetHeaderFilePath, astConsumer, db);

		for (cldb::Primitive* const utilityHeaderTargetType : UtilityHeaderTargetTypeList)
		{
			switch (utilityHeaderTargetType->kind)
			{
			case cldb::Primitive::Kind::KIND_CLASS:
				WriteClassMacros(cg, static_cast<cldb::Class*>(utilityHeaderTargetType), rootclass_typename, db);
				break;

			default:
				break;
			}
			

			cg.Line();
			cg.Line();
			cg.Line("-------------------------------------------");
			cg.Line();
			cg.Line();
		}

		cg.WriteToFile(outputPath.c_str());
	}
	else
	{
		LOG(warnings, INFO, "fail to generate UtilityHeader output path");
	}


	
	/*
	const cldb::Name targetClassName = 

	
	CodeGen cg;

	// Generate arrays
	cg.Line("// Utility Header File ( Don't Edit this )");
	cg.Line("static const int clcppNbTypes = %d;", primitives.size());
	cg.Line("static const clcpp::Type* clcppTypePtrs[clcppNbTypes] = { 0 };");
	cg.Line();

	// Generate initialisation function
	cg.Line("#ifndef GENERATE_BODY"); // If this macros is written in first line of header file, following macros will be ingnored
	cg.Line("#define GENERATE_BODY \\");

	cg.Line("void clcppInitGetType(const clcpp::Database* db)");
	cg.EnterScope();
	cg.Line("// Populate the type pointer array if a database is specified");
	cg.Line("if (db != 0)");
	cg.EnterScope();
	for (size_t i = 0; i < primitives.size(); i++)
		cg.Line("clcppTypePtrs[%d] = db->GetType(0x%x);", i, primitives[i].hash);
	cg.ExitScope();
	cg.ExitScope();
	cg.Line();

	ForwardDeclareTypes(cg, namespaces);

	// Generate the implementations
	cg.Line("// Specialisations for GetType and GetTypeNameHash");
	cg.Line("namespace clcpp");
	cg.EnterScope();
	//GenGetTypes(cg, primitives, PT_Type | PT_Class | PT_Struct | PT_EnumClass | PT_EnumStruct);
	cg.Line("#if defined(CLCPP_USING_MSVC)");
	GenGetTypes(cg, primitives, PT_Enum);
	cg.Line("#endif");
	cg.ExitScope();


	const std::string fileID = "FILE_ID_" + SourceFileNameWithoutExtension;
	cg.Line("#undef D_FILE_ID");
	cg.Line("#define D_FILE_ID %s", fileID.c_str());

	cg.Line("#undef GENERATE_BODY ");
	cg.Line("#define GENERATE_BODY \\");
	cg.Line("BASE_CHAIN_DATA \\");

	*/

	
}

// 개발 방향 :
// 클래스 명과 파일 명이 무조건 같게 강제하자.
// 같은 파일명의 클래스 없으면 그냥 통과
// 클래스 명과 Argument로 넘어오는 파일 명이 같은 경우를 찾으면 Reflection 데이터 생성

// 여기서 그냥 BaseChain도 완전히 생성해버리자.
// 어차피 Base 타고 올라가면 Base의 Hash값 모두 컴파일 타임에 만들어 낼 수 있다..
// ex) BaseChain[] = { 현재 클래스 Hash 값, 부모 클래스 Hash 값.... };

// GENERATE_BODY 매크로도 만들자.

// Reflection 구조체를 가져오는 Static 함수 정의도 넣자.

// 타입명은 같은데 네임스페이스가 다른 경우도 고려되어야한다.

//개발 방향 1 : clscan하는 소스파일에서 확장자만 h로 바꾸어서 해당 header에 선언되어 있는 모든 클래서, 구조체, enum 등 generated.h가 필요한 타입들에 대해 하나의 파일에 다 담자. ( Nice )
//
//	고려 사항 : header만 있고 소스파일이 없는 경우? -> 이 경우 generated.h 생성 불가 -> 소스 파일이 없는 경우 에러를 발생시켜야한다.
//
//개발 방향 2 : clexport에서 생성 -> 다시 생성해야하는 것을 구분하지 못하는 문제가 발생 -> 결국 모든 header파일에 대한 generated_h를 다시 생성해야한다 -> 끔찍.