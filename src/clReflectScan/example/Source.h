#pragma once

#include "clcpp/clcpp.h"

#include "TestClass1.h"


class clcpp_attr(reflect_part) TestClass2 : public TestClass1
{
private:

	clcpp_attr(reflect) int c;
	clcpp_attr(reflect) int d;

public:

	clcpp_attr(reflect)
	virtual void Do1() override { c++; }

	clcpp_attr(reflect)
	void Do2() { c++; }

	clcpp_attr(reflect)
	void Do3() { c++; }
};


enum clcpp_attr(reflect) TestEnum
{
	ENUM_ELEMENT1 = 5,
	ENUM_ELEMENT2,
	ENUM_ELEMENT3,
	ENUM_ELEMENT4
};

struct clcpp_attr(reflect) TestStruct
{
	int e;
	float f;
};
clcpp_reflect(test_base_chain)
namespace test_base_chain
{
	class B
	{

	};

	class TEST1
	{

	};

	class TEST2
	{

	};

	class TEST3
	{

	};

	class C : public B, public TEST1
	{

	public:
		virtual void Do() {}
	};

	class D : public TEST2, public C
	{
	};
	
	class F : public TEST3, public D
	{
	};

	class G : public F
	{
	};
}