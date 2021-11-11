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
		if (baseChainList.empty() == false)
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

void UtilityHeaderGen::WriteBaseChainList(const cldb::Name className, CodeGen& cg, const std::vector<cldb::Name>& baseChainList)
{
	assert(baseChainList.empty() == false);
	if (baseChainList.empty() == false)
	{
		cg.Line();

		//std::string text; // you don't need cache it. CodeGen is internally caching text
		cg.Line("#undef BASE_CHAIN_DATA");
		cg.Line("#define BASE_CHAIN_DATA ");

		std::string baseChainText;
		//baseChainText+=
		for (int index = 0; index < baseChainList.size() - 1; index++) // don't use size_t, it can make underflow
		{
			baseChainText += baseChainList[index].hash;
			baseChainText += ", ";
		}
		baseChainText += baseChainList[baseChainList.size() - 1].hash;

		cg.Line("%s;", baseChainText.c_str()); // unsigned long guarantee 32bit
	}
}

void UtilityHeaderGen::GenUtilityHeader
(
	const std::string& sourceFilePath, 
	const std::string& outputFilePath, 
	const std::string& rootclass_typename, 
	cldb::Database & db
)
{
	size_t lastBackSlashPos = sourceFilePath.find_last_of('\\');
	if (lastBackSlashPos == std::string::npos)
	{
		lastBackSlashPos = sourceFilePath.find_last_of('/');
	}

	const std::string SourceFileNameWithExtension = (lastBackSlashPos != std::string::npos)
		? sourceFilePath.substr(lastBackSlashPos + 1) : SourceFileNameWithExtension;

	std::string SourceFileNameWithoutExtension = SourceFileNameWithExtension; // TargetClassName
	const size_t extensionDotPos = SourceFileNameWithExtension.find_last_of('.');
	if (lastBackSlashPos != std::string::npos)
	{
		SourceFileNameWithoutExtension = std::string(SourceFileNameWithoutExtension.begin(), SourceFileNameWithoutExtension.begin() + extensionDotPos);
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

	cg.WriteToFile(outputFilePath.c_str());
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