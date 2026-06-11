#pragma once

#include "CoreMinimal.h"
#include "MonolithToolRegistry.h"

class FMonolithSourceDatabase;

/**
 * 9 engine source intelligence actions + 1 reindex trigger.
 * Ports the Python unreal-source-mcp server tools to native C++.
 */
class FMonolithSourceActions
{
public:
	static void RegisterAll();

	// Public for unit testing (MonolithCppErgonomicsTest.cpp) — pure, stateless helpers.
	/**
	 * Derive the canonical #include form from an indexed file path. A path under
	 * Public/ | Classes/ | Internal/ strips that prefix and returns an includable
	 * cross-module form (bOutIncludable = true). A Private/ path is NOT includable
	 * from another module: bOutIncludable = false, the same-module relative form is
	 * returned, and OutWarning carries the not-includable note. No recognised prefix
	 * (e.g. an engine header outside the Public/Private convention) -> basename
	 * fallback. Always forward-slashed.
	 */
	static FString DeriveIncludePath(const FString& IndexedFilePath, bool& bOutIncludable, FString& OutWarning);

	/**
	 * Compact a (possibly multi-line) declaration into a single-line signature:
	 * accumulates from StartIdx forward to the closing of the parameter list and
	 * the terminating `;` or opening `{`, strips trailing macro `\` continuations
	 * and any inline body, and collapses whitespace. Used by get_signature
	 * (item 2) for the declaration-read path and exposed for unit testing.
	 */
	static FString CompactDeclaration(const TArray<FString>& Lines, int32 StartIdx);

private:
	// Action handlers
	static FMonolithActionResult HandleReadSource(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleFindReferences(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleFindCallers(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleFindCallees(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleSearchSource(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleGetClassHierarchy(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleGetModuleInfo(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleGetSymbolContext(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleReadFile(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleTriggerReindex(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleTriggerProjectReindex(const TSharedPtr<FJsonObject>& Params);

	// Phase 1 — demand-proven lookups (LLM C++ authoring ergonomics)
	static FMonolithActionResult HandleGetIncludePath(const TSharedPtr<FJsonObject>& Params);    // item 1
	static FMonolithActionResult HandleGetSignature(const TSharedPtr<FJsonObject>& Params);      // item 2
	static FMonolithActionResult HandleCheckDeprecations(const TSharedPtr<FJsonObject>& Params); // item 3

	// Helpers
	static FMonolithSourceDatabase* GetDB();
	static FString ShortPath(const FString& FullPath);

	/** Resolve the owning module name (+ Build.cs note) for a symbol via the source DB (files->modules join). */
	static bool ResolveOwningModule(FMonolithSourceDatabase* DB, const FString& Symbol, FString& OutModule, FString& OutBuildCsNote);

	static FString ReadFileLines(const FString& FilePath, int32 StartLine, int32 EndLine);
	static bool IsForwardDeclaration(const FString& FilePath, int32 LineStart, int32 LineEnd);
	static FString ExtractMembers(const FString& FilePath, int32 StartLine, int32 EndLine);

	static FString MakeTextResult(const FString& Text);

	// Hierarchy walk helpers
	struct FHierarchyCounter
	{
		int32 Shown = 0;
		int32 Truncated = 0;
		int32 Limit = 80;
	};
	static void WalkAncestors(FMonolithSourceDatabase* DB, int64 SymId, TArray<FString>& Lines, int32 Indent, int32 MaxDepth, FHierarchyCounter& Counter, TSet<int64>& Visited);
	static void WalkDescendants(FMonolithSourceDatabase* DB, int64 SymId, TArray<FString>& Lines, int32 Indent, int32 MaxDepth, FHierarchyCounter& Counter, TSet<int64>& Visited);
};
