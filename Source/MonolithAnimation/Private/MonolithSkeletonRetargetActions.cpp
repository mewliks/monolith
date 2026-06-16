#include "MonolithSkeletonRetargetActions.h"
#include "MonolithAssetUtils.h"
#include "MonolithParamSchema.h"

#include "Animation/Skeleton.h"
#include "ReferenceSkeleton.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h" // GEditor->BeginTransaction / EndTransaction

// ---------------------------------------------------------------------------
// Mode string <-> namespaced enum (parse/echo by NAME — never by raw int).
//
// EBoneTranslationRetargetingMode is `namespace { enum Type : int }`
// (Skeleton.h:69-86). All five members verified live (UE 5.7):
//   Animation, Skeleton, AnimationScaled, AnimationRelative, OrientAndScale.
// ---------------------------------------------------------------------------

bool FMonolithSkeletonRetargetActions::ParseTranslationRetargetMode(
	const FString& In, EBoneTranslationRetargetingMode::Type& Out)
{
	const FString S = In.TrimStartAndEnd();
	if (S.Equals(TEXT("Animation"), ESearchCase::IgnoreCase))
	{
		Out = EBoneTranslationRetargetingMode::Animation; return true;
	}
	if (S.Equals(TEXT("Skeleton"), ESearchCase::IgnoreCase))
	{
		Out = EBoneTranslationRetargetingMode::Skeleton; return true;
	}
	if (S.Equals(TEXT("AnimationScaled"), ESearchCase::IgnoreCase))
	{
		Out = EBoneTranslationRetargetingMode::AnimationScaled; return true;
	}
	if (S.Equals(TEXT("AnimationRelative"), ESearchCase::IgnoreCase))
	{
		Out = EBoneTranslationRetargetingMode::AnimationRelative; return true;
	}
	if (S.Equals(TEXT("OrientAndScale"), ESearchCase::IgnoreCase))
	{
		Out = EBoneTranslationRetargetingMode::OrientAndScale; return true;
	}
	return false;
}

namespace
{
	const TCHAR* TranslationRetargetModeToString(EBoneTranslationRetargetingMode::Type Mode)
	{
		switch (Mode)
		{
			case EBoneTranslationRetargetingMode::Animation:         return TEXT("Animation");
			case EBoneTranslationRetargetingMode::Skeleton:          return TEXT("Skeleton");
			case EBoneTranslationRetargetingMode::AnimationScaled:   return TEXT("AnimationScaled");
			case EBoneTranslationRetargetingMode::AnimationRelative: return TEXT("AnimationRelative");
			case EBoneTranslationRetargetingMode::OrientAndScale:    return TEXT("OrientAndScale");
			default:                                                 return TEXT("Animation");
		}
	}

	// biped_locomotion preset — role-keyed mode map.
	// Applied through the GENERIC setter (this is a thin convenience layer):
	//   root   -> Animation       (let the root translate freely)
	//   pelvis -> AnimationScaled  (scale pelvis height to target proportions)
	//   ik_*   -> Animation        (IK targets must follow source translation)
	//   rest   -> Skeleton         (fixed translation; rotation-only retarget)
	//
	// Matched case-insensitively against the bone name. A name is "pelvis" if it
	// equals/contains "pelvis" or "hips"; "root" if it equals "root"; IK if it
	// begins with "ik_". First match wins in that priority order.
	EBoneTranslationRetargetingMode::Type BipedLocomotionModeForBone(const FString& BoneNameLower)
	{
		if (BoneNameLower.Equals(TEXT("root"), ESearchCase::IgnoreCase))
		{
			return EBoneTranslationRetargetingMode::Animation;
		}
		if (BoneNameLower.Contains(TEXT("pelvis"), ESearchCase::IgnoreCase) ||
			BoneNameLower.Contains(TEXT("hips"), ESearchCase::IgnoreCase))
		{
			return EBoneTranslationRetargetingMode::AnimationScaled;
		}
		if (BoneNameLower.StartsWith(TEXT("ik_"), ESearchCase::IgnoreCase))
		{
			return EBoneTranslationRetargetingMode::Animation;
		}
		return EBoneTranslationRetargetingMode::Skeleton;
	}
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void FMonolithSkeletonRetargetActions::RegisterActions(FMonolithToolRegistry& Registry)
{
	Registry.RegisterAction(TEXT("animation"), TEXT("set_bone_translation_retargeting"),
		TEXT("Set per-bone translation retargeting mode on a Skeleton (legacy USkeleton retargeting). "
			 "Controls how each bone's animated translation is interpreted when anims authored for one "
			 "skeleton play on a differently-proportioned one. Provide 'entries' as a list of "
			 "{bone, mode} (mode in Animation|Skeleton|AnimationScaled|AnimationRelative|OrientAndScale), "
			 "and/or set 'preset':'biped_locomotion' to apply a role-keyed map "
			 "(root=Animation, pelvis=AnimationScaled, ik_*=Animation, rest=Skeleton) across all bones. "
			 "Explicit 'entries' override the preset for the bones they name. 'recursive':true also "
			 "applies each entry's mode to that bone's children (bChildrenToo)."),
		FMonolithActionHandler::CreateStatic(&HandleSetBoneTranslationRetargeting),
		FParamSchemaBuilder()
			.RequiredAssetPath(TEXT("skeleton_path"), TEXT("Skeleton asset path"))
			.Optional(TEXT("entries"), TEXT("array"),
				TEXT("Array of {bone, mode} objects. mode: Animation|Skeleton|AnimationScaled|AnimationRelative|OrientAndScale"))
			.Optional(TEXT("preset"), TEXT("string"),
				TEXT("Named preset to apply across all bones before entries. Supported: biped_locomotion"))
			.Optional(TEXT("recursive"), TEXT("boolean"),
				TEXT("If true, also apply each entry's mode to the bone's children (bChildrenToo). Default false."))
			.Build());

	Registry.RegisterAction(TEXT("animation"), TEXT("get_bone_translation_retargeting"),
		TEXT("Read the current per-bone translation retargeting mode for a Skeleton. Returns one entry "
			 "per bone {bone, mode} (mode by name). Optionally pass 'bones' (array of names) to read only "
			 "those bones."),
		FMonolithActionHandler::CreateStatic(&HandleGetBoneTranslationRetargeting),
		FParamSchemaBuilder()
			.RequiredAssetPath(TEXT("skeleton_path"), TEXT("Skeleton asset path"))
			.Optional(TEXT("bones"), TEXT("array"),
				TEXT("Optional list of bone names to read. Omit to read all bones."))
			.Build());
}

// ---------------------------------------------------------------------------
// T1-R4: set_bone_translation_retargeting
// ---------------------------------------------------------------------------

FMonolithActionResult FMonolithSkeletonRetargetActions::HandleSetBoneTranslationRetargeting(
	const TSharedPtr<FJsonObject>& Params)
{
	const FString SkeletonPath = Params->GetStringField(TEXT("skeleton_path"));

	USkeleton* Skeleton = FMonolithAssetUtils::LoadAssetByPath<USkeleton>(SkeletonPath);
	if (!Skeleton)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath));
	}

	const FString Preset = Params->GetStringField(TEXT("preset"));
	const bool bRecursive = Params->HasField(TEXT("recursive")) && Params->GetBoolField(TEXT("recursive"));

	// Validate the preset name up-front (only biped_locomotion supported in v1).
	const bool bHasPreset = !Preset.IsEmpty();
	if (bHasPreset && !Preset.Equals(TEXT("biped_locomotion"), ESearchCase::IgnoreCase))
	{
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Unknown preset '%s'. Supported presets: biped_locomotion"), *Preset));
	}

	// Parse + validate explicit entries before mutating anything.
	struct FBoneModeEntry
	{
		FString BoneName;
		int32 BoneIndex;
		EBoneTranslationRetargetingMode::Type Mode;
	};
	TArray<FBoneModeEntry> ResolvedEntries;

	const FReferenceSkeleton& RefSkel = Skeleton->GetReferenceSkeleton();
	const int32 NumBones = RefSkel.GetNum();

	const TArray<TSharedPtr<FJsonValue>>* EntriesArr = nullptr;
	if (Params->TryGetArrayField(TEXT("entries"), EntriesArr))
	{
		for (const TSharedPtr<FJsonValue>& Val : *EntriesArr)
		{
			const TSharedPtr<FJsonObject>* EntryObj = nullptr;
			if (!Val->TryGetObject(EntryObj) || !EntryObj->IsValid())
			{
				return FMonolithActionResult::Error(TEXT("Each item in 'entries' must be an object {bone, mode}."));
			}

			FString BoneName;
			if (!(*EntryObj)->TryGetStringField(TEXT("bone"), BoneName) || BoneName.IsEmpty())
			{
				return FMonolithActionResult::Error(TEXT("Each entry requires a non-empty 'bone' field."));
			}

			FString ModeStr;
			if (!(*EntryObj)->TryGetStringField(TEXT("mode"), ModeStr) || ModeStr.IsEmpty())
			{
				return FMonolithActionResult::Error(FString::Printf(
					TEXT("Entry for bone '%s' requires a non-empty 'mode' field."), *BoneName));
			}

			EBoneTranslationRetargetingMode::Type Mode;
			if (!ParseTranslationRetargetMode(ModeStr, Mode))
			{
				return FMonolithActionResult::Error(FString::Printf(
					TEXT("Invalid mode '%s' for bone '%s'. Valid: Animation, Skeleton, AnimationScaled, "
						 "AnimationRelative, OrientAndScale"), *ModeStr, *BoneName));
			}

			const int32 BoneIndex = RefSkel.FindBoneIndex(FName(*BoneName));
			if (BoneIndex == INDEX_NONE)
			{
				return FMonolithActionResult::Error(FString::Printf(
					TEXT("Bone not found in skeleton: %s"), *BoneName));
			}

			ResolvedEntries.Add({BoneName, BoneIndex, Mode});
		}
	}

	if (!bHasPreset && ResolvedEntries.Num() == 0)
	{
		return FMonolithActionResult::Error(
			TEXT("Provide 'entries' (a list of {bone, mode}) and/or 'preset':'biped_locomotion'."));
	}

	// Mutate. The preset is applied first (across every bone via the generic
	// setter); explicit entries are applied afterwards so they override the
	// preset for the bones they name.
	GEditor->BeginTransaction(FText::FromString(TEXT("Set Bone Translation Retargeting")));
	Skeleton->Modify();

	int32 PresetBonesSet = 0;
	if (bHasPreset)
	{
		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			const FString BoneName = RefSkel.GetBoneName(BoneIndex).ToString();
			const EBoneTranslationRetargetingMode::Type Mode = BipedLocomotionModeForBone(BoneName);
			// Preset never uses bChildrenToo — it sets every bone explicitly.
			Skeleton->SetBoneTranslationRetargetingMode(BoneIndex, Mode, /*bChildrenToo=*/false);
			++PresetBonesSet;
		}
	}

	TArray<TSharedPtr<FJsonValue>> AppliedArr;
	for (const FBoneModeEntry& Entry : ResolvedEntries)
	{
		Skeleton->SetBoneTranslationRetargetingMode(Entry.BoneIndex, Entry.Mode, bRecursive);

		TSharedPtr<FJsonObject> AppliedObj = MakeShared<FJsonObject>();
		AppliedObj->SetStringField(TEXT("bone"), Entry.BoneName);
		AppliedObj->SetStringField(TEXT("mode"), TranslationRetargetModeToString(Entry.Mode));
		AppliedArr.Add(MakeShared<FJsonValueObject>(AppliedObj));
	}

	GEditor->EndTransaction();
	Skeleton->MarkPackageDirty(); // dirty is not transactional state — set after EndTransaction

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("skeleton"), SkeletonPath);
	Root->SetBoolField(TEXT("recursive"), bRecursive);
	if (bHasPreset)
	{
		Root->SetStringField(TEXT("preset"), TEXT("biped_locomotion"));
		Root->SetNumberField(TEXT("preset_bones_set"), PresetBonesSet);
	}
	Root->SetArrayField(TEXT("entries_applied"), AppliedArr);
	Root->SetNumberField(TEXT("bone_count"), NumBones);
	return FMonolithActionResult::Success(Root);
}

// ---------------------------------------------------------------------------
// T1-R4: get_bone_translation_retargeting
// ---------------------------------------------------------------------------

FMonolithActionResult FMonolithSkeletonRetargetActions::HandleGetBoneTranslationRetargeting(
	const TSharedPtr<FJsonObject>& Params)
{
	const FString SkeletonPath = Params->GetStringField(TEXT("skeleton_path"));

	USkeleton* Skeleton = FMonolithAssetUtils::LoadAssetByPath<USkeleton>(SkeletonPath);
	if (!Skeleton)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath));
	}

	const FReferenceSkeleton& RefSkel = Skeleton->GetReferenceSkeleton();
	const int32 NumBones = RefSkel.GetNum();

	// Optional bone-name filter.
	TArray<FString> RequestedBones;
	const TArray<TSharedPtr<FJsonValue>>* BonesArr = nullptr;
	if (Params->TryGetArrayField(TEXT("bones"), BonesArr))
	{
		for (const TSharedPtr<FJsonValue>& Val : *BonesArr)
		{
			RequestedBones.Add(Val->AsString());
		}
	}

	TArray<TSharedPtr<FJsonValue>> EntriesArr;
	TArray<FString> NotFound;

	auto EmitBone = [&](int32 BoneIndex)
	{
		// GetBoneTranslationRetargetingMode reads BoneTree[BoneTreeIdx]; the
		// skeleton's BoneTree is index-parallel with the reference skeleton.
		const EBoneTranslationRetargetingMode::Type Mode =
			Skeleton->GetBoneTranslationRetargetingMode(BoneIndex);

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("bone"), RefSkel.GetBoneName(BoneIndex).ToString());
		Obj->SetStringField(TEXT("mode"), TranslationRetargetModeToString(Mode));
		EntriesArr.Add(MakeShared<FJsonValueObject>(Obj));
	};

	if (RequestedBones.Num() > 0)
	{
		for (const FString& BoneName : RequestedBones)
		{
			const int32 BoneIndex = RefSkel.FindBoneIndex(FName(*BoneName));
			if (BoneIndex == INDEX_NONE)
			{
				NotFound.Add(BoneName);
				continue;
			}
			EmitBone(BoneIndex);
		}
	}
	else
	{
		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			EmitBone(BoneIndex);
		}
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("skeleton"), SkeletonPath);
	Root->SetNumberField(TEXT("bone_count"), NumBones);
	Root->SetArrayField(TEXT("entries"), EntriesArr);
	if (NotFound.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> NotFoundArr;
		for (const FString& NF : NotFound)
		{
			NotFoundArr.Add(MakeShared<FJsonValueString>(NF));
		}
		Root->SetArrayField(TEXT("not_found"), NotFoundArr);
	}
	return FMonolithActionResult::Success(Root);
}
