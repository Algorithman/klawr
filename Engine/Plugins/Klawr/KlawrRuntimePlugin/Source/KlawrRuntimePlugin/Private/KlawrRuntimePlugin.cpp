//-------------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2014 Vadim Macagon
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//-------------------------------------------------------------------------------
#include "KlawrRuntimePluginPrivatePCH.h"
#include "KlawrClrHost.h"
#include "KlawrNativeUtils.h"
#include "KlawrObjectReferencer.h"
#include "KlawrBlueprintGeneratedClass.h"

#if WITH_EDITOR
#include "BlueprintEditorUtils.h"
#include "AssetRegistryModule.h"
#include "KismetEditorUtilities.h"
#include "CompilerResultsLog.h"
#include "KlawrBlueprint.h"
#endif

DEFINE_LOG_CATEGORY(LogKlawrRuntimePlugin);

namespace Klawr {

UProperty* FindScriptPropertyHelper(const UClass* Class, FName PropertyName)
{
	TFieldIterator<UProperty> PropertyIt(Class, EFieldIteratorFlags::ExcludeSuper);
	for ( ; PropertyIt; ++PropertyIt)
	{
		UProperty* Property = *PropertyIt;
		if (Property->GetFName() == PropertyName)
		{
			return Property; 
		}
	}
	return nullptr;
}

namespace NativeGlue {

// defined in KlawrGeneratedNativeWrappers.inl (included down below)
void RegisterWrapperClasses();

} // namespace NativeGlue

class FRuntimePlugin : public IKlawrRuntimePlugin
{
	int PrimaryEngineAppDomainID;

#if WITH_EDITOR
	int PIEAppDomainID;
#endif // WITH_EDITOR

public:

	FRuntimePlugin()
		: PrimaryEngineAppDomainID(0)
	{
#if WITH_EDITOR
		PIEAppDomainID = 0;
#endif // WITH_EDITOR
	}

#if WITH_EDITOR
	virtual void SetPIEAppDomainID(int AppDomainID) override
	{
		PIEAppDomainID = AppDomainID;
	}

	virtual bool ReloadPrimaryAppDomain() override
	{
		// to ensure that we don't end up without a primary engine app domain because any of the
		// assemblies couldn't be reloaded for whatever reason we create a new app domain before
		// getting rid of the current one
		bool bReloaded = false;
		int NewAppDomainID = 0;
		if (CreateAppDomain(NewAppDomainID))
		{
			if (DestroyPrimaryAppDomain())
			{
				PrimaryEngineAppDomainID = NewAppDomainID;
				bReloaded = true;

			}
			else
			{
				DestroyAppDomain(NewAppDomainID);
			}
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			TArray<FAssetData> AssetData;
			AssetRegistryModule.Get().GetAssetsByClass(UKlawrBlueprint::StaticClass()->GetFName(), AssetData);

			for (const auto& iter : AssetData)
			{
				UE_LOG(LogKlawrRuntimePlugin, Log, TEXT("Recreating Graph for class %s"), *(iter.AssetName.ToString()));
				UKlawrBlueprint* temp = CastChecked<UKlawrBlueprint>(iter.GetAsset());

				FCompilerResultsLog LogResults;
				LogResults.bLogDetailedResults = true;
				LogResults.EventDisplayThresholdMs = 500;
				FKismetEditorUtilities::CompileBlueprint(temp, false, false, false, &LogResults);
			}
		}
		else
		{
			UE_LOG(
				LogKlawrRuntimePlugin, Error,
				TEXT("Failed to create a new primary engine app domain!")
			);
		}
		return bReloaded;
	}

	virtual void GetScriptComponentTypes(TArray<FString>& Types) override
	{
		std::vector<tstring> scriptTypes;
		IClrHost::Get()->GetScriptComponentTypes(PrimaryEngineAppDomainID, scriptTypes);
		Types.Reserve(scriptTypes.size());
		for (const auto& scriptType : scriptTypes)
		{
			const TCHAR* type = *(FString(scriptType.c_str()));
			Types.Add(FString(scriptType.c_str()));
		}
	}
#endif // WITH_EDITOR

	virtual int GetObjectAppDomainID(const UObject* Object) const override
	{
#if WITH_EDITOR
		return (Object->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor)) ?
			PIEAppDomainID : PrimaryEngineAppDomainID;
#else
		return PrimaryEngineAppDomainID;
#endif // WITH_EDITOR
	}

	virtual bool CreatePrimaryAppDomain() override
	{
		bool bCreated = false;

		if (ensure(PrimaryEngineAppDomainID == 0))
		{
			bCreated = CreateAppDomain(PrimaryEngineAppDomainID);

			if (!bCreated)
			{
				UE_LOG(
					LogKlawrRuntimePlugin, Error, 
					TEXT("Failed to create primary engine app domain!")
				);
			}
		}
		
		return bCreated;
	}

	virtual bool DestroyPrimaryAppDomain() override
	{
		bool bDestroyed = DestroyAppDomain(PrimaryEngineAppDomainID);
		if (bDestroyed)
		{
			PrimaryEngineAppDomainID = 0;
		}
		else
		{
			UE_LOG(
				LogKlawrRuntimePlugin, Error, 
				TEXT("Failed to destroy primary engine app domain!")
			);
		}
		return bDestroyed;
	}

	virtual bool CreateAppDomain(int& outAppDomainID) override
	{
		IClrHost* clrHost = IClrHost::Get();
		if (clrHost->CreateEngineAppDomain(outAppDomainID))
		{
			NativeUtils nativeUtils =
			{
				FNativeUtils::Object,
				FNativeUtils::Log,
				FNativeUtils::Array
			};
			return clrHost->InitEngineAppDomain(outAppDomainID, nativeUtils);
		}
		return false;
	}

	virtual bool DestroyAppDomain(int AppDomainID) override
	{
		// nothing to destroy if it doesn't exist
		if (AppDomainID == 0)
		{
			return true;
		}

		bool bDestroyed = IClrHost::Get()->DestroyEngineAppDomain(AppDomainID);

#if WITH_EDITOR
		// FIXME: This isn't very robust, need to improve!
		// hopefully the app domain was unloaded successfully and all references to native 
		// objects within that app domain were released, but in case something went wrong the
		// remaining references need to be cleared out so that UnrealEd can garbage collect the
		// native objects that were referenced in the app domain
		int32 NumReleased = FObjectReferencer::RemoveAllObjectRefsInAppDomain(AppDomainID);
		if (NumReleased > 0)
		{
			UE_LOG(
				LogKlawrRuntimePlugin, Warning,
				TEXT("%d object(s) still referenced in the engine app domain #%d."), 
				NumReleased, AppDomainID
			);
		}
#endif // WITH_EDITOR

		return bDestroyed;
	}

public: // IModuleInterface interface
	
	virtual void StartupModule() override
	{
		FObjectReferencer::Startup();
		FString GameAssembliesDir = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(
				*FPaths::GameDir(), TEXT("Binaries"), FPlatformProcess::GetBinariesSubdirectory(),
				TEXT("Klawr")
			)
		);
		
		if (IClrHost::Get()->Startup(*GameAssembliesDir, TEXT("GameScripts")))
		{
			NativeGlue::RegisterWrapperClasses();
#if !WITH_EDITOR
			// When running in the editor the primary app domain will be created when the Klawr 
			// editor plugin starts up, which will be after the runtime plugin, this is done so that
			// the scripts assembly can be built (if it is missing) before the primary engine app
			// domain attempts to load it.
			CreatePrimaryAppDomain();
#endif // !WITH_EDITOR
		}
		else
		{
			UE_LOG(LogKlawrRuntimePlugin, Error, TEXT("Failed to start CLR!"));
		}
	}
	
	virtual void ShutdownModule() override
	{
		// the host will destroy all app domains on shutdown, there is no need to explicitly
		// destroy the primary app domain
		IClrHost::Get()->Shutdown();
		FObjectReferencer::Shutdown();
	}

	virtual bool SupportsDynamicReloading() override
	{
		// once the CLR has been stopped in a particular process it cannot be started again,
		// so this module cannot be reloaded
		return false;
	}
	void PushAllProperties(int appDomainID, __int64 instanceID, UKlawrScriptComponent* object) const override
	{
		auto bpClass = UKlawrBlueprintGeneratedClass::GetBlueprintGeneratedClass(object->GetClass());
		for (auto prop : bpClass->ScriptProperties)
		{
			FString dummy = prop->GetFName().ToString();
			const TCHAR* propertyName = *(dummy);
			if (prop->GetClass()->IsChildOf(UBoolProperty::StaticClass()))
			{
				UBoolProperty* propBool = Cast<UBoolProperty>(prop);
				SetBool(appDomainID, instanceID, propertyName, propBool->GetPropertyValue_InContainer(object));
			}
			else if (prop->GetClass()->IsChildOf(UIntProperty::StaticClass()))
			{
				UIntProperty* propInt = Cast<UIntProperty>(prop);
				SetInt(appDomainID, instanceID, propertyName, propInt->GetPropertyValue_InContainer(object));
			}
			else if (prop->GetClass()->IsChildOf(UStrProperty::StaticClass()))
			{
				UStrProperty* propStr = Cast<UStrProperty>(prop);
				SetStr(appDomainID, instanceID, propertyName, *(propStr->GetPropertyValue_InContainer(object)));
			}
			else if (prop->GetClass()->IsChildOf(UFloatProperty::StaticClass()))
			{
				UFloatProperty* propFloat = Cast<UFloatProperty>(prop);
				SetFloat(appDomainID, instanceID, propertyName, propFloat->GetPropertyValue_InContainer(object));
			}
			else if (prop->GetClass()->IsChildOf(UObjectProperty::StaticClass()))
			{
				UObjectProperty* propObject = Cast<UObjectProperty>(prop);
				UObject* temp = propObject->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UObject*>(object));
				SetObj(appDomainID, instanceID, propertyName, propObject->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UObject*>(object)));
			}
		}
	}

	void PopAllProperties(int appDomainID, __int64 instanceID, UKlawrScriptComponent* object) const override 
	{
		auto bpClass = UKlawrBlueprintGeneratedClass::GetBlueprintGeneratedClass(object->GetClass());
		for (auto prop : bpClass->ScriptProperties)
		{
			FString dummy = prop->GetFName().ToString();
			const TCHAR* propertyName = *(dummy);
			if (prop->GetClass()->IsChildOf(UBoolProperty::StaticClass()))
			{
				UBoolProperty* propBool = Cast<UBoolProperty>(prop);
				propBool->SetPropertyValue_InContainer(object, GetBool(appDomainID, instanceID, propertyName));
			}
			else if (prop->GetClass()->IsChildOf(UIntProperty::StaticClass()))
			{
				UIntProperty* propInt = Cast<UIntProperty>(prop);
				propInt->SetPropertyValue_InContainer(object, GetInt(appDomainID, instanceID, propertyName));
			}
			else if (prop->GetClass()->IsChildOf(UStrProperty::StaticClass()))
			{
				UStrProperty* propStr = Cast<UStrProperty>(prop);
				propStr->SetPropertyValue_InContainer(object, GetStr(appDomainID, instanceID, propertyName));
			}
			else if (prop->GetClass()->IsChildOf(UFloatProperty::StaticClass()))
			{
				UFloatProperty* propFloat = Cast<UFloatProperty>(prop);
				propFloat->SetPropertyValue_InContainer(object, GetFloat(appDomainID, instanceID, propertyName));
			}
			else if (prop->GetClass()->IsChildOf(UObjectProperty::StaticClass()))
			{
				UObjectProperty* propObject = Cast<UObjectProperty>(prop);
				UObject* returnObject = GetObj(appDomainID, instanceID, propertyName);
				if ((returnObject != NULL) && (returnObject != nullptr))
				{
					propObject->SetObjectPropertyValue(prop->ContainerPtrToValuePtr<UObject*>(object), returnObject);
				}
			}
		}
	}

	void SetFloat(int appDomainID, __int64 instanceID, const TCHAR* propertyName, float value) const override
	{
		IClrHost::Get()->SetFloat(appDomainID, instanceID, propertyName, value);
	}

	void SetInt(int appDomainID, __int64 instanceID, const TCHAR* propertyName, int value) const override
	{
		IClrHost::Get()->SetInt(appDomainID, instanceID, propertyName, value);
	}

	void SetBool(int appDomainID, __int64 instanceID, const TCHAR* propertyName, bool value) const override
	{
		IClrHost::Get()->SetBool(appDomainID, instanceID, propertyName, value);
	}

	void SetStr(int appDomainID, __int64 instanceID, const TCHAR* propertyName, const TCHAR* value) const override
	{
		IClrHost::Get()->SetStr(appDomainID, instanceID, propertyName, value);
	}

	virtual void SetObj(int appDomainID, __int64 instanceID, const TCHAR* propertyName, UObject* value) const override
	{
		if (value)
		{
			if (value != GetObj(appDomainID, instanceID, propertyName))
			{
				FObjectReferencer::AddObjectRef(value);
			}
			IClrHost::Get()->SetObj(appDomainID, instanceID, propertyName, value);
		}
	}


	float GetFloat(int appDomainID, __int64 instanceID, const TCHAR* propertyName) const 
	{
		return IClrHost::Get()->GetFloat(appDomainID, instanceID, propertyName);
	}
	int GetInt(int appDomainID, __int64 instanceID, const TCHAR* propertyName) const
	{
		return IClrHost::Get()->GetInt(appDomainID, instanceID, propertyName);
	}

	bool GetBool(int appDomainID, __int64 instanceID, const TCHAR* propertyName) const
	{
		return IClrHost::Get()->GetBool(appDomainID, instanceID, propertyName);
	}

	const TCHAR* GetStr(int appDomainID, __int64 instanceID, const TCHAR* propertyName) const
	{
		return IClrHost::Get()->GetStr(appDomainID, instanceID, propertyName);
	}

	UObject* GetObj(int appDomainID, __int64 instanceID, const TCHAR* propertyName) const 
	{
		
		UObject* returnValue = IClrHost::Get()->GetObj(appDomainID, instanceID, propertyName);
		return returnValue;
	}

};

} // namespace Klawr

IMPLEMENT_MODULE(Klawr::FRuntimePlugin, KlawrRuntimePlugin)

#include "KlawrGeneratedNativeWrappers.inl"
