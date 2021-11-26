
//
// ===============================================================================
// clReflect, DatabaseBinarySerialiser.h - Binary serialisation of the offline
// Reflection Database. Much faster and more compact than the text representation.
// -------------------------------------------------------------------------------
// Copyright (c) 2011-2012 Don Williamson & clReflect Authors (see AUTHORS file)
// Released under MIT License (see LICENSE file)
// ===============================================================================
//


#pragma once


namespace cldb
{
	class Database;

	void WriteBinaryDatabase(const wchar_t* filename, const Database& db);
	bool ReadBinaryDatabase(const wchar_t* filename, Database& db);
	bool IsBinaryDatabase(const wchar_t* filename);
}