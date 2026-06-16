#pragma once

#include "CoreMinimal.h"
#include "MonolithToolRegistry.h"
#include "Animation/Skeleton.h" // EBoneTranslationRetargetingMode (namespaced enum) — parse/echo by name

class USkeleton;

/**
 * Legacy per-bone translation retargeting authoring (T1-R4).
 *
 * Wraps the plain ENGINE_API surface on USkeleton:
 *   - SetBoneTranslationRetargetingMode(int32 BoneIndex, EBoneTranslationRetargetingMode::Type, bool bChildrenToo)
 *   - GetBoneTranslationRetargetingMode(int32 BoneTreeIdx, bool) const
 *
 * These control how a bone's animated translation is interpreted when an
 * animation authored for one skeleton plays on a differently-proportioned one
 * (the classic "wrong-height pelvis / sliding feet" cross-skeleton problem).
 *
 * EBoneTranslationRetargetingMode is a NAMESPACED enum
 * (`namespace EBoneTranslationRetargetingMode { enum Type : int }`,
 * Skeleton.h:69-86) — its five members are Animation / Skeleton /
 * AnimationScaled / AnimationRelative / OrientAndScale. Parsed/echoed by
 * name (never by raw int) via ParseTranslationRetargetMode /
 * TranslationRetargetModeToString.
 *
 * The `biped_locomotion` preset is a thin convenience layer: a role-keyed map
 * (root -> Animation, pelvis -> AnimationScaled, ik_* -> Animation,
 * everything else -> Skeleton) applied through the same generic setter.
 *
 * All actions are editor/game-thread, synchronous, single-call. Writes wrap the
 * mutation in a transaction and MarkPackageDirty(); reads skip the transaction.
 */
class FMonolithSkeletonRetargetActions
{
public:
	/** Register the T1-R4 actions with the tool registry. */
	static void RegisterActions(FMonolithToolRegistry& Registry);

	// --- T1-R4: per-bone translation retargeting ---
	static FMonolithActionResult HandleSetBoneTranslationRetargeting(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleGetBoneTranslationRetargeting(const TSharedPtr<FJsonObject>& Params);

	/**
	 * Parse a mode string (case-insensitive) into the namespaced enum value.
	 * Accepts: Animation | Skeleton | AnimationScaled | AnimationRelative | OrientAndScale.
	 * Returns false (Out untouched) if the string matches no member.
	 */
	static bool ParseTranslationRetargetMode(const FString& In, EBoneTranslationRetargetingMode::Type& Out);
};
