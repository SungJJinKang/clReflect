#include <fstream>
#include "C:/clReflect/inc/clcpp/clcpp.h"
#include <cassert>
#include <iostream>

#include "Source.h"

class StdFile : public clcpp::IFile
{
public:
	StdFile(const char* filename)
	{
		m_FP = fopen(filename, "rb");
		if (m_FP == 0)
		{
			return;
		}
	}

	~StdFile()
	{
		if (m_FP != 0)
		{
			fclose(m_FP);
		}
	}

	bool IsOpen() const
	{
		return m_FP != 0;
	}

	bool Read(void* dest, clcpp::size_type size)
	{
		return fread(dest, 1, size, m_FP) == size;
	}

private:
	FILE* m_FP;
};


class Malloc : public clcpp::IAllocator
{
	void* Alloc(clcpp::size_type size)
	{
		return malloc(size);
	}
	void Free(void* ptr)
	{
		free(ptr);
	}
};


int main()
{
	
	StdFile file("C:/Users/kmsjk/source/repos/Project7/clReflectCompialationData_Debug_x64.cppbin");
	if (!file.IsOpen())
		return 1;

	Malloc allocator;
	clcpp::Database db;
	if (!db.Load(&file, &allocator, 0))
		return 1;

	{

		auto aName = db.GetName("TestClass2");
		auto atype = db.GetType(aName.hash);
		auto class1 = atype->AsClass();

		auto fieldC = clcpp::FindPrimitive(class1->fields, db.GetName("c").hash);
		auto fieldD = clcpp::FindPrimitive(class1->fields, db.GetName("d").hash);

		assert(class1->fields.data[0]->offset == 12); // TestClass2::d
		assert(class1->fields.data[1]->offset == 8);  // TestClass2::c

		do {} while (false);
	}

	{

		auto aName = db.GetName("TestEnum");
		auto atype = db.GetType(aName.hash);
		auto enum1 = atype->AsEnum();
	
		assert(strcmp(enum1->GetValueName(5), "ENUM_ELEMENT1") == 0);
		assert(strcmp(enum1->GetValueName(6), "ENUM_ELEMENT2") == 0);
		assert(strcmp(enum1->GetValueName(7), "ENUM_ELEMENT3") == 0);
		assert(strcmp(enum1->GetValueName(8), "ENUM_ELEMENT4") == 0);

		do {} while (false);
	}


	
}