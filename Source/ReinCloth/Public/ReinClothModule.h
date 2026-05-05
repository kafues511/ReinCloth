// Copyright © 2026 kafues511 All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FReinClothModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
