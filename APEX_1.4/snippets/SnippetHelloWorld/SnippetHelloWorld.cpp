/*
 * Copyright (c) 2008-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "../SnippetCommon/SnippetCommon.h"

#include "destructible/DestructibleAsset.h"
#include "nvparameterized/NvParameterized.h"
#include "nvparameterized/NvParamUtils.h"

#include "PsString.h"

#include <sys/stat.h>

using namespace nvidia::apex;

class MyResourceCallback;

ApexSDK*					gApexSDK = NULL;
DummyRenderResourceManager* gDummyRenderResourceManager = NULL;
MyResourceCallback*			gMyResourceCallback = NULL;

ModuleDestructible*			gModuleDestructible = NULL;


class MyResourceCallback : public ResourceCallback
{
public:
	virtual void* requestResource(const char* nameSpace, const char* name)
	{
		Asset* asset = 0;

		if (!physx::shdfnd::strcmp(nameSpace, DESTRUCTIBLE_AUTHORING_TYPE_NAME))
		{
			const char* path = name;

			// does file exists?
			struct stat info;
			if ((stat(path, &info) != -1) && (info.st_mode & (S_IFREG)) != 0)
			{
				PxFileBuf* stream = gApexSDK->createStream(path, PxFileBuf::OPEN_READ_ONLY);

				if (stream)
				{
					NvParameterized::Serializer::SerializeType serType = gApexSDK->getSerializeType(*stream);
					NvParameterized::Serializer::ErrorType serError;
					NvParameterized::Serializer*  ser = gApexSDK->createSerializer(serType);
					PX_ASSERT(ser);

					NvParameterized::Serializer::DeserializedData data;
					serError = ser->deserialize(*stream, data);

					if (serError == NvParameterized::Serializer::ERROR_NONE && data.size() == 1)
					{
						NvParameterized::Interface* params = data[0];
						asset = gApexSDK->createAsset(params, name);
						PX_ASSERT(asset && "ERROR Creating NvParameterized Asset");
					}
					else
					{
						PX_ASSERT(0 && "ERROR Deserializing NvParameterized Asset");
					}

					stream->release();
					ser->release();
				}
			}
		}

		PX_ASSERT(asset);
		return asset;
	}

	virtual void  releaseResource(const char*, const char*, void* resource)
	{
		Asset* asset = (Asset*)resource;
		gApexSDK->releaseAsset(*asset);
	}
};


void initApex()
{
	// Fill out the Apex SDKriptor
	ApexSDKDesc apexDesc;

	// Apex needs an allocator and error stream.  By default it uses those of the PhysX SDK.

	// Let Apex know about our PhysX SDK and cooking library
	apexDesc.physXSDK = gPhysics;
	apexDesc.cooking = gCooking;
	apexDesc.pvd = gPvd;

	// Our custom dummy render resource manager
	gDummyRenderResourceManager = new DummyRenderResourceManager();
	apexDesc.renderResourceManager = gDummyRenderResourceManager;

	// Our custom named resource handler
	gMyResourceCallback = new MyResourceCallback();
	apexDesc.resourceCallback = gMyResourceCallback;
	apexDesc.foundation = gFoundation;

	// Finally, create the Apex SDK
	ApexCreateError error;
	gApexSDK = CreateApexSDK(apexDesc, &error);
	PX_ASSERT(gApexSDK);

	// Initialize destructible module
	gModuleDestructible = static_cast<ModuleDestructible*>(gApexSDK->createModule("Destructible"));
	NvParameterized::Interface* params = gModuleDestructible->getDefaultModuleDesc();
	gModuleDestructible->init(*params);
}

Asset* loadApexAsset(const char* path)
{
	Asset* asset = static_cast<Asset*>(gApexSDK->getNamedResourceProvider()->getResource(DESTRUCTIBLE_AUTHORING_TYPE_NAME, path));
	return asset;
}

void releaseAPEX()
{
	gApexSDK->release();
	delete gDummyRenderResourceManager;
	delete gMyResourceCallback;
}

#include "Shlwapi.h"

int main(int, char**)
{
	initPhysX();
	initApex();

	LPTSTR cmd = GetCommandLine();
	PathRemoveFileSpec(cmd);
	strcat(cmd, "/../../snippets/SnippetCommon/cow.apb");


	Asset* asset = loadApexAsset(&cmd[1]);
	shdfnd::printString(asset->getName());
	asset->release();

	releaseAPEX();
	releasePhysX();

	return 0;
}
