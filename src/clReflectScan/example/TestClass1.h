#pragma once

#include "clcpp/clcpp.h"

clcpp_reflect_part(TestClass1)
class TestClass1
{
	clcpp_attr(reflect) int a;
	clcpp_attr(reflect) int b;
	clcpp_attr(reflect) int c;
	int d;
	int e;

	clcpp_attr(reflect)
	virtual void Do1() { d++; }
};

clcpp_reflect_part(dooms)
namespace dooms
{
	clcpp_reflect_part(dooms::test)
	namespace test
	{
		enum clcpp_attr(reflect) EnumTest
		{
			EnumTestConstant1,
				EnumTestConstant2,
				EnumTestConstant3,
				EnumTestConstant4
		};
	}
}