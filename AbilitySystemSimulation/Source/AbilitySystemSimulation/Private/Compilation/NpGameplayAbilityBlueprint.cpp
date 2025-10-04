


#include "Compilation/NpGameplayAbilityBlueprint.h"

#include "Compilation/NpAbilityGeneratedClass.h"
#if WITH_EDITOR
UClass* UNpGameplayAbilityBlueprint::GetBlueprintClass() const
{
	return UNpAbilityGeneratedClass::StaticClass();
}
#endif
