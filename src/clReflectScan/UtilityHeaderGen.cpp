#include "UtilityHeaderGen.h"

#include <clReflectCore/Logging.h>
#include <clReflectCore/FileUtils.h>
#include <clcpp/clcpp.h>

#include <cassert>

using namespace cldb;

thread_local std::map<cldb::u32, ksj::BaseClassList> UtilityHeaderGen::BaseClassList{};

UtilityHeaderGen::UtilityHeaderGen()
{
	LOG_TO_STDOUT(utilityHeaderGen, ALL);
	LOG_TO_STDOUT(warnings, INFO);

	
}

std::vector<cldb::u32> UtilityHeaderGen::GetBaseClassesName(const cldb::u32 searchDerivedClassNameHash, cldb::Database& db)
{
	ksj::BaseClassList& baseClassList = BaseClassList[searchDerivedClassNameHash];
	if (baseClassList.isInitialized == false)
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
				baseClassList.NameHashList.push_back(inherit.base_type.hash);
			}
		}

		baseClassList.isInitialized = true;
	}
	
	return baseClassList.NameHashList;
}

std::string UtilityHeaderGen::ConvertNameToMacrobableName(const std::string & name)
{
	// "::" can't be contained in macros, so we use "__"
	std::string result = name;
	for (size_t i = 0; i < result.size(); i++)
	{
		if (result[i] == ':')
		{
			result[i] = '_';
		}
	}

	return result;
}

bool UtilityHeaderGen::GenerateBaseChainList
(
	const cldb::u32 targetClassNameHash,
	const cldb::u32 targetRootClassNameHash,
	cldb::Database& db,
	std::vector<cldb::u32>& baseChainList
)
{
	if (targetClassNameHash == targetRootClassNameHash)
	{
		//If root class
		baseChainList.push_back(targetRootClassNameHash);
		return true;
	}
	else
	{
		std::vector<cldb::u32> baseClassList = GetBaseClassesName(targetClassNameHash, db);
		if (baseClassList.empty() == false)
		{
			for (cldb::u32& baseClassName : baseClassList)
			{
				const bool result = GenerateBaseChainList(baseClassName, targetRootClassNameHash, db, baseChainList);
				if (result == true)
				{
					// Root class is found while travel recursive function!!
					baseChainList.push_back(baseClassName);
					return true;
				}
			}
		}

		// everything fails...
		return false;
	}	
}

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

void UtilityHeaderGen::WriteBaseChainList(CodeGen& cg, const std::vector<cldb::u32>& baseChainList)
{
	assert(baseChainList.empty() == false);
	if (baseChainList.empty() == false)
	{
		cg.Line("\\");

		std::string baseChainText;
		//baseChainText+=
		for (int index = 0; index < baseChainList.size() - 1; index++) // don't use size_t, it can make underflow
		{
			baseChainText += std::to_string(baseChainList[index]);
			baseChainText += ", ";
		}
		baseChainText += std::to_string(baseChainList[baseChainList.size() - 1]);

		cg.Line("private: inline static const unsigned long int BASE_CHAIN_LIST[] { %s }; \\", baseChainText.c_str()); // unsigned long guarantee 32bit
	}
}

void UtilityHeaderGen::GenUtilityHeader
(
	const std::string& sourceFilePath, 
	const std::string& rootclass_typename, 
	cldb::Database & db
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
		const std::string outputPath = std::string{ sourceFilePath.begin(), sourceFilePath.begin() + extensionDotPos } +".h";

		// Check types declared in outputPath
		// Check record_decl->getLocation()

		CodeGen cg;

		if (rootclass_typename.empty() == false)
		{
			// 1. check if rootclass_typename exist looping db::types
			// find all classes and enums and structs.. declared in sourceFilePath.h

			// 2. define macros like GENERATED_BODY_DOOMS_GRAPHICS_GRAPHICS_SERVER(). "::" can't be contained in macros, so we use "__"

			// 2. generate hash value and template function like clreflect_compiletime_gettype.cpp

			// 3. generate base chain data

			// 4. generate reflection variable, function, static functions, static variable


			// test codes
			std::vector<cldb::u32> baseChainList;
			GenerateBaseChainList
			(
				clcpp::internal::HashNameString("test_base_chain::G"),
				clcpp::internal::HashNameString(rootclass_typename.c_str()),
				db,
				baseChainList
			);

			WriteBaseChainList(cg, baseChainList);

			assert(baseChainList[0] == clcpp::internal::HashNameString("test_base_chain::G"));
			assert(baseChainList[0] != clcpp::internal::HashNameString("test_base_chain::TEST3"));
			assert(baseChainList[1] == clcpp::internal::HashNameString("test_base_chain::F"));
			assert(baseChainList[2] == clcpp::internal::HashNameString("test_base_chain::D"));
			assert(baseChainList[3] == clcpp::internal::HashNameString("test_base_chain::C"));
			assert(baseChainList[4] == clcpp::internal::HashNameString("test_base_chain::B"));
			assert(baseChainList[4] != clcpp::internal::HashNameString("test_base_chain::TEST2"));
			assert(baseChainList[4] != clcpp::internal::HashNameString("test_base_chain::TEST1"));
			assert(baseChainList.size() == 5);
			// create base chain data if rootclass_typename is not empty

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

// ���� ���� :
// Ŭ���� ��� ���� ���� ������ ���� ��������.
// ���� ���ϸ��� Ŭ���� ������ �׳� ���
// Ŭ���� ��� Argument�� �Ѿ���� ���� ���� ���� ��츦 ã���� Reflection ������ ����

// ���⼭ �׳� BaseChain�� ������ �����ع�����.
// ������ Base Ÿ�� �ö󰡸� Base�� Hash�� ��� ������ Ÿ�ӿ� ����� �� �� �ִ�..
// ex) BaseChain[] = { ���� Ŭ���� Hash ��, �θ� Ŭ���� Hash ��.... };

// GENERATE_BODY ��ũ�ε� ������.

// Reflection ����ü�� �������� Static �Լ� ���ǵ� ����.

// Ÿ�Ը��� ������ ���ӽ����̽��� �ٸ� ��쵵 ����Ǿ���Ѵ�.

//���� ���� 1 : clscan�ϴ� �ҽ����Ͽ��� Ȯ���ڸ� h�� �ٲپ �ش� header�� ����Ǿ� �ִ� ��� Ŭ����, ����ü, enum �� generated.h�� �ʿ��� Ÿ�Ե鿡 ���� �ϳ��� ���Ͽ� �� ����. ( Nice )
//
//	��� ���� : header�� �ְ� �ҽ������� ���� ���? -> �� ��� generated.h ���� �Ұ� -> �ҽ� ������ ���� ��� ������ �߻����Ѿ��Ѵ�.
//
//���� ���� 2 : clexport���� ���� -> �ٽ� �����ؾ��ϴ� ���� �������� ���ϴ� ������ �߻� -> �ᱹ ��� header���Ͽ� ���� generated_h�� �ٽ� �����ؾ��Ѵ� -> ����.