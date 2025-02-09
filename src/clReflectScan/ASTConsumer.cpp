
//
// ===============================================================================
// clReflect
// -------------------------------------------------------------------------------
// Copyright (c) 2011-2012 Don Williamson & clReflect Authors (see AUTHORS file)
// Released under MIT License (see LICENSE file)
// ===============================================================================
//

#include "ASTConsumer.h"
#include "AttributeParser.h"
#include "ReflectionSpecs.h"

#include <clcpp/clcpp.h>

#include <clReflectCore/FileUtils.h>
#include <clReflectCore/Logging.h>

// clang\ast\decltemplate.h(1484) : warning C4345: behavior change: an object of POD type constructed with an initializer of the
// form () will be default-initialized
#pragma warning(disable : 4345)

#include <clang/AST/ASTContext.h>
#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclGroup.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/RecordLayout.h>
#include <clang/AST/TemplateName.h>
#include <llvm/IR/Attributes.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/FileCollector.h>
#include <llvm/ADT/SmallVector.h>

#include <stdarg.h>

#include <unordered_set>
#include <memory>
#include <clReflectCore/pathUtility.h>

namespace
{
    // Untyped flags for the MakeField function, as opposed to a few bools
    enum
    {
        MF_CHECK_TYPE_IS_REFLECTED = 1,
    };

    // Helper for constructing printf-formatted strings immediately and passing them between functions
    struct va
    {
        va(const char* format, ...)
        {
            char buffer[512];
            va_list args;
            va_start(args, format);
#if defined(CLCPP_USING_MSVC)
            vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
#else
            vsnprintf(buffer, sizeof(buffer), format, args);
#endif // CLCPP_USING_MSVC
            va_end(args);
            text = buffer;
        }

        operator const std::string &() const
        {
            return text;
        }

        std::string text;
    };

    struct Status
    {
        static Status Warn(const std::string& message)
        {
            Status status;
            status.messages = message;
            return status;
        }

        static Status JoinWarn(const Status& older, const std::string& message)
        {
            if (older.IsSilentFail())
                return older;

            // Add the message before concatenating with the older ones
            Status status;
            status.messages = message + "; " + older.messages;
            return status;
        }

        static Status SilentFail()
        {
            Status status;
            status.messages = "SILENT FAIL";
            return status;
        }

        bool HasWarnings() const
        {
            return messages != "";
        }

        bool IsSilentFail() const
        {
            return messages == "SILENT FAIL";
        }

        void Print(clang::SourceLocation location, const clang::SourceManager& srcmgr, const std::string& message)
        {
            // Don't do anything on a silent fail
            if (IsSilentFail())
                return;

            // Get text for source location
            clang::PresumedLoc presumed_loc = srcmgr.getPresumedLoc(location);
            const char* filename = presumed_loc.getFilename();
            int line = presumed_loc.getLine();

            // Print immediate warning
            LOG(warnings, INFO, "%s(%d) : warning - %s; %s\n", filename, line, message.c_str(), messages.c_str());
        }

        std::string messages;
    };

    void Remove(std::string& str, const std::string& remove_str)
    {
        for (size_t i; (i = str.find(remove_str)) != std::string::npos;)
            str.replace(i, remove_str.length(), "");
    }

    struct ParameterInfo
    {
        ParameterInfo()
            : array_count(0)
        {
        }
        std::string type_name;
        cldb::Qualifier qualifer;
        cldb::u32 array_count;
    };

    Status GetParameterInfo(ASTConsumer& consumer, clang::QualType qual_type, ParameterInfo& info, int flags, std::shared_ptr<std::unordered_set<std::string>> alreadyParsedType);
    Status ParseTemplateSpecialisation(ASTConsumer& consumer, const clang::Type* type, std::string& type_name_str, std::shared_ptr<std::unordered_set<std::string>> alreadyParsedType);

    // A wrapper around a clang's CV-qualified types
    struct ClangASTType
    {
        ClangASTType(clang::QualType qt)
        {
            Set(qt);
        }

        // For each invocation of this function, we are passing in the returned
        // result of a function with clang::QuadType, this would actually generate
        // a local variable, if we use reference here, we will be referencing a
        // local temporary variable! Even if we copy the actual QualType in this
        // function, we do not want this to happen
        void Set(clang::QualType qt)
        {
            qual_type = qt;
            split_qual_type = qual_type.split();
            type = split_qual_type.Ty;
        }

        void UpdateTypedefOrElaborated()
        {
            clang::Type::TypeClass tc = type->getTypeClass();
            if (tc == clang::Type::Typedef || tc == clang::Type::Elaborated)
                Set(qual_type.getCanonicalType());
        }

        clang::QualType qual_type;
        clang::SplitQualType split_qual_type;
        const clang::Type* type;
    };

    Status ParseBaseClass(ASTConsumer& consumer, cldb::Name derived_type_name, const clang::CXXBaseSpecifier& base,
                          cldb::Name& base_name, const size_t inheritOrder, std::shared_ptr<std::unordered_set<std::string>> alreadyParsedType)
		// inheritLoc : order of declaring inheritance
    {
        // Get canonical base type
        ClangASTType base_type(base.getType());
        base_type.UpdateTypedefOrElaborated();

        // Parse the type name
        base_name = cldb::Name();
        std::string type_name_str = base_type.qual_type.getAsString(consumer.GetASTContext().getLangOpts());
        Remove(type_name_str, "struct ");
        Remove(type_name_str, "class ");

        // Can't support virtual base classes - offsets change at runtime
        if (base.isVirtual())
            return Status::Warn(va("Class '%s' is an unsupported virtual base class", type_name_str.c_str()));

        // Discover any new template types
        Status status = ParseTemplateSpecialisation(consumer, base_type.type, type_name_str, alreadyParsedType);
        if (status.HasWarnings())
            return status;
		
		//base_type.qual_ty
        cldb::Database& db = consumer.GetDB();
        base_name = db.GetName(type_name_str.c_str());
        const cldb::Name typeInheritanceName = db.AddTypeInheritance(derived_type_name, base_name);

		consumer.GetTypeInheritanceDeclararingOrder().insert_or_assign(typeInheritanceName.hash, inheritOrder);

        return Status();
    }

	bool IsForwardDeclaration(const clang::NamedDecl* const decl)
	{
		// Must be a struct/union/class
		const clang::CXXRecordDecl* record_decl = llvm::dyn_cast<const clang::CXXRecordDecl>(decl);
		if (record_decl == nullptr)
		{
			return false;
		}

		// See AddClassDecl comments for this behaviour
		if (record_decl->isThisDeclarationADefinition() != clang::VarDecl::DeclarationOnly)
		{
			return false;
		}
		if (!record_decl->isFreeStanding())
		{
			return false;
		}

		return true;
	}

    Status ParseTemplateSpecialisation
	(
		ASTConsumer& consumer, 
		const clang::ClassTemplateSpecializationDecl* cts_decl,
        std::string& type_name_str, 
		std::shared_ptr<std::unordered_set<std::string>> alreadyParsedType /* Added by ksj, This is for proventing circular inherit. ex) class Renderer : public ISingleton<Renderer> -> Parsing this class never be finished.    */
	)
    {
        // Get the template being specialised and see if it's marked for reflection
        // The template definition needs to be in scope for specialisations to occur. This implies
        // that the reflection spec must also be in scope.
        const clang::ClassTemplateDecl* template_decl = cts_decl->getSpecializedTemplate();
        type_name_str = template_decl->getQualifiedNameAsString();

        // Parent the instance to its declaring template
        cldb::Database& db = consumer.GetDB();
        cldb::Name parent_name = db.GetName(type_name_str.c_str());

        // Prepare for adding template arguments to the type name
        type_name_str += "<";

        // Get access to the template argument list
        ParameterInfo template_args[cldb::TemplateType::MAX_NB_ARGS];
        const clang::TemplateArgumentList& list = cts_decl->getTemplateArgs();
        if (list.size() >= cldb::TemplateType::MAX_NB_ARGS)
            return Status::Warn(
                va("Only %d template arguments are supported; template has %d", cldb::TemplateType::MAX_NB_ARGS, list.size()));

        for (unsigned int i = 0; i < list.size(); i++)
        {
            // Only support type arguments
            const clang::TemplateArgument& arg = list[i];
			if (arg.getKind() != clang::TemplateArgument::Type)
			{
				if (arg.getKind() == clang::TemplateArgument::Integral)
				{
					// Concatenate the arguments in the type name
					if (i)
						type_name_str += ",";
					type_name_str += arg.getAsIntegral().toString(10);
				}
				else
				{
					return Status::Warn(va("Unsupported non-type template parameter %d", i + 1));
				}
			}
			else
			{
				// Recursively parse the template argument to get some parameter info
				Status status = GetParameterInfo(consumer, arg.getAsType(), template_args[i], false, alreadyParsedType);
				if (status.HasWarnings())
					return Status::JoinWarn(status, va("Unsupported template parameter type %d", i + 1));

				// References currently not supported
				if (template_args[i].qualifer.op == cldb::Qualifier::REFERENCE)
					return Status::Warn(va("Unsupported reference type as template parameter %d", i + 1));

				// Can't reflect array template parameters
				if (template_args[i].array_count)
					return Status::Warn(va("Unsupported array template parameter %d", i + 1));

				// Concatenate the arguments in the type name
				if (i)
					type_name_str += ",";
				type_name_str += template_args[i].type_name;
				if (template_args[i].qualifer.op == cldb::Qualifier::POINTER)
					type_name_str += "*";
			}
            
        }

        type_name_str += ">";

        // Create the referenced template type on demand if it doesn't exist
        if (db.GetFirstPrimitive<cldb::TemplateType>(type_name_str.c_str()) == 0)
        {
            cldb::Name type_name = db.GetName(type_name_str.c_str());

			if (cts_decl->getNumBases() != 0)
			{
				if (alreadyParsedType == false)
				{
					alreadyParsedType = std::make_shared<std::unordered_set<std::string>>();
				}

				//ParameterInfo parameterInfo{};

				alreadyParsedType->emplace(type_name_str);
			}

            // Try to parse the base classes
            std::vector<cldb::Name> base_names;
            for (clang::CXXRecordDecl::base_class_const_iterator base_it = cts_decl->bases_begin();
                 base_it != cts_decl->bases_end(); base_it++)
            {
                cldb::Name base_name;
                Status status = ParseBaseClass(consumer, type_name, *base_it, base_name, base_it - cts_decl->bases_begin(), alreadyParsedType);
                if (status.HasWarnings())
                    return Status::JoinWarn(status, "Failure to create template type due to invalid base class");
                base_names.push_back(base_name);
            }

            // Construct the template type with a size, even though we're not populating its members
            const clang::ASTRecordLayout& layout = consumer.GetASTContext().getASTRecordLayout(cts_decl);
            cldb::u32 size = layout.getSize().getQuantity();
            cldb::TemplateType type(type_name, parent_name, size);

            // Populate the template argument list
            for (unsigned int i = 0; i < list.size(); i++)
            {
                type.parameter_types[i] = db.GetName(template_args[i].type_name.c_str());
                type.parameter_ptrs[i] = template_args[i].qualifer.op == cldb::Qualifier::POINTER;
            }

            // Log the creation of this new instance
            LOG(ast, INFO, "class %s", type_name_str.c_str());
            for (size_t i = 0; i < base_names.size(); i++)
                LOG_APPEND(ast, INFO, (i == 0) ? " : %s" : ", %s", base_names[i].text.c_str());
            LOG_NEWLINE(ast);

            auto iter = db.AddPrimitive(type);

			if (IsForwardDeclaration(template_decl) == false)
			{
				consumer.AddSourceLocation(cts_decl->getLocation(), &(iter->second));
			}
			
        }

        return Status();
    }

    Status ParseTemplateSpecialisation(ASTConsumer& consumer, const clang::Type* type, std::string& type_name_str, std::shared_ptr<std::unordered_set<std::string>> alreadyParsedType)
    {
        if (const clang::CXXRecordDecl* type_decl = type->getAsCXXRecordDecl())
        {
            // Don't attempt to parse declarations that contain this as it will be fully defined after
            // a merge operation. clang will try its best not to instantiate a template when it doesn't have too.
            // In such situations, parsing the specialisation is not possible.
            clang::Type::TypeClass tc = type->getTypeClass();
            if (tc == clang::Type::TemplateSpecialization && type_decl->getTemplateSpecializationKind() == clang::TSK_Undeclared)
                return Status::SilentFail();

            if (type_decl->getTemplateSpecializationKind() != clang::TSK_Undeclared)
            {
                const clang::ClassTemplateSpecializationDecl* cts_decl =
                    llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(type_decl);
                assert(cts_decl && "Couldn't cast to template specialisation decl");

                // Parse template specialisation parameters
                Status status = ParseTemplateSpecialisation(consumer, cts_decl, type_name_str, alreadyParsedType);
                if (status.HasWarnings())
                    return Status::JoinWarn(status,
                                            va("Couldn't parse template specialisation parameter '%s'", type_name_str.c_str()));
            }
        }

        return Status();
    }

    Status GetParameterInfo(ASTConsumer& consumer, clang::QualType qual_type, ParameterInfo& info, int flags, std::shared_ptr<std::unordered_set<std::string>> alreadyParsedType)
    {
        // Get type info for the parameter
        ClangASTType ctype(qual_type);

        // If this is an array of constant size, strip the size from the type and store it in the parameter info
        if (const clang::ConstantArrayType* array_type = llvm::dyn_cast<clang::ConstantArrayType>(ctype.type))
        {
            uint64_t size = *array_type->getSize().getRawData();
            if (size > UINT_MAX)
                return Status::Warn(va("Array size too big (%d)", size));
            info.array_count = (cldb::u32)size;
            ctype.Set(array_type->getElementType());
        }

        // If this is an elaborated type, get the canonical type (includes typedefs)
        ctype.UpdateTypedefOrElaborated();

        // Only handle one level of recursion for pointers and references

        // Get pointee type info if this is a pointer
        if (const clang::PointerType* ptr_type = llvm::dyn_cast<clang::PointerType>(ctype.type))
        {
            info.qualifer.op = cldb::Qualifier::POINTER;
            ctype.Set(ptr_type->getPointeeType());
        }

        // Get pointee type info if this is a reference
        else if (const clang::LValueReferenceType* ref_type = llvm::dyn_cast<clang::LValueReferenceType>(ctype.type))
        {
            info.qualifer.op = cldb::Qualifier::REFERENCE;
            ctype.Set(ref_type->getPointeeType());
        }

        // Do another pass on the elaborated/typedef types that may have been pulled from the pointer/reference types
        ctype.UpdateTypedefOrElaborated();

        // Record the qualifiers before stripping them and generating the type name
        clang::Qualifiers qualifiers = ctype.split_qual_type.Quals;
        ctype.qual_type.removeLocalFastQualifiers();
        info.type_name = ctype.qual_type.getAsString(consumer.GetASTContext().getLangOpts());
        info.qualifer.is_const = qualifiers.hasConst();

        // Is this a field that can be safely recorded?
        clang::Type::TypeClass tc = ctype.type->getTypeClass();
        switch (tc)
        {
        case clang::Type::TemplateSpecialization:
        case clang::Type::Builtin:
        case clang::Type::Enum:
        case clang::Type::Elaborated:
        case clang::Type::Record:
            break;
        default:
            return Status::Warn("Type class is unknown");
        }

        // Discover any new template types
		if (alreadyParsedType == false || alreadyParsedType->find(info.type_name) == alreadyParsedType->end())
		{
			Status status = ParseTemplateSpecialisation(consumer, ctype.type, info.type_name, alreadyParsedType);
			if (status.HasWarnings())
				return status;
		}

        // Pull the class descriptions from the type name
        Remove(info.type_name, "enum ");
        Remove(info.type_name, "struct ");
        Remove(info.type_name, "class ");

        // I'm not sure what's going on internally here with the Clang API but the type name interface has changed again since
        // updating so it's unclear what expected behaviour is. Given the following function:
        //
        //    void Do(Container<Type*>);
        //
        // type strings returned by clang are either Container<Type*> or Container<Type *>. It appears to be linked to whether
        // the TU has access to the function definition but I can't be entirely sure of the behaviour, especially as there seems
        // to be no means (in the current version) of changing that behaviour.
        //
        // Anyway, the workaround is to just replace the case we don't want.
        info.type_name = StringReplace(info.type_name, " *", "*");
        info.type_name = StringReplace(info.type_name, " &", "&");

        return Status();
    }



    Status MakeField(ASTConsumer& consumer, clang::QualType qual_type, const char* param_name, const std::string& parent_name,
                     int index, cldb::Field& field, int flags)
    {
        ParameterInfo info;
        Status status = GetParameterInfo(consumer, qual_type, info, flags, nullptr);
        if (status.HasWarnings())
            return Status::JoinWarn(status, va("Failure to make field '%s'", param_name));

        // Construct the field
        cldb::Database& db = consumer.GetDB();
        cldb::Name type_name = db.GetName(info.type_name.c_str());
        field = cldb::Field(db.GetName(param_name), db.GetName(parent_name.c_str()), type_name, info.qualifer, index);

        // Add a container info for this field if it's a constant array
        if (info.array_count)
        {
            std::string full_name = parent_name + "::" + param_name;
            cldb::ContainerInfo ci;
            ci.name = db.GetName(full_name.c_str());
            ci.flags = cldb::ContainerInfo::IS_C_ARRAY;
            ci.count = info.array_count;
            db.AddPrimitive(ci);
			// TODO : Support add sourceLocation of containerInfo primitive 
        }

        return Status();
    }

    template <typename TYPE>
    void AddAttribute(cldb::Database& db, TYPE* attribute)
    {
        // Only add the attribute if its unique
        const cldb::DBMap<TYPE>& store = db.GetDBMap<TYPE>();
        typename cldb::DBMap<TYPE>::const_iterator i = store.find(attribute->name.hash);
        if (i == store.end() || !i->second.Equals(*attribute))
        {
            LOG(ast, INFO, "attribute %s\n", attribute->name.text.c_str());
            db.AddPrimitive(*attribute);
			// TODO : Support add sourceLocation of attribute primitive 
        }
    }

   

    enum ParseAttributesResult
    {
        PAR_Normal,
        PAR_Reflect,
        PAR_ReflectPartial,
        PAR_NoReflect,
    };

    ParseAttributesResult ParseAttributes(ASTConsumer& consumer, clang::NamedDecl* decl, const std::string& parent,
                                          bool allow_reflect)
    {
        ParseAttributesResult result = PAR_Normal;

        cldb::Database& db = consumer.GetDB();
        const clang::SourceManager& srcmgr = consumer.GetASTContext().getSourceManager();

        // See what the reflection specs have to say (namespaces can't have attributes)
        const ReflectionSpecs& specs = consumer.GetReflectionSpecs();
        ReflectionSpecType spec_type = specs.Get(parent);
        switch (spec_type)
        {
        case (RST_Full):
            result = PAR_Reflect;
            break;
        case (RST_Partial):
            result = PAR_ReflectPartial;
            break;
        default:
            break;
        }

        // The underlying class declaration for templates contains the attached attributes
        if (clang::ClassTemplateDecl* template_decl = llvm::dyn_cast<clang::ClassTemplateDecl>(decl))
        {
            if (template_decl->getTemplatedDecl() != nullptr)
            {
                decl = template_decl->getTemplatedDecl();
            }
        }

        // Walk all annotation attributes attached to this decl
        std::vector<std::pair<clang::AnnotateAttr*, cldb::Attribute*>> attributes;
        for (clang::AnnotateAttr* annotate_attr : decl->specific_attrs<clang::AnnotateAttr>())
        {
            // Get the annotation text
            llvm::StringRef attribute_text = annotate_attr->getAnnotation();

            // Figure out what operations to apply to the attributes
            if (attribute_text.startswith("attr:"))
            {
                attribute_text = attribute_text.substr(sizeof("attr"));
            }

            // Decipher the source location of the attribute for error reporting
            clang::SourceLocation location = annotate_attr->getLocation();
            clang::PresumedLoc presumed_loc = srcmgr.getPresumedLoc(location);
            const char* filename = presumed_loc.getFilename();
            int line = presumed_loc.getLine();

            // Parse all attributes in the text
            for (cldb::Attribute* attribute : ::ParseAttributes(db, attribute_text.str().c_str(), filename, line))
            {
                attributes.push_back({annotate_attr, attribute});
            }
        }

        // Look for a reflection spec as the first attribute
        size_t attr_search_start = 0;
        thread_local static unsigned int reflect_hash = clcpp::internal::HashNameString("reflect");
        thread_local static unsigned int reflect_part_hash = clcpp::internal::HashNameString("reflect_part");
        thread_local static unsigned int noreflect_hash = clcpp::internal::HashNameString("noreflect");
        if (attributes.size())
        {
            unsigned int name_hash = attributes[0].second->name.hash;
            if (name_hash == reflect_hash)
                result = PAR_Reflect;
            else if (name_hash == reflect_part_hash)
                result = PAR_ReflectPartial;
            else if (name_hash == noreflect_hash)
                result = PAR_NoReflect;

            // Start adding attributes after any reflection specs
            // Their existence is implied by the presence of the primitives they describe
            if (result != PAR_Normal)
                attr_search_start = 1;
        }

        // Determine whether the attributes themselves need reflecting
        if (allow_reflect || result != PAR_NoReflect)
        {
            for (size_t i = attr_search_start; i < attributes.size(); i++)
            {
                cldb::Attribute* attribute = attributes[i].second;

                if (result != PAR_Normal)
                {
                    // Check that no attribute after the initial one contains a reflection spec
                    clang::SourceLocation location = attributes[i].first->getLocation();
                    unsigned int name_hash = attribute->name.hash;
                    if (name_hash == reflect_hash || name_hash == reflect_part_hash || name_hash == noreflect_hash)
                    {
                        Status().Print(location, srcmgr,
                                       va("'%s' attribute unexpected and ignored", attribute->name.text.c_str()));
                    }
                }

                // Add the attributes to the database, parented to the calling declaration
                attribute->parent = db.GetName(parent.c_str());
                switch (attribute->kind)
                {
                case (cldb::Primitive::KIND_FLAG_ATTRIBUTE):
                    AddAttribute(db, (cldb::FlagAttribute*)attribute);
                    break;
                case (cldb::Primitive::KIND_INT_ATTRIBUTE):
                    AddAttribute(db, (cldb::IntAttribute*)attribute);
                    break;
                case (cldb::Primitive::KIND_FLOAT_ATTRIBUTE):
                    AddAttribute(db, (cldb::FloatAttribute*)attribute);
                    break;
                case (cldb::Primitive::KIND_PRIMITIVE_ATTRIBUTE):
                    AddAttribute(db, (cldb::PrimitiveAttribute*)attribute);
                    break;
                case (cldb::Primitive::KIND_TEXT_ATTRIBUTE):
                    AddAttribute(db, (cldb::TextAttribute*)attribute);
                    break;
                default:
                    break;
                }
            }
        }

        // Delete the allocated attributes
        for (size_t i = 0; i < attributes.size(); i++)
        {
            delete attributes[i].second;
        }

        // TEMPORARY: If you add a pointer to a forward-declared type *without* marking that forward declaration as reflected,
        // the field itself won't get reflected. This is a huge burden that can cause bugs when one has a non-forward-declared
        // view of the type and another does not.
        //
        // I intend to fix this by changing how types are reflected: all types encountered are partially reflected by default.
        // Any other types need the reflect attribute to be fully reflected. This is a big change, however.
        //
        // For now, this specific case can be fixed by partially reflecting all forward-declarations.
        if (IsForwardDeclaration(decl))
        {
            result = PAR_ReflectPartial;
        }

        return result;
    }
}

ASTConsumer::ASTConsumer(cldb::Database& db, const ReflectionSpecs& rspecs, const std::string& ast_log)
    : m_DB(db)
    , m_ReflectionSpecs(rspecs)
    , m_AllowReflect(false)
{
	thread_local static bool isLogInitialized = false;
	if (isLogInitialized == false)
	{
		isLogInitialized = true;
		LOG_TO_STDOUT(warnings, INFO);

		if (ast_log != "")
			LOG_TO_FILE(ast, ALL, ast_log.c_str());
	}
}

void ASTConsumer::WalkTranlationUnit(clang::ASTContext* ast_context, clang::TranslationUnitDecl* tu_decl)
{
    m_ASTContext = ast_context;

    // Root namespace
    cldb::Name parent_name;

    // Iterate over every named declaration
    for (clang::DeclContext::decl_iterator i = tu_decl->decls_begin(); i != tu_decl->decls_end(); ++i)
    {
        clang::NamedDecl* named_decl = llvm::dyn_cast<clang::NamedDecl>(*i);
        if (named_decl == 0)
            continue;

        // Filter out unsupported decls at the global namespace level
        clang::Decl::Kind kind = named_decl->getKind();
        switch (kind)
        {
        case (clang::Decl::Namespace):
        case (clang::Decl::CXXRecord):
        case (clang::Decl::Function):
        case (clang::Decl::Enum):
        case (clang::Decl::ClassTemplate):
            AddDecl(named_decl, "", 0);
            break;
        default:
            break;
        }
    }
}

void ASTConsumer::AddDecl(clang::NamedDecl* decl, const std::string& parent_name, const clang::ASTRecordLayout* layout)
{
    // Skip decls with errors and those marked by the Reflection Spec pass to ignore
    if (decl->isInvalidDecl())
        return;

    // Gather all attributes associated with this primitive
    std::string name = decl->getQualifiedNameAsString();
    ParseAttributesResult result = ParseAttributes(*this, decl, name, m_AllowReflect);

    // Return immediately if 'noreflect' is specified, ignoring all children
    if (result == PAR_NoReflect)
        return;

    // If 'reflect' is specified, backup the allow reflect state and set it to true for this
    // declaration and all of its children.
    int old_allow_reflect = -1;
    if (result == PAR_Reflect)
    {
        old_allow_reflect = m_AllowReflect;
        m_AllowReflect = true;
    }

    // Reflect only if the allow reflect state has been inherited or the 'reflect_part'
    // attribute is specified
    if (m_AllowReflect || result == PAR_ReflectPartial)
    {
        clang::Decl::Kind kind = decl->getKind();
        switch (kind)
        {
        case (clang::Decl::Namespace):
            AddNamespaceDecl(decl, name, parent_name);
            break;
        case (clang::Decl::CXXRecord):
            AddClassDecl(decl, name, parent_name);
            break;
        case (clang::Decl::Enum):
            AddEnumDecl(decl, name, parent_name);
            break;
        case (clang::Decl::Function):
            AddFunctionDecl(decl, name, parent_name);
            break;
        case (clang::Decl::CXXMethod):
            AddMethodDecl(decl, name, parent_name);
            break;
        case (clang::Decl::Field):
            AddFieldDecl(decl, name, parent_name, layout);
            break;
        case (clang::Decl::ClassTemplate):
            AddClassTemplateDecl(decl, name, parent_name);
            break;
        default:
            break;
        }
    }

    // Restore any previously changed allow reflect state
    if (old_allow_reflect != -1)
        m_AllowReflect = old_allow_reflect != 0;

    // if m_ReflectPrimitives was changed, restore it
}

void ASTConsumer::AddNamespaceDecl(clang::NamedDecl* decl, const std::string& name, const std::string& parent_name)
{
    // Only add the namespace if it doesn't exist yet
    if (m_DB.GetFirstPrimitive<cldb::Namespace>(name.c_str()) == 0)
    {
		cldb::Namespace namespacePrimitive = cldb::Namespace(m_DB.GetName(name.c_str()), m_DB.GetName(parent_name.c_str()));
		auto iter = m_DB.AddPrimitive(namespacePrimitive);
		AddSourceLocation(decl->getLocation(), &(iter->second));
        LOG(ast, INFO, "namespace %s\n", name.c_str());
    }

    // Add everything within the namespace
    AddContainedDecls(decl, name, 0);
}

void ASTConsumer::AddClassDecl(clang::NamedDecl* decl, const std::string& name, const std::string& parent_name)
{
    // Cast to a record (NOTE: CXXRecord is a temporary clang type and will change in future revisions)
    clang::CXXRecordDecl* record_decl = llvm::dyn_cast<clang::CXXRecordDecl>(decl);
    assert(record_decl != 0 && "Failed to cast to record declaration");

    // Check for forward-declared types
    bool forward_decl = false;
    if (record_decl->isThisDeclarationADefinition() == clang::VarDecl::DeclarationOnly)
    {
        //
        // This classification of CXXRecord also comes through the AST like so:
        //
        //    namespace ns
        //    {
        //        class ClassName { };
        //    }
        //
        //    CXXRecord ns::ClassName
        //       CXXRecord ns::ClassName::ClassName
        //       CXXConstructor ns::ClassName::ClassName
        //
        // So before every constructor of a class, there's a superfluous CXX declaration of the same name.
        // Not sure why it's here, however the "free standing" flag is documented in the code to mark
        // these cases:
        //
        //    namespace ns
        //    {
        //       class ClassName;
        //    }
        //
        //    CXXRecord ns::ClassName
        //
        // And these are the exact cases that represent a reflected forward-declaration!
        //

        if (!record_decl->isFreeStanding())
            return;
        forward_decl = true;
    }

    // Ignore classes with virtual bases
    if (!forward_decl && record_decl->getNumVBases())
    {
        Status().Print(record_decl->getLocation(), m_ASTContext->getSourceManager(),
                       va("Class '%s' has an unsupported virtual base class", name.c_str()));
        return;
    }

	//volatile auto a = m_ASTContext->getSourceManager().getFilename(record_decl->getLocation());

    // Name gets added to the database if it's not already there
    cldb::Name type_name = m_DB.GetName(name.c_str());

    // Parse base classes
    std::vector<cldb::Name> base_names;
    if (!forward_decl && record_decl->getNumBases())
    {
        for (clang::CXXRecordDecl::base_class_const_iterator base_it = record_decl->bases_begin();
             base_it != record_decl->bases_end(); base_it++)
        {
            cldb::Name base_name;
            Status status = ParseBaseClass(*this, type_name, *base_it, base_name, base_it - record_decl->bases_begin(), nullptr);
            if (status.HasWarnings())
            {
                status.Print(record_decl->getLocation(), m_ASTContext->getSourceManager(),
                             va("Failed to reflect class '%s'", name.c_str()));
                return;
            }

            // If the base class is valid, then add the inheritance relationship
            m_DB.AddTypeInheritance(type_name, base_name);
            base_names.push_back(base_name);
        }
    }

    if (record_decl->isAnonymousStructOrUnion())
    {
        // Add declarations to the parent
        const clang::ASTRecordLayout& layout = m_ASTContext->getASTRecordLayout(record_decl);
        AddContainedDecls(decl, parent_name, &layout);
    }
    else
    {
        LOG(ast, INFO, "class %s", name.c_str());

        // Ensure there's at least an empty class definition in the database for this name
        cldb::Class* class_ptr = m_DB.GetFirstPrimitive<cldb::Class>(name.c_str());
        if (class_ptr == nullptr)
        {
            bool is_class = record_decl->getTagKind() == clang::TTK_Class;

			cldb::Class classPrimitive = cldb::Class(m_DB.GetName(name.c_str()), m_DB.GetName(parent_name.c_str()), is_class);

			auto iter = m_DB.AddPrimitive(classPrimitive);
            class_ptr = m_DB.GetFirstPrimitive<cldb::Class>(name.c_str());
        }

        if (!forward_decl)
        {
			assert(class_ptr != nullptr);

            // Fill in the missing class size
            const clang::ASTRecordLayout& layout = m_ASTContext->getASTRecordLayout(record_decl);
            class_ptr->size = layout.getSize().getQuantity();
			
            for (size_t i = 0; i < base_names.size(); i++)
                LOG_APPEND(ast, INFO, (i == 0) ? " : %s" : ", %s", base_names[i].text.c_str());

            LOG_NEWLINE(ast);

            // Populate class contents
			// iterate all function, properties inside of class.
			// parent name of them is class name.
            AddContainedDecls(decl, name, &layout);

			//call AddSourceLocation function only when it's not forward declaration
			AddSourceLocation(record_decl->getLocation(), class_ptr);
        }

        else
        {
            // Forward-declaration log
            LOG_NEWLINE(ast);
        }
    }
}

void ASTConsumer::AddEnumDecl(clang::NamedDecl* decl, const std::string& name, const std::string& parent_name)
{
    // Note that by unnamed enums are not explicitly discarded here. This is because they don't generally
    // get this far because you can't can't reference them in reflection specs.

    // Cast to an enum
    clang::EnumDecl* enum_decl = llvm::dyn_cast<clang::EnumDecl>(decl);
    assert(enum_decl != 0 && "Failed to cast to enum declaration");

    // Is this a C++11 scoped enum(enum class)?
    cldb::Enum::Scoped scoped = cldb::Enum::Scoped::None;
    const char* scope_tag = "";
    if (enum_decl->isScoped())
    {
        if (enum_decl->isScopedUsingClassTag())
        {
			//when enum class
            scoped = cldb::Enum::Scoped::Class;
            scope_tag = "class ";
        }
        else
        {
            scoped = cldb::Enum::Scoped::Struct;
            scope_tag = "struct ";
        }
    }

    // Add to the database
    LOG(ast, INFO, "enum %s%s\n", scope_tag, name.c_str());
	cldb::Enum enumPrimitive = cldb::Enum(m_DB.GetName(name.c_str()), m_DB.GetName(parent_name.c_str()), scoped);

	auto iter = m_DB.AddPrimitive(enumPrimitive);
	if (IsForwardDeclaration(decl) == false)
	{
		AddSourceLocation(enum_decl->getLocation(), &(iter->second));
	}

    LOG_PUSH_INDENT(ast);

    // Iterate over all constants
    for (clang::EnumDecl::enumerator_iterator i = enum_decl->enumerator_begin(); i != enum_decl->enumerator_end(); ++i)
    {
        clang::EnumConstantDecl* constant_decl = *i;

        // Strip out the raw 64-bit value - the compiler will automatically modify any values
        // greater than 64-bits without having to worry about that here
        llvm::APSInt value = constant_decl->getInitVal();
        int value_int = (int)value.getRawData()[0];

        // Clang doesn't construct the enum name as a C++ compiler would see it so do that first
        // NOTE: May want to revisit this later
        std::string constant_name = constant_decl->getNameAsString();

		constant_name = name + "::" + constant_name; // ksj : never change this, this make constant_name unique	
        // enum's elements contain enum's full name
        // ex)
        // namespace testNamespace
        // {
        //      enum TestEnum
        //      { TestEnumElement1, TestEnumElement2 };
        // }
        // testNamespace::TestEnum::TestEnumElement1's name is testNamespace::TestEnum::TestEnumElement1
        // testNamespace::TestEnum::TestEnumElement2's name is testNamespace::TestEnum::TestEnumElement2

		

        // Add to the database
		cldb::EnumConstant enumConstantPrimitive = cldb::EnumConstant(m_DB.GetName(constant_name.c_str()), m_DB.GetName(name.c_str()), value_int);
     
		auto iter = m_DB.AddPrimitive(enumConstantPrimitive);
		AddSourceLocation(constant_decl->getLocation(), &(iter->second));

        LOG(ast, INFO, "   %s = 0x%x\n", constant_name.c_str(), value_int);
    }

    LOG_POP_INDENT(ast);
}

void ASTConsumer::AddFunctionDecl(clang::NamedDecl* decl, const std::string& name, const std::string& parent_name)
{
    // Parse and add the function
    std::vector<cldb::Field> parameters;
    MakeFunction(decl, name, parent_name, parameters);
}

void ASTConsumer::AddMethodDecl(clang::NamedDecl* decl, const std::string& name, const std::string& parent_name)
{
    // Cast to a method
    clang::CXXMethodDecl* method_decl = llvm::dyn_cast<clang::CXXMethodDecl>(decl);
    assert(method_decl != 0 && "Failed to cast to C++ method declaration");

    // Ignore overloaded operators for now
    if (method_decl->isOverloadedOperator())
        return;

    std::vector<cldb::Field> parameters;
    if (method_decl->isInstance())
    {
        // Parse the 'this' type, treating it as the first parameter to the method
        cldb::Field this_param;
        Status status = MakeField(*this, method_decl->getThisType(), "this", name, 0, this_param, MF_CHECK_TYPE_IS_REFLECTED);
        if (status.HasWarnings())
        {
            status.Print(method_decl->getLocation(), m_ASTContext->getSourceManager(),
                         va("Failed to reflect method '%s' due to invalid 'this' type", name.c_str()));
            return;
        }
        parameters.push_back(this_param);
    }

    // Parse and add the method
    MakeFunction(decl, name, parent_name, parameters);
}

void ASTConsumer::AddFieldDecl(clang::NamedDecl* decl, const std::string& name, const std::string& parent_name,
                               const clang::ASTRecordLayout* layout)
{
    // Cast to a field
    clang::FieldDecl* field_decl = llvm::dyn_cast<clang::FieldDecl>(decl);
    assert(field_decl != 0 && "Failed to cast to field declaration");

    // These are implicitly generated by clang so skip them
    if (field_decl->isAnonymousStructOrUnion())
        return;

    // Parse and add the field
    cldb::Field field;
    cldb::u32 offset = layout->getFieldOffset(field_decl->getFieldIndex()) / 8;
    std::string field_name = field_decl->getName().str();
    Status status =
        MakeField(*this, field_decl->getType(), field_name.c_str(), parent_name, offset, field, MF_CHECK_TYPE_IS_REFLECTED);
    if (status.HasWarnings())
    {
        status.Print(field_decl->getLocation(), m_ASTContext->getSourceManager(),
                     va("Failed to reflect field in '%s'", parent_name.c_str()));
        return;
    }

    LOG(ast, INFO, "Field: %s%s%s %s\n", field.qualifier.is_const ? "const " : "", field.type.text.c_str(),
        field.qualifier.op == cldb::Qualifier::POINTER ? "*" : field.qualifier.op == cldb::Qualifier::REFERENCE ? "&" : "",
        field.name.text.c_str());
    m_DB.AddPrimitive(field);
}

void ASTConsumer::AddClassTemplateDecl(clang::NamedDecl* decl, const std::string& name, const std::string& parent_name)
{
    // Cast to class template decl
    clang::ClassTemplateDecl* template_decl = llvm::dyn_cast<clang::ClassTemplateDecl>(decl);
    assert(template_decl != 0 && "Failed to cast template declaration");

    // Only add the template if it doesn't exist yet
    if (m_DB.GetFirstPrimitive<cldb::Template>(name.c_str()) == 0)
    {
        // First check that the argument count is valid
        const clang::TemplateParameterList* parameters = template_decl->getTemplateParameters();
        if (parameters->size() > cldb::TemplateType::MAX_NB_ARGS)
        {
            Status().Print(template_decl->getLocation(), m_ASTContext->getSourceManager(),
                           va("Too many template arguments for '%s'", name.c_str()));
            return;
        }

		cldb::Template templatePrimitive = cldb::Template(m_DB.GetName(name.c_str()), m_DB.GetName(parent_name.c_str()));
 
		auto iter = m_DB.AddPrimitive(templatePrimitive);

		if (IsForwardDeclaration(decl) == false)
		{
			AddSourceLocation(template_decl->getLocation(), &(iter->second));
		}
		

        LOG(ast, INFO, "template %s\n", name.c_str());
    }
}

void ASTConsumer::AddContainedDecls(clang::NamedDecl* decl, const std::string& parent_name, const clang::ASTRecordLayout* layout)
{
    LOG_PUSH_INDENT(ast)

    // Iterate over every contained named declaration
    clang::DeclContext* decl_context = decl->castToDeclContext(decl);
    for (clang::DeclContext::decl_iterator i = decl_context->decls_begin(); i != decl_context->decls_end(); ++i)
    {
        clang::NamedDecl* named_decl = llvm::dyn_cast<clang::NamedDecl>(*i);
        if (named_decl != 0)
            AddDecl(named_decl, parent_name, layout);
    }

    LOG_POP_INDENT(ast)
}

void ASTConsumer::MakeFunction(clang::NamedDecl* decl, const std::string& function_name, const std::string& parent_name,
                               std::vector<cldb::Field>& parameters)
{
    // Cast to a function
    clang::FunctionDecl* function_decl = llvm::dyn_cast<clang::FunctionDecl>(decl);
    assert(function_decl != 0 && "Failed to cast to function declaration");

    // Only add the function once
    if (!function_decl->isFirstDecl())
        return;
	
	if (function_decl->isPure() == true)
	{
		LOG(ast, INFO, "Pure virtual function can't be reflected ( %s )\n", function_name.c_str());
		return;
	}


    // Parse the return type - named as a reserved keyword so it won't clash with user symbols
    cldb::Field return_parameter;
    Status status = MakeField(*this, function_decl->getCallResultType(), "return", function_name, -1, return_parameter, 0);
    if (status.HasWarnings())
    {
        status.Print(function_decl->getLocation(), m_ASTContext->getSourceManager(),
                     va("Failed to reflect function '%s' due to invalid return type", function_name.c_str()));
        return;
    }

    // Try to gather every parameter successfully before adding the function
    int index = parameters.size();
    for (clang::FunctionDecl::param_iterator i = function_decl->param_begin(); i != function_decl->param_end(); ++i)
    {
        clang::ParmVarDecl* param_decl = *i;

        // Auto name unnamed parameters
        llvm::StringRef param_name = param_decl->getName();
        std::string param_name_str = param_name.str();
        if (param_name_str.empty())
        {
            param_name_str = std::string("unnamed") + itoa(index);
        }

        // Collect a list of constructed parameters in case evaluating one of them fails
        cldb::Field parameter;
        status = MakeField(*this, param_decl->getType(), param_name_str.c_str(), function_name, index++, parameter, 0);
        if (status.HasWarnings())
        {
            status.Print(function_decl->getLocation(), m_ASTContext->getSourceManager(),
                         va("Failed to reflection function '%s'", function_name.c_str()));
            return;
        }
        parameters.push_back(parameter);
    }

    // Generate a hash unique to this function among other functions of the same name
    // This is so that its parameters can re-parent themselves correctly
    cldb::u32 unique_id = cldb::CalculateFunctionUniqueID(parameters);

    // Parent each parameter to the function
    return_parameter.parent_unique_id = unique_id;
    for (size_t i = 0; i < parameters.size(); i++)
        parameters[i].parent_unique_id = unique_id;

    // Add the function
    LOG(ast, INFO, "function %s\n", function_name.c_str());
	cldb::Function functionPrimitive = cldb::Function(m_DB.GetName(function_name.c_str()), m_DB.GetName(parent_name.c_str()), unique_id);

	auto iter = m_DB.AddPrimitive(functionPrimitive);
	if (IsForwardDeclaration(function_decl) == false)
	{
		AddSourceLocation(function_decl->getLocation(), &(iter->second));
	}

    LOG_PUSH_INDENT(ast);

    // Only add the return parameter if it's non-void
    if (return_parameter.type.text != "void")
    {
        LOG(ast, INFO, "Returns: %s%s%s\n", return_parameter.qualifier.is_const ? "const " : "",
            return_parameter.type.text.c_str(),
            return_parameter.qualifier.op == cldb::Qualifier::POINTER
                ? "*"
                : return_parameter.qualifier.op == cldb::Qualifier::REFERENCE ? "&" : "");

		auto iter2 = m_DB.AddPrimitive(return_parameter);
		AddSourceLocation(function_decl->getLocation(), &(iter2->second));
    }
    else
    {
        LOG(ast, INFO, "Returns: void (not added)\n");
    }

    // Add the parameters
    for (std::vector<cldb::Field>::iterator i = parameters.begin(); i != parameters.end(); ++i)
    {
        LOG(ast, INFO, "%s%s%s %s\n", i->qualifier.is_const ? "const " : "", i->type.text.c_str(),
            i->qualifier.op == cldb::Qualifier::POINTER ? "*" : i->qualifier.op == cldb::Qualifier::REFERENCE ? "&" : "",
            i->name.text.c_str());

		auto iter3 = m_DB.AddPrimitive(*i);
		AddSourceLocation(function_decl->getLocation(), &(iter3->second));
    }

    LOG_POP_INDENT(ast);
}



void ASTConsumer::AddSourceLocation(const clang::SourceLocation & sourceLocation, cldb::Primitive * primitive)
{
	llvm::StringRef sourceLocationPath = m_ASTContext->getSourceManager().getFilename(sourceLocation);
	//llvm::sys::path::native(sourceLocationPat)
	llvm::FileCollector::PathCanonicalizer pathCanonicalize{};
	const llvm::FileCollector::PathCanonicalizer::PathStorage canonicalizedPathPathStorage = pathCanonicalize.canonicalize(sourceLocationPath);
	
	//Small Vector isn't made for string encoding. it means it doesn't have null terminator because it know element count

	const std::string canonicalizedPath{ canonicalizedPathPathStorage.VirtualPath.data(), canonicalizedPathPathStorage.VirtualPath.data() + canonicalizedPathPathStorage.VirtualPath.size() };

	//const std::string sourceFilePath = sourcePath.data();
	assert(canonicalizedPath.empty() == false);
	if (canonicalizedPath.empty() == false)
	{
		m_SourceFilePathOfDeclMap[canonicalizedPath].push_back(primitive);
	}
}

