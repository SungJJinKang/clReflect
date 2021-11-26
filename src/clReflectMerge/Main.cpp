
//
// ===============================================================================
// clReflect
// -------------------------------------------------------------------------------
// Copyright (c) 2011-2012 Don Williamson & clReflect Authors (see AUTHORS file)
// Released under MIT License (see LICENSE file)
// ===============================================================================
//

#include "DatabaseMerge.h"

#include <clReflectCore/CodeGen.h>
#include <clReflectCore/Arguments.h>
#include <clReflectCore/Logging.h>
#include <clReflectCore/Database.h>
#include <clReflectCore/DatabaseTextSerialiser.h>
#include <clReflectCore/DatabaseBinarySerialiser.h>


#include <clcpp/clcpp.h>


int main(int argc, const char* argv[])
{
	LOG_TO_STDOUT(main, ALL);

	// Leave early if there aren't enough arguments
	Arguments args(argc, argv);
	if (args.Count() < 3)
	{
		LOG(main, ERROR, "Not enough arguments\n");
		return 1;
	}

	// Parse flags and mark where the file list starts
	size_t arg_start = 2;
	std::string cpp_codegen = args.GetProperty("-cpp_codegen");
	if (cpp_codegen != "")
		arg_start += 2;
    std::string h_codegen = args.GetProperty("-h_codegen");
    if (h_codegen != "")
        arg_start += 2;

    cldb::Database db;
	for (size_t i = arg_start; i < args.Count(); i++)
	{
		const wchar_t* filename = args[i].c_str();

		// Try to load the database
		cldb::Database loaded_db;
		if (!cldb::ReadBinaryDatabase(filename, loaded_db))
		{
			if (!cldb::ReadTextDatabase(filename, loaded_db))
			{
				LOG(main, ERROR, "Couldn't read '%s' as binary or text database - does it exist?", filename);
				return 1;
			}
		}

		// Merge into the main oent
		MergeDatabases(db, loaded_db, filename);
	}

	// Save the result
	const char* output_filename = args[1].c_str();
	cldb::WriteTextDatabase(output_filename, db);

	// Generate any required C++ code
    if (cpp_codegen != "" || h_codegen != "")
        GenMergedCppImpl(cpp_codegen.c_str(), h_codegen.c_str(), db);

    return 0;
}


struct Argvs
{
    int count = 0;
    char** argv;
};

Argvs argvSplit(const char* string, const char delimiter)
{
	int length = 0, count = 0, i = 0, j = 0;
	while (*(string++))
	{
		if (*string == delimiter)
			count++;
		length++;
	}
	string -= (length + 1); // string was incremented one more than length
	count++;

	char** array = (char**)malloc(sizeof(char*) * (count + 1));
	char** base = array;

	*array = (char*)malloc(sizeof(char));
	**array = '\0';
	array++;

	for (i = 0; i < count; i++)
	{
		j = 0;
		while (string[j] != delimiter && string[j] != '\0')
			j++;
		j++;
		*array = (char*)malloc(sizeof(char) * j);
		memcpy(*array, string, (j - 1));
		(*array)[j - 1] = '\0';
		string += j;
		array++;
	}

	Argvs argsv;
	argsv.argv = base;
	argsv.count = count + 1;

	return argsv;
}

extern "C" __declspec(dllexport) int c_clmerge(const char* argv)
{
    const Argvs argvs = argvSplit(argv, ' ');

    const int result = main(argvs.count, (const char**)argvs.argv);

	for (int i = 0; i < argvs.count ; i++)
	{
		free(argvs.argv[i]);
	}
	free(argvs.argv);

    return result;
}
