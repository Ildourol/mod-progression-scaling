# Reference acquisition

The supplied sibling `progression-scaling-references` directory held 13 separate checkouts. Revisions and default branch reporting came from `git -C <directory> rev-parse HEAD`, `git -C <directory> symbolic-ref --short refs/remotes/origin/HEAD` and `git -C <directory> remote get-url origin`. See the [reference audit](REFERENCE_AUDIT.md) for the observed results. The supplied open-world difficulty URL returned 404. There was no clone attempt for that missing reference. Audit checkouts were never built, installed or updated.

For a clean reference mirror, run the following in the workspace parent of `progression-scaling-references`, outside any active core `modules/` tree. The final entry needs a working upstream URL or access from its maintainer.

```powershell
$referenceRoot = Join-Path (Get-Location) 'progression-scaling-references'
New-Item -ItemType Directory -Force -Path $referenceRoot | Out-Null
$repositories = @(
    'azerothcore/mod-autobalance', 'mod-playerbots/mod-playerbots',
    'mama128/mod-global-scaling', 'azerothcore/mod-zone-difficulty',
    'Maddnes95/mod-ilvl-scaling', 'Corfirean/mod-coa-content-scaling',
    'Conquest-of-AzerothCore/azerothcore-wotlk-coa', 'azerothcore/mod-solocraft',
    'firstoni-dev/mod-zone-scaling', 'Leaf-Spring/mod-character-scale',
    'azerothcore/mod-dynamic-xp', 'Sphynxmods/mod-individual-rates',
    'azerothcore/mod-progression-system', 'Stefan2102/mod-open-world-difficulty'
)
foreach ($repository in $repositories) {
    $destination = Join-Path $referenceRoot ($repository.Split('/')[-1])
    if (Test-Path -LiteralPath $destination) { continue }
    git clone "https://github.com/$repository.git" $destination
    if ($LASTEXITCODE) { Write-Warning "Clone failed for $repository" }
}
```

To update existing clones deliberately, keep any local changes and require a fast-forward.

```powershell
Get-ChildItem -LiteralPath $referenceRoot -Directory | ForEach-Object {
    if (Test-Path -LiteralPath (Join-Path $_.FullName '.git')) {
        git -C $_.FullName pull --ff-only
        if ($LASTEXITCODE) { Write-Warning "Review required for $($_.Name)" }
    }
}
```

Stock AzerothCore was fetched separately into an isolated build staging directory for header and loader verification using `git clone --depth 1 --single-branch --branch master https://github.com/azerothcore/azerothcore-wotlk.git <staging>/stock`. See [Testing](TESTING.md) for the observed SHA.
