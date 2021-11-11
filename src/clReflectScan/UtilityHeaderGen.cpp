#include "UtilityHeaderGen.h"

#include <clReflectCore/Logging.h>

UtilityHeaderGen::UtilityHeaderGen()
{
	LOG_TO_STDOUT(utilityHeaderGen, ALL);
	LOG_TO_STDOUT(warnings, INFO);

	
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

	std::string SourceFileNameWithoutExtension = SourceFileNameWithExtension;
	const size_t extensionDotPos = SourceFileNameWithExtension.find_last_of('.');
	if (lastBackSlashPos != std::string::npos)
	{
		SourceFileNameWithoutExtension = std::string(SourceFileNameWithoutExtension.begin(), SourceFileNameWithoutExtension.begin() + extensionDotPos);
	}

	const std::string OutputFilePath
}

// 개발 방향 :
// 클래스 명과 파일 명이 무조건 같게 강제하자.
// 같은 파일명의 클래스 없으면 그냥 통과
// 클래스 명과 Argument로 넘어오는 파일 명이 같은 경우를 찾으면 Reflection 데이터 생성

// 여기서 그냥 BaseChain도 완전히 생성해버리자.
// 어차피 Base 타고 올라가면 Base의 Hash값 모두 컴파일 타임에 만들어 낼 수 있다..
// ex) BaseChain[] = { 현재 클래스 Hash 값, 부모 클래스 Hash 값.... };

// GENERATE_BODY 매크로도 만들자.