// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidDesignPackage.h"
#include "Data/VoidImportContext.h"
#include "Data/VoidValidationReport.h"

/**
 * IVoidPackageReader
 *
 * Contract for turning a file on disk into an FVoidDesignPackage,
 * regardless of on-disk format. Defined in terms of Core-only types
 * (FString, FVoidDesignPackage, FVoidImportContext, FVoidValidationReport)
 * specifically so this interface never has to depend on a particular
 * parsing library -- FVoidJsonPackageReader (Import module) is the only
 * implementation in Phase 2, but a future YAML, XML, binary, or
 * remote-API reader would implement this same interface and register
 * itself with FVoidPackageReaderRegistry by file extension. Nothing
 * above this interface -- FVoidDesignPackageImporter, the Editor panel,
 * the commandlet, or any future generator -- would need to change.
 *
 * Implementations must never crash or throw on malformed input: report
 * problems through OutReport (using AddFatal for "couldn't read this at
 * all", AddError/AddWarning/AddInfo for anything less severe) and return
 * a best-effort (possibly partially empty) package.
 */
class VOIDWORLDBUILDERCORE_API IVoidPackageReader
{
public:
	virtual ~IVoidPackageReader() = default;

	/** Lowercase file extension this reader handles, without the leading dot, e.g. "json". Used as the registry lookup key. */
	virtual FName GetSupportedExtension() const = 0;

	/**
	 * Reads and maps FilePath into OutPackage.
	 * @return true if reading proceeded far enough to attempt validation.
	 *         This does NOT imply the data is valid -- check OutReport /
	 *         OutPackage's own validation for that. Returns false only
	 *         for conditions that make validation meaningless (file
	 *         unreadable, content unparseable in this format); such
	 *         conditions must also be recorded in OutReport via AddFatal.
	 */
	virtual bool TryRead(const FString& FilePath, FVoidImportContext& Context, FVoidDesignPackage& OutPackage, FVoidValidationReport& OutReport) const = 0;
};
