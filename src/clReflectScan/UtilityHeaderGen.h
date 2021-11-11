#pragma once

#include "clReflectCore/Database.h"

class UtilityHeaderGen
{
public:

	UtilityHeaderGen();

	void GenUtilityHeader(const std::string& sourceFilePath, const std::string& outputFilePath, cldb::Database& db);

};