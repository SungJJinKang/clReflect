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

// ���� ���� :
// Ŭ���� ��� ���� ���� ������ ���� ��������.
// ���� ���ϸ��� Ŭ���� ������ �׳� ���
// Ŭ���� ��� Argument�� �Ѿ���� ���� ���� ���� ��츦 ã���� Reflection ������ ����

// ���⼭ �׳� BaseChain�� ������ �����ع�����.
// ������ Base Ÿ�� �ö󰡸� Base�� Hash�� ��� ������ Ÿ�ӿ� ����� �� �� �ִ�..
// ex) BaseChain[] = { ���� Ŭ���� Hash ��, �θ� Ŭ���� Hash ��.... };

// GENERATE_BODY ��ũ�ε� ������.