// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidPackageReaderRegistry.h"
#include "VoidWorldBuilderImportLog.h"
#include "Misc/Paths.h"

FVoidPackageReaderRegistry& FVoidPackageReaderRegistry::Get()
{
	static FVoidPackageReaderRegistry Instance;
	return Instance;
}

void FVoidPackageReaderRegistry::RegisterReader(TSharedRef<IVoidPackageReader> Reader)
{
	const FName Extension = Reader->GetSupportedExtension();

	if (Readers.Contains(Extension))
	{
		UE_LOG(LogVoidImport, Warning, TEXT("A reader for extension '.%s' is already registered; overwriting previous registration."), *Extension.ToString());
	}

	Readers.Add(Extension, Reader);
	UE_LOG(LogVoidImport, Log, TEXT("Registered package reader for extension '.%s'."), *Extension.ToString());
}

void FVoidPackageReaderRegistry::UnregisterReader(FName Extension)
{
	Readers.Remove(Extension);
}

TSharedPtr<IVoidPackageReader> FVoidPackageReaderRegistry::FindReaderForFile(const FString& FilePath) const
{
	const FName Extension(*FPaths::GetExtension(FilePath).ToLower());

	if (const TSharedRef<IVoidPackageReader>* Found = Readers.Find(Extension))
	{
		return *Found;
	}

	return nullptr;
}
