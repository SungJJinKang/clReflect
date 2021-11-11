#include "UtilityHeaderGen.h"

#include <clReflectCore/Logging.h>
#include <clReflectCore/FileUtils.h>
#include <clcpp/clcpp.h>

#include <cassert>

using namespace cldb;

UtilityHeaderGen::UtilityHeaderGen()
{
	LOG_TO_STDOUT(utilityHeaderGen, ALL);
	LOG_TO_STDOUT(warnings, INFO);

	
}

std::vector<cldb::Name> UtilityHeaderGen::GetBaseClassesName(const cldb::Name& searchDerivedClassName, cldb::Database& db)
{
	std::vector<cldb::Name> result;

	ksj::BaseClassList& baseClassList = BaseClassList[searchDerivedClassName.hash];
	if (baseClassList.isInitialized == false)
	{
		for (cldb::DBMap<cldb::TypeInheritance>::const_iterator i = db.m_TypeInheritances.begin(); i != db.m_TypeInheritances.end(); ++i)
		{
			// This code can be slow.
			// But I want to edit existing codes as little as possible.

			// cldb::DBMap<cldb::TypeInheritance>'s TypeInheritance guarantee uniqueness. it never have same typeInheritance
			const cldb::TypeInheritance& inherit = i->second;

			if (searchDerivedClassName.hash == inherit.derived_type.hash) // hash guarantee uniqueness
			{
				baseClassList.NameHashList.push_back(inherit.base_type.hash);
			}
		}

		baseClassList.isInitialized = true;
	}
	
}

bool UtilityHeaderGen::GenerateBaseChainList
(
	const cldb::Name& targetClassName, 
	const cldb::Name& targetRootClassName, 
	cldb::Database& db,
	std::vector<cldb::Name>& baseChainList
)
{
	if (targetClassName == targetRootClassName)
	{
		//If root class
		baseChainList.push_back(targetRootClassName);
		return true;
	}
	else
	{
		std::vector<cldb::Name> baseClassList = GetBaseClassesName(targetClassName, db);
		if (baseChainList.empty() == false)
		{
			for (cldb::Name& baseClassName : baseClassList)
			{
				const bool result = GenerateBaseChainList(baseClassName, targetRootClassName, db, baseChainList);
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

void UtilityHeaderGen::WriteBaseChainList(CodeGen& cg, const std::vector<cldb::Name>& baseChainList)
{
	assert(baseChainList.empty() == false);
	if (baseChainList.empty() == false)
	{
		cg.Line();

		//std::string text; // you don't need cache it. CodeGen is internally caching text

		cg.Line("unsigned long BaseChainList = \\"); // unsigned long guarantee 32bit
		cg.Line("{"); // unsigned long guarantee 32bit
		for (int index = 0; index < baseChainList.size() - 1; index++) // don't use size_t, it can make underflow
		{
			cg.Line("%d ,", baseChainList[index].hash);
		}
		cg.Line("%d", baseChainList[baseChainList.size() - 1].hash);
		cg.Line("};"); // unsigned long guarantee 32bit
	}
}

void UtilityHeaderGen::GenUtilityHeader(const std::string& sourceFilePath, const std::string& outputFilePath, cldb::Database & db)
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



	CodeGen cg;

	// Generate arrays
	cg.Line("// Utility Header File ( Don't Edit this )");
	cg.Line("static const int clcppNbTypes = %d;", primitives.size());
	cg.Line("static const clcpp::Type* clcppTypePtrs[clcppNbTypes] = { 0 };");
	cg.Line();

	// Generate initialisation function
	cg.Line("#ifndef GENERATE_BODY");
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
	GenGetTypes(cg, primitives, PT_Type | PT_Class | PT_Struct | PT_EnumClass | PT_EnumStruct);
	cg.Line("#if defined(CLCPP_USING_MSVC)");
	GenGetTypes(cg, primitives, PT_Enum);
	cg.Line("#endif");
	cg.ExitScope();

	cg.WriteToFile(outputFilePath.c_str());
}

// 개발 방향 :
// 클래스 명과 파일 명이 무조건 같게 강제하자.
// 같은 파일명의 클래스 없으면 그냥 통과
// 클래스 명과 Argument로 넘어오는 파일 명이 같은 경우를 찾으면 Reflection 데이터 생성

// 여기서 그냥 BaseChain도 완전히 생성해버리자.
// 어차피 Base 타고 올라가면 Base의 Hash값 모두 컴파일 타임에 만들어 낼 수 있다..
// ex) BaseChain[] = { 현재 클래스 Hash 값, 부모 클래스 Hash 값.... };

// GENERATE_BODY 매크로도 만들자.