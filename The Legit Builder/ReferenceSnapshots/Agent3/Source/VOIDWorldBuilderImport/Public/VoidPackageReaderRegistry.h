// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IVoidPackageReader.h"

/**
 * FVoidPackageReaderRegistry
 *
 * Central lookup for IVoidPackageReader implementations, keyed by
 * lowercase file extension. FVoidDesignPackageImporter::LoadFromFile asks
 * this registry for a reader instead of assuming JSON directly -- this
 * is precisely the mechanism that lets a future YAML/XML/Binary/Remote
 * reader be added without touching the importer, the Editor panel, the
 * commandlet, or any generator code: implement IVoidPackageReader,
 * register it here for its extension, done.
 *
 * Mirrors FVoidGeneratorRegistry's design deliberately, for the same
 * reasons: manual registration (not static-init auto-registration) while
 * there's a small, known set of registrants, called from each owning
 * module's StartupModule/ShutdownModule.
 */
class VOIDWORLDBUILDERIMPORT_API FVoidPackageReaderRegistry
{
public:
	static FVoidPackageReaderRegistry& Get();

	void RegisterReader(TSharedRef<IVoidPackageReader> Reader);
	void UnregisterReader(FName Extension);

	/** Resolves FilePath's extension (case-insensitively, without the leading dot) to a registered reader. Returns nullptr if none is registered for that extension. */
	TSharedPtr<IVoidPackageReader> FindReaderForFile(const FString& FilePath) const;

private:
	TMap<FName, TSharedRef<IVoidPackageReader>> Readers;
};
