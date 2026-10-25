# Patches to BearWare's TeamTalk5

TT Central's client is BearWare's qtTeamTalk plus the patches here. They are
applied in order to the pinned submodule `external/TeamTalk5` (tag `v5.23`) by
`scripts/bootstrap.py`. Like the code they modify, they are **GPLv3**.

| Patch | What it does |
|---|---|
| `TeamTalk5/0001-Link-the-Professional-TeamTalk5Pro-SDK-library.patch` | Client links `TeamTalk5Pro` (the same change BearWare makes in its Pro SDK) |

## Adding a patch

Patches are made against the submodule root, so paths look like
`a/Client/qtTeamTalk/...`.

```bat
REM 1. Start from pristine upstream with the existing patches applied
git -C external/TeamTalk5 checkout -- .
python scripts\bootstrap.py

REM 2. Stage that state as a local baseline (inside the submodule only)
git -C external/TeamTalk5 add -A

REM 3. Make your change in external/TeamTalk5, then capture only that change
git -C external/TeamTalk5 diff > patches\TeamTalk5\0002-Short-description.patch

REM 4. Unstage the baseline and check the whole series applies from pristine
git -C external/TeamTalk5 reset -q
git -C external/TeamTalk5 checkout -- .
python scripts\bootstrap.py
```

Then add a short header to the top of the patch (subject, why, upstream tag),
like `0001`. Keep each patch small and about one thing.

## Updating upstream

Move the submodule to the new BearWare tag, fetch the matching SDK
(`python scripts\bootstrap.py --refresh-sdk`, after updating the version in
`scripts/fetch_sdk.py`), and refresh any patch that no longer applies.
`bootstrap.py` refuses to continue if the header and DLL versions differ.
