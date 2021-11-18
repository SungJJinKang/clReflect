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
#include <algorithm>
#include <exception>

using namespace cldb;


thread_local std::map<cldb::u32, ksj::BaseTypeList> UtilityHeaderGen::BaseTypeList{};

namespace utilityHeaderGenHelper
{
	template <typename PRIMITIVE_TYPE>
	PRIMITIVE_TYPE* FindPrimitive(const cldb::Name name, cldb::Database& db)
	{
		PRIMITIVE_TYPE* foundType = nullptr;

		assert(name.hash == 0);
		assert(name.text.empty() == true);

		auto iter = db.GetDBMap<PRIMITIVE_TYPE>().find(name.hash);
		if (iter != db.GetDBMap<PRIMITIVE_TYPE>().end())
		{
			foundType = &(iter->second);
		}

		return foundType;
	}
}

UtilityHeaderGen::UtilityHeaderGen()
{
	thread_local static bool isLogInitialized = false;
	if (isLogInitialized == false)
	{
		isLogInitialized = true;
		LOG_TO_STDOUT(UtilityHeaderGen, WARNING);
		LOG_TO_STDOUT(UtilityHeaderGen, ERROR);
		LOG_TO_STDOUT(UtilityHeaderGen, INFO);
	}
}

const std::vector<cldb::TypeInheritance>& UtilityHeaderGen::GetBaseTypesName(const cldb::u32 searchDerivedClassNameHash, cldb::Database& db)
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
				//baseTypeList.BaseTypeNameList.push_back(inherit.base_type);
				baseTypeList.TypeInheritanceList.push_back(inherit);
			}
		}

		baseTypeList.isInitialized = true;
	}
	
	return baseTypeList.TypeInheritanceList;
}

std::string UtilityHeaderGen::ConvertNameToMacrobableName(const std::string & fullTypeName)
{
	// "::" can't be contained in macros, so we use "__"
	std::string macrobableTypeName = fullTypeName;
	for (size_t i = 0; i < macrobableTypeName.size(); i++)
	{
		if (macrobableTypeName[i] == ':')
		{
			macrobableTypeName[i] = '_';
		}
		else if (macrobableTypeName[i] == '/')
		{
			macrobableTypeName[i] = '_';
		}
		else if (macrobableTypeName[i] == '\\')
		{
			macrobableTypeName[i] = '_';
		}
		else if (macrobableTypeName[i] == '.')
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
		shorTypeName = fullTypeName.substr(lastNamespaceSpecialCharacter + 1);
	}

	return shorTypeName;
}

bool UtilityHeaderGen::GenerateBaseChainList_RecursiveFunction
(
	const cldb::Name targetClassName,
	const cldb::Name targetRootClassName,
	cldb::Database& db,
	ASTConsumer& astConsumer,
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
		const std::vector<cldb::TypeInheritance>& typeInheritanceList = GetBaseTypesName(targetClassName.hash, db);
		if (typeInheritanceList.empty() == false)
		{
			for (size_t i = 0 ; i < typeInheritanceList.size() ; i++)
			{
				const bool result = GenerateBaseChainList_RecursiveFunction(typeInheritanceList[i].base_type, targetRootClassName, db, astConsumer, baseChainTypeNameList);
				if (result == true)
				{
					auto iter = astConsumer.GetTypeInheritanceDeclararingOrder().find(typeInheritanceList[i].name.hash);
					assert(iter != astConsumer.GetTypeInheritanceDeclararingOrder().end());

					const size_t inheritanceOrder = iter->second;

					if (inheritanceOrder != 0)
					{
						LOG(UtilityHeaderGen, ERROR, "Inherited class ( %s ) of DObject should inherit DObject's subclass at first pos. Please put declaration of inheriting ( %s ) from ( %s ) at firt pos", targetClassName.text.c_str(), typeInheritanceList[i].base_type.text.c_str(), typeInheritanceList[i].derived_type.text.c_str());

						throw std::exception("Inherited class ( %s ) of DObject should inherit DObject's subclass at first pos");
					}

					// TODO : Check inherit at first pos.
					// ex ) class GraphicsServer : public ISingleton, public DObject  <-- This can make problem!! 
					//      Because CastTo is implmented with reinterpret_cast, DObject's virtual pointer should be located at first address of object

					// Root class is found while travel recursive function!!
					baseChainTypeNameList.push_back(targetClassName);
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
	const std::vector<cldb::Name>& baseChainList,
	const cldb::Name& targetClassFullName,
	const cldb::Name& rootclass_typename,
	const std::string& macrobableClassFullTypeName,
	cldb::Database& db,
	const bool isClass
)
{
	std::string baseChainMacros;

	//if class is rootclass or inherited from root class, it never return false and empty list.
	if (baseChainList.empty() == false)
	{
		cg.Line();

		std::string baseChainText;
		//baseChainText+=
		for (int index = baseChainList.size() - 1 ; index > 0 ; index--) // don't use size_t, it can make underflow
		{
			baseChainText += std::to_string(baseChainList[index].hash);
			baseChainText += ", ";
		}
		baseChainText += std::to_string(baseChainList[0].hash);

		baseChainMacros = "INHERITANCE_INFORMATION_" + macrobableClassFullTypeName;
		cg.Line("#undef %s", baseChainMacros.c_str());
		cg.Line("#define %s \\", baseChainMacros.c_str());
		if (isClass == true)
		{
			cg.Line("public : \\");
		}
		cg.Line
		(
			"inline static const unsigned long int BASE_CHAIN_LIST[] { %s }; \\", 
			baseChainText.c_str()
		); // unsigned long guarantee 32bit
		cg.Line
		(
			"inline static const unsigned long int BASE_CHAIN_LIST_LENGTH { %u }; \\",
			baseChainList.size()
		); 
		cg.Line("virtual const unsigned long int* GetBaseChainList() const { return BASE_CHAIN_LIST; } \\"); 
		cg.Line("virtual unsigned long int GetBaseChainListLength() const { return BASE_CHAIN_LIST_LENGTH; } \\");

		if (baseChainList.size() >= 2)
		{
			cg.Line("typedef %s Base;", baseChainList[baseChainList.size() - 2].text.c_str());
		}
	}

	return baseChainMacros;
}


void UtilityHeaderGen::WriteClassMacros
(
	CodeGen& cg, 
	const cldb::Type* const targetClassPrimitive,
	const std::string& rootclass_typename, 
	const bool isLastType,
	cldb::Database & db,
	ASTConsumer& astConsumer
)
{
	const std::string targetClassShortTypeName = ConvertFullTypeNameToShortTypeName(targetClassPrimitive->name.text);

	const std::string macrobableClassFullTypeName = ConvertNameToMacrobableName(targetClassPrimitive->name.text); //test
	const std::string macrobableClassShortTypeName = ConvertNameToMacrobableName(targetClassShortTypeName); //test

	// define full name macros. you should wrtie namespace with this. ex) GENERATE_BODY_test_base_chain__G
	const std::string fullNamebodyMacros = "GENERATE_BODY_FULLNAME_" + macrobableClassFullTypeName;
	

	cg.Line("#ifdef %s", fullNamebodyMacros.c_str());
	cg.Line("#error \"%s already included....\"", fullNamebodyMacros.c_str());
	cg.Line("#endif");

	std::vector<std::string> macrosNameList;
	
	bool isClass = false; // class or struct?
	if (targetClassPrimitive->kind == cldb::Primitive::KIND_CLASS)
	{
		const cldb::Class* classPrimitive = static_cast<const cldb::Class*>(targetClassPrimitive);
		isClass = classPrimitive->is_class; 
	}


	if (rootclass_typename.empty() == false)
	{
		// 3. generate base chain data ( implemented 100% )
		const cldb::Name rootclass_cldb_typename = db.GetName(rootclass_typename.c_str());

		std::vector<cldb::Name> baseChainList;
		// if root class, return false
		const bool isSuccess = GenerateBaseChainList_RecursiveFunction
		(
			targetClassPrimitive->name,
			rootclass_cldb_typename,
			db,
			astConsumer,
			baseChainList
		);

		if (isSuccess == true)
		{
			const std::string baseChainListMacros = WriteInheritanceInformationMacros
			(
				cg,
				baseChainList,
				targetClassPrimitive->name,
				rootclass_cldb_typename,
				macrobableClassFullTypeName,
				db,
				isClass
			);

			if (baseChainListMacros.empty() == false)
			{
				macrosNameList.push_back(baseChainListMacros);
			}



			if (baseChainList[0].text == rootclass_typename.c_str())
			{// if class is inherited from root class

				if (targetClassPrimitive->name.text == rootclass_typename.c_str())
				{
					const std::string CloneObjectMacrosName = WriteAbstractCloneObject(cg, targetClassPrimitive->name, macrobableClassFullTypeName, rootclass_typename, isClass);
					macrosNameList.push_back(CloneObjectMacrosName);
				}
				else
				{
					const std::string CloneObjectMacrosName = WriteCloneObject(cg, targetClassPrimitive->name, macrobableClassFullTypeName, rootclass_typename, isClass);
					macrosNameList.push_back(CloneObjectMacrosName);
				}

			}
		}
	}

	// Define Current Type Alias
	const std::string CurrentTypeAliasMacrosName = WriteCurrentTypeAliasMacros(cg, targetClassPrimitive->name, macrobableClassFullTypeName, isClass);
	macrosNameList.push_back(CurrentTypeAliasMacrosName);

	const std::string CurrentTypeStaticHashValueAndFullNameMacrosName = WriteCurrentTypeStaticHashValueAndFullName(cg, targetClassPrimitive->name, targetClassShortTypeName, macrobableClassFullTypeName, isClass);
	macrosNameList.push_back(CurrentTypeStaticHashValueAndFullNameMacrosName);
	// 4. generate reflection variable, function, static functions, static variable

	
	
	
	

	
	const std::string WriteTypeCheckFunctionMcros = WriteTypeCheckFunction(cg, targetClassPrimitive->name, targetClassShortTypeName, isClass);
	macrosNameList.push_back(WriteTypeCheckFunctionMcros);

	// TODO : CompileType GetType template function like gettype.cpp file's

	cg.Line();
	cg.Line();
	cg.Line("#undef %s", fullNamebodyMacros.c_str());
	cg.Line("#define %s(...) \\", fullNamebodyMacros.c_str());

	for (const std::string& macro : macrosNameList)
	{
		cg.Line("%s \\", macro.c_str());
	}

	if (isClass == true)
	{
		cg.Line("private:");
	}

	cg.Line();
	cg.Line();

	// define short name macros for programer. you can except namespace with this. ex) GENERATE_BODY_G
	const std::string shortNamebodyMacros = "GENERATE_BODY_" + macrobableClassShortTypeName;
	if (shortNamebodyMacros != fullNamebodyMacros)
	{
		cg.Line("//Type Short Name ( without namespace, only type name ) Version Macros.");
		cg.Line("#define %s(...) %s(__VA_ARGS__)", shortNamebodyMacros.c_str(), fullNamebodyMacros.c_str());
	}

	if (isLastType == true)
	{
		cg.Line();
		cg.Line();
		cg.Line("#undef GENERATE_BODY");
		cg.Line("#define GENERATE_BODY(...) %s(__VA_ARGS__)", fullNamebodyMacros.c_str());
	}
	
}


std::string UtilityHeaderGen::WriteCurrentTypeAliasMacros
(
	CodeGen & cg, 
	const cldb::Name& targetClassFullName, 
	const std::string & macrobableClassFullTypeName,
	const bool isClass
)
{
	assert(targetClassFullName.text.empty() == false);
	assert(targetClassFullName.hash != 0);
	assert(macrobableClassFullTypeName.empty() == false);

	cg.Line();
	cg.Line();
	const std::string CurrentTypeAliasMacrosName = "CURRENT_TYPE_ALIAS_" + macrobableClassFullTypeName;
	cg.Line("#undef %s", CurrentTypeAliasMacrosName.c_str());
	cg.Line("#define %s \\", CurrentTypeAliasMacrosName.c_str());
	if (isClass == true)
	{
		cg.Line("public : \\");
	}
	cg.Line("typedef %s Current;", targetClassFullName.text.c_str());

	return CurrentTypeAliasMacrosName;
}

std::string UtilityHeaderGen::WriteCurrentTypeStaticHashValueAndFullName
(
	CodeGen & cg, 
	const cldb::Name & targetClassFullName, 
	const std::string& targetClassShortName, 
	const std::string & macrobableClassFullTypeName,
	const bool isClass
)
{
	assert(targetClassFullName.text.empty() == false);
	assert(targetClassFullName.hash != 0);
	assert(macrobableClassFullTypeName.empty() == false);

	cg.Line();
	cg.Line();
	const std::string CurrentTypeStaticHashValueAndFullName = "TYPE_FULLNAME_HASH_VALUE_NAME_STRING_" + macrobableClassFullTypeName;
	cg.Line("#undef %s", CurrentTypeStaticHashValueAndFullName.c_str());
	cg.Line("#define %s \\", CurrentTypeStaticHashValueAndFullName.c_str());
	if (isClass == true)
	{
		cg.Line("public : \\");
	}
	cg.Line("inline static const unsigned long int TYPE_FULL_NAME_HASH_VALUE = %u; \\", targetClassFullName.hash);
	cg.Line("inline static const char* const TYPE_FULL_NAME = \"%s\"; \\", targetClassFullName.text.c_str());
	cg.Line("inline static const char* const TYPE_SHORT_NAME = \"%s\"; \\",targetClassShortName.c_str());
	cg.Line("virtual unsigned long int GetTypeHashVlue() const { return TYPE_FULL_NAME_HASH_VALUE; } \\");
	cg.Line("virtual const char* GetTypeFullName() const { return TYPE_FULL_NAME; } \\");
	cg.Line("virtual const char* GetTypeShortName() const { return TYPE_SHORT_NAME; }");

	return CurrentTypeStaticHashValueAndFullName;
}

std::string UtilityHeaderGen::WriteTypeCheckFunction
(
	CodeGen & cg, 
	const cldb::Name & targetClassFullName, 
	const std::string & macrobableClassFullTypeName,
	const bool isClass
)
{
	assert(targetClassFullName.text.empty() == false);
	assert(targetClassFullName.hash != 0);
	assert(macrobableClassFullTypeName.empty() == false);

	cg.Line();
	cg.Line();
	const std::string TypeCheckFunctionMacros = "TYPE_CHECK_FUNCTION_" + macrobableClassFullTypeName;
	cg.Line("#undef %s", TypeCheckFunctionMacros.c_str());
	cg.Line("#define %s \\", TypeCheckFunctionMacros.c_str());
	if (isClass == true)
	{
		cg.Line("private : \\");
	}
	cg.Line("attrNoReflect void __TYPE_CHECK() { static_assert(std::is_same_v<std::remove_reference<decltype(*this)>::type, Current> == true, \"ERROR : WRONG TYPE. Please Check GENERATED_~ MACROS\");} \\");

	return TypeCheckFunctionMacros;
}

std::string UtilityHeaderGen::WriteCloneObject
(
	CodeGen & cg, 
	const cldb::Name & targetClassFullName, 
	const std::string & macrobableClassFullTypeName, 
	const std::string& rootclass_typename,
	const bool isClass
)
{
	assert(targetClassFullName.text.empty() == false);
	assert(targetClassFullName.hash != 0);
	assert(macrobableClassFullTypeName.empty() == false);

	cg.Line();
	cg.Line();
	const std::string CloneObjectMacros = "CLONE_OBJECT_" + macrobableClassFullTypeName;
	cg.Line("#undef %s", CloneObjectMacros.c_str());
	cg.Line("#define %s \\", CloneObjectMacros.c_str());
	if (isClass == true)
	{
		cg.Line("public : \\");
	}
	cg.Line("virtual %s* CloneObject() const \\", rootclass_typename.c_str());
	cg.Line("{ \\");
	cg.Line("	%s* clonedObject = nullptr; \\", rootclass_typename.c_str());
	//cg.Line("	D_ASSERT(std::is_copy_constructible<%s>::value == true && std::is_base_of<dooms::DObejct, %s>::value == true); \\", targetClassFullName.text.c_str(), targetClassFullName.text.c_str());
	cg.Line("	/* std::vector<std::unique_ptr> can make false positive for std::is_copy_constructible<std::vector<std::unique_ptr>>::value. So Please explicitly delete copy constructor if you have this type variable */ \\");
	cg.Line("	if constexpr( (std::is_copy_constructible<%s>::value == true) && (std::is_base_of<%s, %s>::value == true) ) \\", targetClassFullName.text.c_str(), rootclass_typename.c_str(), targetClassFullName.text.c_str());
	cg.Line("	{ \\");
	cg.Line("		 clonedObject = dooms::CreateDObject<%s>(*this); \\", targetClassFullName.text.c_str());
	cg.Line("	} \\");
	cg.Line("	assert(clonedObject != nullptr);	\\");
	cg.Line("	return clonedObject;	\\");
	cg.Line("}");

	return CloneObjectMacros;
}

std::string UtilityHeaderGen::WriteAbstractCloneObject
(
	CodeGen & cg, 
	const cldb::Name & targetClassFullName, 
	const std::string & macrobableClassFullTypeName, 
	const std::string& rootclass_typename,
	const bool isClass
)
{
	assert(targetClassFullName.text.empty() == false);
	assert(targetClassFullName.hash != 0);
	assert(macrobableClassFullTypeName.empty() == false);

	cg.Line();
	cg.Line();
	const std::string CloneObjectMacros = "CLONE_OBJECT_" + macrobableClassFullTypeName;
	cg.Line("#undef %s", CloneObjectMacros.c_str());
	cg.Line("#define %s \\", CloneObjectMacros.c_str());
	if (isClass == true)
	{
		cg.Line("public : \\");
	}
	cg.Line("virtual %s* CloneObject() const { assert(false); return nullptr; }", rootclass_typename.c_str());

	return CloneObjectMacros;
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
			if ((UtilityHeaderGen::PRIMITIVE_KIND_TYPE_GENERATING_GENERATED_H_FILE & primitive->kind) != 0)
			{
				// find UtilityHeader's target type!
				if (std::find(targetTypesNameList.begin(), targetTypesNameList.end(), primitive) == targetTypesNameList.end())
				{
					// prevent duplicated primitive
					targetTypesNameList.emplace_back(primitive);
				}
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
			if ((UtilityHeaderGen::PRIMITIVE_KIND_TYPE_GENERATING_GENERATED_H_FILE & primitive->kind) != 0)
			{
				// find UtilityHeader's target type!
				if (std::find(targetTypesNameList.begin(), targetTypesNameList.end(), primitive) == targetTypesNameList.end())
				{
					targetTypesNameList.emplace_back(primitive);
				}
			}
		}

	}

	return targetTypesNameList;
}

#define READ_BUFFER_SIZE 200

bool UtilityHeaderGen::CheckReflectionFileChanged(const std::string & outputPath, CodeGen & newlyCreatedReflectionFile)
{
	FILE* outputFile = fopen(outputPath.c_str(), "r");

	bool isReflectionFileChanged = false;

	if (outputFile == 0)
	{
		isReflectionFileChanged = true;
	}
	else
	{

		static const int _EOF = -1;

		char temp[READ_BUFFER_SIZE];

		const std::string& reflectionFileString = newlyCreatedReflectionFile.GetText();

		size_t textIndex = 0;

		fseek(outputFile, 0L, SEEK_END);
		const size_t existingReflectionsStringLength = ftell(outputFile); // this is file size, not string length in file
		fseek(outputFile, 0L, SEEK_SET);

		if (reflectionFileString.size() > existingReflectionsStringLength)
		{// if string's size is bigger than file's size, string is always different with file
			isReflectionFileChanged = true;
		}
		else
		{// 
			while (size_t size = fread(temp, 1, READ_BUFFER_SIZE, outputFile)) // fgets return null if it's end of file and didn't read anything
			{
				if (strncmp(reflectionFileString.data() + textIndex, temp, size) != 0)
				{
					isReflectionFileChanged = true;
					break;
				}

				textIndex += size;

			}

			if (reflectionFileString.size() != textIndex)
			{
				isReflectionFileChanged = true;
			}
		}

		



		
		/* This codes doesn't work
		// Because ftell say
		fseek(outputFile, 0L, SEEK_END);
		const size_t existingReflectionsStringLength = ftell(outputFile);
		if (reflectionFileString.size() == existingReflectionsStringLength)
		{
			//string length equal
			fseek(outputFile, 0L, SEEK_SET);

			size_t textIndex = 0;

			while (fread(temp, sizeof(temp), 1, outputFile)) // fgets return null if it's end of file and didn't read anything
			{
				if (strncmp(reflectionFileString.data() + textIndex, temp, sizeof(temp)) != 0)
				{
					isReflectionFileChanged = true;
					break;
				}

				textIndex += sizeof(temp);

			} 
		}
		else
		{
			isReflectionFileChanged = true;
		}
		*/
	    fclose(outputFile);
	}

	return isReflectionFileChanged;
}



void UtilityHeaderGen::GenUtilityHeader
(
	const std::string& sourceFilePath, 
	const std::string& rootclass_typename, 
	cldb::Database & db,
	ASTConsumer& astConsumer
)
{
	if (sourceFilePath.empty() == true)
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
		const std::string outputPath = std::string{ sourceFilePath.begin(), sourceFilePath.begin() + extensionDotPos } +".reflection.h";

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
		cg.Line("// SourceFilePath : %s", sourceFilePath.c_str());
		cg.Line();
		cg.Line();


		const std::string outputPathMacros = ConvertNameToMacrobableName(outputPath);
		cg.Line("#ifdef %s", outputPathMacros.c_str());
		cg.Line();
		cg.Line("#error \"%s already included, missing '#pragma once' in %s\"", outputPath.c_str(), outputPath.c_str());
		cg.Line();
		cg.Line("#endif");
		cg.Line();
		cg.Line("#define %s", outputPathMacros.c_str());

		cg.Line();
		cg.Line();
		cg.Line("#include <type_traits>");
		cg.Line("#include <cassert>");
		cg.Line();
		cg.Line();
		cg.Line("//-------------------------------------------");
		cg.Line();
		cg.Line();

		std::vector<cldb::Primitive*> UtilityHeaderTargetTypeList = FindTargetTypesName(sourceFilePath, targetHeaderFilePath, astConsumer, db);

		// swap element location for type having name equal sourcefile name to be located at last pos
		if (UtilityHeaderTargetTypeList.size() >= 2)
		{
			for (auto iter = UtilityHeaderTargetTypeList.begin(); iter != UtilityHeaderTargetTypeList.end(); iter++)
			{
				if (ConvertFullTypeNameToShortTypeName((*iter)->name.text) == SourceFileNameWithoutExtension)
				{
					// find type having name equal sourcefile name!!
					std::swap(iter, UtilityHeaderTargetTypeList.end() - 1);
					break;
				}
			}
		}
		
		bool isDataGenerated = false;

		for (size_t i = 0; i < UtilityHeaderTargetTypeList.size(); i++)
		{
			assert(UtilityHeaderTargetTypeList[i] != nullptr);

			bool isSuccess = true;
			switch (UtilityHeaderTargetTypeList[i]->kind)
			{
			case cldb::Primitive::Kind::KIND_CLASS:
			case cldb::Primitive::Kind::KIND_TEMPLATE:
			case cldb::Primitive::Kind::KIND_TEMPLATE_TYPE:
				WriteClassMacros
				(
					cg,
					static_cast<cldb::Class*>(UtilityHeaderTargetTypeList[i]),
					rootclass_typename,
					(i == UtilityHeaderTargetTypeList.size() - 1),
					db,
					astConsumer
				);
				isDataGenerated = true;
				break;

			default:
				isSuccess = false;
				break;
			}

			if (isSuccess == true)
			{
				cg.Line();
				cg.Line();
				cg.Line("//-------------------------------------------");
				cg.Line();
				cg.Line();
			}

		}
		

		if (isDataGenerated == false)
		{
			LOG(UtilityHeaderGen, WARNING, "Any Type data is not generated. ( SourceFile Path : %s)\n", sourceFilePath.c_str());
		}


		// Check if file exist at outputPath
		// If it exist, compare cg string with the file.
		// If they equal, cg is written to outputPath. 
		// Why need this? : Writing reflection.h file make compiler recompile sourcefile
		if (/*isDataGenerated == true if class is removed, isDataGenerated is false*/ CheckReflectionFileChanged(outputPath, cg) == true)
		{
			cg.WriteToFile(outputPath.c_str());
			LOG(UtilityHeaderGen, INFO, "Success to create reflection.h file (%s)\n", outputPath.c_str());
		}
	}
	else
	{
		LOG(UtilityHeaderGen, WARNING, "fail to generate UtilityHeader output path");
	}


	

	
}
