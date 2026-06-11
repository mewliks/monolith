// SPDX-License-Identifier: MIT
// LLM C++ authoring ergonomics — Phase 1 automation tests (items 1-3).
// Plan: Plugins/Monolith/Docs/plans/2026-06-10-llm-cpp-ergonomics-actions.md (§12 Phase 1).
//
// Tests use disposable SQLite DBs at FPaths::AutomationTransientDir(), never the
// real EngineSource.db. Fixtures live under
// Source/MonolithSource/Private/Tests/Fixtures/CppErgoCorpus/.
//
// DEVIATION NOTE: the plan §12 names IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST;
// this module's existing tests (and the sibling RI tests) use
// IMPLEMENT_SIMPLE_AUTOMATION_TEST, which is what compiles here. We match the
// in-tree idiom.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "MonolithSourceActions.h"
#include "MonolithSourceDatabase.h"
#include "MonolithSourceIndexer.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "SQLiteDatabase.h"

namespace MonolithCppErgoTestDetail
{
	/** Resolve the fixture corpus dir relative to the Monolith plugin install. */
	static FString GetFixtureCorpusDir()
	{
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Monolith"));
		if (Plugin.IsValid())
		{
			return Plugin->GetBaseDir()
				/ TEXT("Source") / TEXT("MonolithSource")
				/ TEXT("Private") / TEXT("Tests") / TEXT("Fixtures") / TEXT("CppErgoCorpus");
		}
		return FPaths::ProjectPluginsDir()
			/ TEXT("Monolith") / TEXT("Source") / TEXT("MonolithSource")
			/ TEXT("Private") / TEXT("Tests") / TEXT("Fixtures") / TEXT("CppErgoCorpus");
	}

	/** A disposable temp DB path under AutomationTransientDir. */
	static FString MakeTempDbPath()
	{
		const FString Dir = FPaths::AutomationTransientDir();
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Dir);
		const FString Path = Dir / FString::Printf(TEXT("cppergo-test-%s.db"), *FGuid::NewGuid().ToString());
		IFileManager::Get().Delete(*Path, /*bRequireExists=*/false, /*bEvenReadOnly=*/true);
		return Path;
	}
}

// ---------------------------------------------------------------------------
// Test 1: DeprecationSchemaBootstrap — empty-DB CreateTablesIfNeeded() creates
// symbol_deprecations and stamps SchemaVersion 2.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCppErgoDeprecationSchemaBootstrapTest,
	"Monolith.Source.CppErgonomics.DeprecationSchemaBootstrap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCppErgoDeprecationSchemaBootstrapTest::RunTest(const FString& /*Parameters*/)
{
	using namespace MonolithCppErgoTestDetail;

	const FString DbPath = MakeTempDbPath();
	{
		FMonolithSourceDatabase DB;
		TestTrue(TEXT("OpenForWriting"), DB.OpenForWriting(DbPath));
		TestTrue(TEXT("CreateTablesIfNeeded"), DB.CreateTablesIfNeeded());

		// schema_version meta == 2
		TestEqual(TEXT("schema_version stamped to 2"), DB.GetMeta(TEXT("schema_version")), FString(TEXT("2")));

		// Inserting a deprecation row succeeds (table exists) and counts.
		DB.InsertDeprecation(/*SymbolId=*/0, TEXT("Foo"), TEXT("5.4"), TEXT("Use Bar"), TEXT("UE_DEPRECATED"));
		TestEqual(TEXT("one deprecation row"), DB.GetDeprecationCount(), 1);

		TOptional<FMonolithDeprecationRow> Got = DB.GetDeprecation(TEXT("Foo"));
		TestTrue(TEXT("GetDeprecation returns a value"), Got.IsSet());
		if (Got.IsSet())
		{
			TestEqual(TEXT("version"), Got.GetValue().Version, FString(TEXT("5.4")));
			TestEqual(TEXT("message"), Got.GetValue().Message, FString(TEXT("Use Bar")));
			TestEqual(TEXT("kind"), Got.GetValue().Kind, FString(TEXT("UE_DEPRECATED")));
		}
		DB.Close();
	}
	IFileManager::Get().Delete(*DbPath, /*bRequireExists=*/false, /*bEvenReadOnly=*/true);
	return true;
}

// ---------------------------------------------------------------------------
// Test 2: DeprecationIndexExtraction — index the fixture corpus (project-only,
// no engine) and assert two rows with parsed names, version/message/kind, and
// symbol_id = NULL (class-body methods have no symbols row).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCppErgoDeprecationIndexExtractionTest,
	"Monolith.Source.CppErgonomics.DeprecationIndexExtraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCppErgoDeprecationIndexExtractionTest::RunTest(const FString& /*Parameters*/)
{
	using namespace MonolithCppErgoTestDetail;

	const FString CorpusDir = GetFixtureCorpusDir();
	if (!IFileManager::Get().DirectoryExists(*CorpusDir))
	{
		AddError(FString::Printf(TEXT("Fixture corpus not found: %s"), *CorpusDir));
		return false;
	}

	const FString DbPath = MakeTempDbPath();

	// Index ONLY the fixture project corpus (engine source path empty -> skipped).
	{
		FMonolithSourceIndexer Indexer;
		Indexer.SetSourcePath(TEXT(""));            // skip engine phase
		Indexer.SetShaderPath(TEXT(""));
		Indexer.SetProjectPath(CorpusDir);          // ProjectPath/Source/* discovered
		Indexer.SetDatabasePath(DbPath);
		Indexer.SetCleanBuild(true);
		Indexer.SetIndexProjectSource(true);
		TestTrue(TEXT("RunSynchronous"), Indexer.RunSynchronous());
	}

	// Read back the rows.
	FMonolithSourceDatabase DB;
	if (!DB.Open(DbPath))
	{
		AddError(TEXT("Failed to reopen indexed DB"));
		IFileManager::Get().Delete(*DbPath, false, true);
		return false;
	}

	TestEqual(TEXT("two deprecation rows extracted"), DB.GetDeprecationCount(), 2);

	// Foo — UE_DEPRECATED(5.4, "Use Bar instead")
	TOptional<FMonolithDeprecationRow> Foo = DB.GetDeprecation(TEXT("Foo"));
	TestTrue(TEXT("Foo deprecated"), Foo.IsSet());
	if (Foo.IsSet())
	{
		TestEqual(TEXT("Foo version"), Foo.GetValue().Version, FString(TEXT("5.4")));
		TestEqual(TEXT("Foo message"), Foo.GetValue().Message, FString(TEXT("Use Bar instead")));
		TestEqual(TEXT("Foo kind"), Foo.GetValue().Kind, FString(TEXT("UE_DEPRECATED")));
	}

	// Baz — UE_DEPRECATED_FORGAME(5.5, "Baz is gone")
	TOptional<FMonolithDeprecationRow> Baz = DB.GetDeprecation(TEXT("Baz"));
	TestTrue(TEXT("Baz deprecated"), Baz.IsSet());
	if (Baz.IsSet())
	{
		TestEqual(TEXT("Baz version"), Baz.GetValue().Version, FString(TEXT("5.5")));
		TestEqual(TEXT("Baz message"), Baz.GetValue().Message, FString(TEXT("Baz is gone")));
		TestEqual(TEXT("Baz kind"), Baz.GetValue().Kind, FString(TEXT("UE_DEPRECATED_FORGAME")));
	}

	// StillFine must NOT be present.
	TestFalse(TEXT("StillFine not deprecated"), DB.GetDeprecation(TEXT("StillFine")).IsSet());

	// symbol_id NULL for both (class-body methods are not indexed as symbols).
	{
		FSQLiteDatabase* Raw = DB.GetRawHandle();
		if (Raw)
		{
			FSQLitePreparedStatement Stmt;
			Stmt.Create(*Raw, TEXT("SELECT COUNT(*) FROM symbol_deprecations WHERE symbol_id IS NULL;"));
			int32 NullCount = -1;
			if (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
			{
				int64 C = 0;
				Stmt.GetColumnValueByIndex(0, C);
				NullCount = static_cast<int32>(C);
			}
			TestEqual(TEXT("both rows have symbol_id NULL"), NullCount, 2);
		}
	}

	DB.Close();
	IFileManager::Get().Delete(*DbPath, false, true);
	return true;
}

// ---------------------------------------------------------------------------
// Test 3: IncludePathDerivation — pure unit over DeriveIncludePath.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCppErgoIncludePathDerivationTest,
	"Monolith.Source.CppErgonomics.IncludePathDerivation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCppErgoIncludePathDerivationTest::RunTest(const FString& /*Parameters*/)
{
	bool bIncludable = false;
	FString Warning;

	// Public/ -> strip, includable.
	{
		const FString In = TEXT("D:/Proj/Source/MyMod/Public/Sub/Thing.h");
		const FString Out = FMonolithSourceActions::DeriveIncludePath(In, bIncludable, Warning);
		TestEqual(TEXT("Public/ strips prefix"), Out, FString(TEXT("Sub/Thing.h")));
		TestTrue(TEXT("Public/ includable"), bIncludable);
		TestTrue(TEXT("Public/ no warning"), Warning.IsEmpty());
	}

	// Classes/ -> strip, includable.
	{
		const FString In = TEXT("D:/Proj/Source/MyMod/Classes/X.h");
		const FString Out = FMonolithSourceActions::DeriveIncludePath(In, bIncludable, Warning);
		TestEqual(TEXT("Classes/ strips prefix"), Out, FString(TEXT("X.h")));
		TestTrue(TEXT("Classes/ includable"), bIncludable);
	}

	// Internal/ -> strip, includable.
	{
		const FString In = TEXT("D:/Proj/Source/MyMod/Internal/Y.h");
		const FString Out = FMonolithSourceActions::DeriveIncludePath(In, bIncludable, Warning);
		TestEqual(TEXT("Internal/ strips prefix"), Out, FString(TEXT("Y.h")));
		TestTrue(TEXT("Internal/ includable"), bIncludable);
	}

	// Private/ -> NOT includable, same-module relative, warning names the module.
	{
		Warning.Empty();
		const FString In = TEXT("D:/Proj/Source/MyMod/Private/Z.h");
		const FString Out = FMonolithSourceActions::DeriveIncludePath(In, bIncludable, Warning);
		TestEqual(TEXT("Private/ same-module relative"), Out, FString(TEXT("Z.h")));
		TestFalse(TEXT("Private/ NOT includable"), bIncludable);
		TestTrue(TEXT("Private/ warning present"), Warning.Contains(TEXT("Private header")));
		TestTrue(TEXT("Private/ warning names module"), Warning.Contains(TEXT("MyMod")));
	}

	// Backslashes + no recognised prefix -> basename fallback.
	{
		Warning.Empty();
		const FString In = TEXT("C:\\Engine\\Source\\Runtime\\Core\\Foo\\Bar.h");
		const FString Out = FMonolithSourceActions::DeriveIncludePath(In, bIncludable, Warning);
		TestEqual(TEXT("no-prefix basename fallback"), Out, FString(TEXT("Bar.h")));
		TestTrue(TEXT("fallback includable"), bIncludable);
	}

	return true;
}

// ---------------------------------------------------------------------------
// Test 4: SignatureCompaction — CompactDeclaration strips inline bodies + macro
// continuations and joins multi-line declarations. No body leaks.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCppErgoSignatureCompactionTest,
	"Monolith.Source.CppErgonomics.SignatureCompaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCppErgoSignatureCompactionTest::RunTest(const FString& /*Parameters*/)
{
	using namespace MonolithCppErgoTestDetail;

	const FString FixturePath = GetFixtureCorpusDir() / TEXT("Signatures.h");
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *FixturePath))
	{
		AddError(FString::Printf(TEXT("Could not load signature fixture: %s"), *FixturePath));
		return false;
	}

	// Locate fixture declarations by content (line numbers are not assumed exact).
	int32 MultiIdx = INDEX_NONE, InlineIdx = INDEX_NONE, MacroIdx = INDEX_NONE;
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		if (Lines[i].Contains(TEXT("MultiLineDecl"))) MultiIdx = i;
		else if (Lines[i].Contains(TEXT("GetTransform()")) && Lines[i].Contains(TEXT("{"))) InlineIdx = i;
		else if (Lines[i].Contains(TEXT("void Thing(")))   MacroIdx = i;
	}

	TestTrue(TEXT("found MultiLineDecl"), MultiIdx != INDEX_NONE);
	TestTrue(TEXT("found inline GetTransform"), InlineIdx != INDEX_NONE);
	TestTrue(TEXT("found macro Thing"), MacroIdx != INDEX_NONE);

	if (MultiIdx != INDEX_NONE)
	{
		const FString Sig = FMonolithSourceActions::CompactDeclaration(Lines, MultiIdx);
		TestEqual(TEXT("multi-line joined"), Sig,
			FString(TEXT("float MultiLineDecl( int32 First, const FString& Second) const")));
		TestFalse(TEXT("multi-line no body"), Sig.Contains(TEXT("{")));
	}

	if (InlineIdx != INDEX_NONE)
	{
		const FString Sig = FMonolithSourceActions::CompactDeclaration(Lines, InlineIdx);
		// Body { return ... } must be cut.
		TestFalse(TEXT("inline body stripped"), Sig.Contains(TEXT("return")));
		TestFalse(TEXT("inline no brace"), Sig.Contains(TEXT("{")));
		TestTrue(TEXT("inline keeps signature"), Sig.Contains(TEXT("GetTransform()")));
	}

	if (MacroIdx != INDEX_NONE)
	{
		const FString Sig = FMonolithSourceActions::CompactDeclaration(Lines, MacroIdx);
		// The `void Thing(int32 x) { DoThing(x); }` line: body stripped.
		TestFalse(TEXT("macro body stripped"), Sig.Contains(TEXT("DoThing")));
		TestFalse(TEXT("macro no brace"), Sig.Contains(TEXT("{")));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
