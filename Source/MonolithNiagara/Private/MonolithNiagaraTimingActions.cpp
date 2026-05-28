#include "MonolithNiagaraTimingActions.h"
#include "MonolithJsonUtils.h"
#include "MonolithParamSchema.h"

DEFINE_LOG_CATEGORY_STATIC(LogMonolithNiagaraTiming, Log, All);

// ============================================================================
//  Registration
// ============================================================================

void FMonolithNiagaraTimingActions::RegisterActions(FMonolithToolRegistry& Registry)
{
	Registry.RegisterAction(TEXT("niagara"), TEXT("get_system_timing"),
		TEXT("**Phase 0 stub.** Read system-level timing fields (warmup, fixed tick delta, require current frame data). Not yet implemented."),
		FMonolithActionHandler::CreateStatic(&HandleGetSystemTiming),
		FParamSchemaBuilder()
			.RequiredAssetPath(TEXT("asset_path"), TEXT("Niagara system asset"))
			.Build());

	Registry.RegisterAction(TEXT("niagara"), TEXT("set_warmup_profile"),
		TEXT("**Phase 0 stub.** Composite write of warmup_time + warmup_tick_delta on a Niagara system. Not yet implemented."),
		FMonolithActionHandler::CreateStatic(&HandleSetWarmupProfile),
		FParamSchemaBuilder()
			.RequiredAssetPath(TEXT("asset_path"), TEXT("Niagara system asset"))
			.Build());

	Registry.RegisterAction(TEXT("niagara"), TEXT("set_fixed_tick_delta"),
		TEXT("**Phase 0 stub.** Set bFixedTickDelta + FixedTickDeltaTime on a Niagara system. Not yet implemented."),
		FMonolithActionHandler::CreateStatic(&HandleSetFixedTickDelta),
		FParamSchemaBuilder()
			.RequiredAssetPath(TEXT("asset_path"), TEXT("Niagara system asset"))
			.Build());

	Registry.RegisterAction(TEXT("niagara"), TEXT("set_require_current_frame_data"),
		TEXT("**Phase 0 stub.** Toggle bRequireCurrentFrameData on a Niagara system. Not yet implemented."),
		FMonolithActionHandler::CreateStatic(&HandleSetRequireCurrentFrameData),
		FParamSchemaBuilder()
			.RequiredAssetPath(TEXT("asset_path"), TEXT("Niagara system asset"))
			.Build());

	Registry.RegisterAction(TEXT("niagara"), TEXT("set_emitter_loop_profile"),
		TEXT("**Phase 0 stub.** Composite write of EmitterState loop inputs (LoopBehavior, LoopDuration, LoopDelay, LoopCount, bLoopDelayEnabled). Not yet implemented."),
		FMonolithActionHandler::CreateStatic(&HandleSetEmitterLoopProfile),
		FParamSchemaBuilder()
			.RequiredAssetPath(TEXT("asset_path"), TEXT("Niagara system asset"))
			.Build());

	Registry.RegisterAction(TEXT("niagara"), TEXT("get_emitter_timing_summary"),
		TEXT("**Phase 0 stub.** Aggregated read of emitter timing (loop config, sim stages, particle lifetime bounds). Not yet implemented."),
		FMonolithActionHandler::CreateStatic(&HandleGetEmitterTimingSummary),
		FParamSchemaBuilder()
			.RequiredAssetPath(TEXT("asset_path"), TEXT("Niagara system asset"))
			.Build());

	Registry.RegisterAction(TEXT("niagara"), TEXT("set_sim_stage_iteration_count"),
		TEXT("**Phase 0 stub.** Alias setting NumIterations on a UNiagaraSimulationStageGeneric. Not yet implemented."),
		FMonolithActionHandler::CreateStatic(&HandleSetSimStageIterationCount),
		FParamSchemaBuilder()
			.RequiredAssetPath(TEXT("asset_path"), TEXT("Niagara system asset"))
			.Build());

	Registry.RegisterAction(TEXT("niagara"), TEXT("set_sim_stage_execute_behavior"),
		TEXT("**Phase 0 stub.** Alias setting ExecuteBehavior on a UNiagaraSimulationStageGeneric. Not yet implemented."),
		FMonolithActionHandler::CreateStatic(&HandleSetSimStageExecuteBehavior),
		FParamSchemaBuilder()
			.RequiredAssetPath(TEXT("asset_path"), TEXT("Niagara system asset"))
			.Build());

	Registry.RegisterAction(TEXT("niagara"), TEXT("set_particle_lifetime"),
		TEXT("**Phase 0 stub.** Set Lifetime min/max on the Initialize Particle module. Not yet implemented."),
		FMonolithActionHandler::CreateStatic(&HandleSetParticleLifetime),
		FParamSchemaBuilder()
			.RequiredAssetPath(TEXT("asset_path"), TEXT("Niagara system asset"))
			.Build());
}

// ============================================================================
//  Handlers (Phase 0 stubs)
// ============================================================================

FMonolithActionResult FMonolithNiagaraTimingActions::HandleGetSystemTiming(const TSharedPtr<FJsonObject>& Params)
{
	return FMonolithActionResult::Error(TEXT("not implemented"));
}

FMonolithActionResult FMonolithNiagaraTimingActions::HandleSetWarmupProfile(const TSharedPtr<FJsonObject>& Params)
{
	return FMonolithActionResult::Error(TEXT("not implemented"));
}

FMonolithActionResult FMonolithNiagaraTimingActions::HandleSetFixedTickDelta(const TSharedPtr<FJsonObject>& Params)
{
	return FMonolithActionResult::Error(TEXT("not implemented"));
}

FMonolithActionResult FMonolithNiagaraTimingActions::HandleSetRequireCurrentFrameData(const TSharedPtr<FJsonObject>& Params)
{
	return FMonolithActionResult::Error(TEXT("not implemented"));
}

FMonolithActionResult FMonolithNiagaraTimingActions::HandleSetEmitterLoopProfile(const TSharedPtr<FJsonObject>& Params)
{
	return FMonolithActionResult::Error(TEXT("not implemented"));
}

FMonolithActionResult FMonolithNiagaraTimingActions::HandleGetEmitterTimingSummary(const TSharedPtr<FJsonObject>& Params)
{
	return FMonolithActionResult::Error(TEXT("not implemented"));
}

FMonolithActionResult FMonolithNiagaraTimingActions::HandleSetSimStageIterationCount(const TSharedPtr<FJsonObject>& Params)
{
	return FMonolithActionResult::Error(TEXT("not implemented"));
}

FMonolithActionResult FMonolithNiagaraTimingActions::HandleSetSimStageExecuteBehavior(const TSharedPtr<FJsonObject>& Params)
{
	return FMonolithActionResult::Error(TEXT("not implemented"));
}

FMonolithActionResult FMonolithNiagaraTimingActions::HandleSetParticleLifetime(const TSharedPtr<FJsonObject>& Params)
{
	return FMonolithActionResult::Error(TEXT("not implemented"));
}
