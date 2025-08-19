param([Parameter(Mandatory=$true)][string]$Root)

$bundle = Join-Path $Root 'Saved\Automation\_Bundle'
if (Test-Path $bundle) { Remove-Item -Recurse -Force $bundle }
New-Item -ItemType Directory -Path $bundle | Out-Null

$paths = @(
  'Saved\Automation\Editor\Automation',
  'Saved\Automation\Client\Func\Automation',
  'Saved\Automation\Client\AI\Automation'
)

foreach ($p in $paths) {
  $src = Join-Path $Root $p
  if (Test-Path $src) {
    robocopy $src (Join-Path $bundle (Split-Path $p -Leaf)) *.xml *.json *.html /E | Out-Null
  }
}

$top = Join-Path $Root 'Saved\Automation'
if (Test-Path (Join-Path $top 'index.json')) {
  Copy-Item (Join-Path $top 'index.*') $bundle -ErrorAction SilentlyContinue
}

$bot = Join-Path $Root 'Saved\Automation\BotResults\Latest.json'
if (Test-Path $bot) {
  $botDir = Join-Path $bundle 'BotResults'
  New-Item -ItemType Directory -Path $botDir -Force | Out-Null
  Copy-Item $bot $botDir -Force
}

$logs = Join-Path $Root 'Saved\Logs'
if (Test-Path $logs) {
  $logsDir = Join-Path $bundle 'Logs'
  New-Item -ItemType Directory -Path $logsDir -Force | Out-Null
  robocopy $logs $logsDir *.log /E | Out-Null
}

$auto = Join-Path $Root 'PerformanceTestResults_Automation.csv'
if (Test-Path $auto) {
  Copy-Item $auto (Join-Path $bundle 'Performance_Automation.csv') -Force
}

$arch = Join-Path $Root 'ArchivedResults'
if (Test-Path $arch) {
  $latest = Get-ChildItem $arch -Filter 'PerformanceTestResults_*.csv' |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1
  if ($latest) {
    Copy-Item $latest.FullName (Join-Path $bundle 'Performance_Archive_Latest.csv') -Force
  }
}
