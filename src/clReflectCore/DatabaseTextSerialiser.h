
//
// ===============================================================================
// clReflect, DatabaseTextSerialiser.h - Text serialisation of the offline
// Reflection database. Used mainly during development for debugging.
// -------------------------------------------------------------------------------
// Copyright (c) 2011-2012 Don Williamson & clReflect Authors (see AUTHORS file)
// Released under MIT License (see LICENSE file)
// ===============================================================================
//


#pragma once


namespace cldb
{
	class Database;

	void WriteTextDatabase(const wchar_t* filename, const Database& db);
	bool ReadTextDatabase(const wchar_t* filename, Database& db);
	bool IsTextDatabase(const wchar_t* filename);
}