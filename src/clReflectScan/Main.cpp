
//
// ===============================================================================
// clReflect
// -------------------------------------------------------------------------------
// Copyright (c) 2011-2012 Don Williamson & clReflect Authors (see AUTHORS file)
// Released under MIT License (see LICENSE file)
// ===============================================================================
//

#include "ASTConsumer.h"
#include "ReflectionSpecs.h"
#include "UtilityHeaderGen.h"

#include "clReflectCore/Database.h"
#include "clReflectCore/DatabaseBinarySerialiser.h"
#include "clReflectCore/DatabaseTextSerialiser.h"
#include "clReflectCore/Logging.h"

#include "clang/AST/ASTContext.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include <llvm/Support/TargetSelect.h>

#include <stdio.h>
#include <time.h>
#include <mutex>

namespace
{
    bool EndsWith(const std::string& str, const std::string& end)
    {
        return str.rfind(end) == str.length() - end.length();
    }

    void WriteDatabase(const cldb::Database& db, const std::string& filename)
    {
        if (EndsWith(filename, ".csv"))
        {
            cldb::WriteTextDatabase(filename.c_str(), db);
        }
        else
        {
            cldb::WriteBinaryDatabase(filename.c_str(), db);
        }
    }

    using ParseTUHandler = std::function<void(clang::ASTContext&, clang::TranslationUnitDecl*)>;

    // Top-level AST consumer that passes an entire TU to the provided callback
    class ReflectConsumer : public clang::ASTConsumer
    {
    public:
        ReflectConsumer(ParseTUHandler handler)
            : m_handler(handler)
        {
        }

        void HandleTranslationUnit(clang::ASTContext& context)
        {
            m_handler(context, context.getTranslationUnitDecl());
        }

    private:
        ParseTUHandler m_handler;
    };

    // Frontend action to create the ReflectConsumer
    class ReflectFrontendAction : public clang::ASTFrontendAction
    {
    public:
        ReflectFrontendAction(ParseTUHandler handler)
            : m_handler(handler)
        {
        }

        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& compiler, llvm::StringRef file)
        {
            return std::unique_ptr<clang::ASTConsumer>(new ReflectConsumer(m_handler));
        }

    private:
        ParseTUHandler m_handler;
    };

    // Custom FrontendActionFactory creator that allows me to pass in arbitrary arguments
    std::unique_ptr<clang::tooling::FrontendActionFactory> NewReflectFrontendActionFactory(ParseTUHandler handler)
    {
        struct ReflectFrontendActionFactory : public clang::tooling::FrontendActionFactory
        {
            std::unique_ptr<clang::FrontendAction> create() override
            {
                return std::make_unique<ReflectFrontendAction>(handler);
            }
            ParseTUHandler handler;
        };

        auto* factory = new ReflectFrontendActionFactory();
        factory->handler = handler;
        return std::unique_ptr<clang::tooling::FrontendActionFactory>(factory);
    }
}

namespace ksj
{
	static std::mutex CommonOptionsParserMutex{};
}

int main(int argc, const char* argv[])
{
    float start = clock();

	thread_local static bool isLogInitialized = false;
	if(isLogInitialized == false)
	{
		isLogInitialized = true;
		LOG_TO_STDOUT(main, ALL);
		LOG_TO_STDOUT(main, WARNING);
	}

	std::unique_lock<std::mutex> lk_b(ksj::CommonOptionsParserMutex);

    // Command-line options
    //you can't put thread_local to this
    static llvm::cl::OptionCategory ToolCategoryOption("clreflect options");
	static llvm::cl::cat ToolCategory(ToolCategoryOption);
	static llvm::cl::opt<std::string> ReflectionSpecLog("spec_log", llvm::cl::desc("Specify reflection spec log filename"),
                                                        ToolCategory, llvm::cl::value_desc("filename"));
	static llvm::cl::opt<std::string> ASTLog("ast_log", llvm::cl::desc("Specify AST log filename"), ToolCategory,
                                             llvm::cl::value_desc("filename"));
	static llvm::cl::opt<std::string> Output("output", llvm::cl::desc("Specify database output file, depending on extension"),
                                             ToolCategory, llvm::cl::value_desc("filename"));
	static llvm::cl::opt<std::string> RootClassTypeName("rootClass_typeName", llvm::cl::desc("Specify Root class typename including namespace"),
		ToolCategory, llvm::cl::value_desc("type"));
	static llvm::cl::opt<bool> Timing("timing", llvm::cl::desc("Print some rough timing info"), ToolCategory);
	
	//auto option = llvm::cl::getRegisteredOptions(*llvm::cl::AllSubCommands);
	//option["<source0> [... <sourceN>]"]->reset();
    // Parse command-line options
    auto options_parser = clang::tooling::CommonOptionsParser::create(argc, argv, ToolCategoryOption, llvm::cl::OneOrMore);
	
    if (!options_parser)
    {
        return 1;
    }

	
    // Initialize inline ASM parsing
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmParser();

    // Create the clang tool that parses the input files


	// this looks hacky.
	// but clang compiler doesn't support multithread ( it use static variable at many codes ). 
	// so this is to do that
	const std::vector<std::string> sourcePathList = options_parser->getSourcePathList();
    clang::tooling::ClangTool tool(options_parser->getCompilations(), sourcePathList);
	
	const std::string outputFilePath = Output;
	const std::string ast_logFilePath = ASTLog;
	const std::string spec_logFilePath = ReflectionSpecLog;
	const std::string rootclass_typename = RootClassTypeName;
	const bool _Timing = Timing;
	ReflectionSpecLog.reset();
	ASTLog.reset();
	Output.reset();
	RootClassTypeName.reset();
	Timing.reset();

	for (auto iter = llvm::cl::AllSubCommands->OptionsMap.begin(); iter != llvm::cl::AllSubCommands->OptionsMap.end(); iter++)
	{
		iter->getValue()->setDefault();
	}
	for (auto iter = llvm::cl::AllSubCommands->PositionalOpts.begin(); iter != llvm::cl::AllSubCommands->PositionalOpts.end(); iter++)
	{
		(*iter)->setDefault();
	}
	for (auto iter = llvm::cl::AllSubCommands->SinkOpts.begin(); iter != llvm::cl::AllSubCommands->SinkOpts.end(); iter++)
	{
		(*iter)->setDefault();
	}
	

	//llvm::cl::ResetAllOptionOccurrences();

	lk_b.unlock();

    float prologue = clock();

    ReflectionSpecs reflection_specs(spec_logFilePath);
    cldb::Database db;
    ASTConsumer ast_consumer(db, reflection_specs, ast_logFilePath);

    float parsing, specs;

    if (tool.run(NewReflectFrontendActionFactory([&](clang::ASTContext& context, clang::TranslationUnitDecl* tu_decl) {
                    // Measures parsing and creation of the AST
                    parsing = clock();

                    // Gather reflection specs for the translation unit
                    reflection_specs.Gather(tu_decl);

                    specs = clock();

                    // On the second pass, build the reflection database
                    db.AddBaseTypePrimitives();
                    ast_consumer.WalkTranlationUnit(&context, tu_decl);
                }).get()) != 0)
    {
        return 2;
    }

    float build = clock();

    // Add all the container specs
    const ReflectionSpecContainer::MapType& container_specs = reflection_specs.GetContainerSpecs();
    for (ReflectionSpecContainer::MapType::const_iterator i = container_specs.begin(); i != container_specs.end(); ++i)
    {
        const ReflectionSpecContainer& c = i->second;
        db.AddContainerInfo(i->first, c.read_iterator_type, c.write_iterator_type, c.has_key);
    }

    // Write to a text/binary database depending upon extension
    if (outputFilePath != "")
    {
        WriteDatabase(db, outputFilePath);
    }

	if (rootclass_typename.empty() == true)
	{
		LOG(main, WARNING, "rootClass_typeName is not found\n");
	}
	
	
	assert(sourcePathList.size() == 1);
	if (sourcePathList.empty() == false)
	{
		UtilityHeaderGen utilityHeadergen{};
		try
		{
			utilityHeadergen.GenUtilityHeader(sourcePathList[0], rootclass_typename, db, ast_consumer);
		}
		catch (std::exception e)
		{
			LOG(main, WARNING, e.what());
			return 3;
		}
		
	}
	

    float end = clock();

    // Print some rough profiling info
    if (_Timing)
    {
        printf("Prologue:   %.3f\n", (prologue - start) / CLOCKS_PER_SEC);
        printf("Parsing:    %.3f\n", (parsing - prologue) / CLOCKS_PER_SEC);
        printf("Specs:      %.3f\n", (specs - parsing) / CLOCKS_PER_SEC);
        printf("Building:   %.3f\n", (build - specs) / CLOCKS_PER_SEC);
        printf("Database:   %.3f\n", (end - build) / CLOCKS_PER_SEC);
        printf("Total time: %.3f\n", (end - start) / CLOCKS_PER_SEC);
    }

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

extern "C" __declspec(dllexport) int c_clscan(const char** argv)
{
    const Argvs argvs = argvSplit((const char*)argv, ' ');

    const int result = main(argvs.count, (const char**)argvs.argv);

	for (int i = 0; i < argvs.count ; i++)
	{
		free(argvs.argv[i]);
	}
	free(argvs.argv);

    return result;
}