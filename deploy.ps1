# deploy.ps1 — Deploy tikipac.com to TIKI-100 emulator
#
# Copies workbase.dsk to work.dsk, adds tikipac.com, and boots the
# Djupdal emulator with work.dsk on drive A:.
#
# Usage:
#   .\deploy.ps1              Build (if needed), deploy, launch emulator
#   .\deploy.ps1 -NoLaunch    Deploy only, don't start emulator
#   .\deploy.ps1 -NoBuild     Skip build step

param(
    [switch]$NoLaunch,
    [switch]$NoBuild
)

$ProjectRoot = $PSScriptRoot
$BuildDir    = "$ProjectRoot\build"
$DskDir      = "$ProjectRoot\dsk"
$EmuExe      = "$ProjectRoot\tikiemul.exe"
$BaseDisk    = "$DskDir\workbase.dsk"
$WorkDisk    = "$DskDir\work.dsk"

$comFile = "$BuildDir\tikitet.com"
$comName = "TIKITET"

# === TIKI-100 400K DISK PARAMETERS ===
$SECTOR_SIZE       = 512
$SECTORS_PER_TRACK = 10
$SIDES             = 2
$TRACKS            = 40
$BLOCK_SIZE        = 2048
$DIR_ENTRIES       = 128
$RESERVED_TRACKS   = 1
$DIR_ENTRY_SIZE    = 32
$CPM_RECORD_SIZE   = 128
$CPM_EMPTY         = 0xE5
$IMAGE_SIZE        = 409600

$DATA_OFFSET       = $RESERVED_TRACKS * $SIDES * $SECTORS_PER_TRACK * $SECTOR_SIZE
$DIR_SIZE          = $DIR_ENTRIES * $DIR_ENTRY_SIZE
$DIR_BLOCKS        = $DIR_SIZE / $BLOCK_SIZE
$FIRST_DATA_BLOCK  = $DIR_BLOCKS

Write-Host ""
Write-Host "=== TIKI TETRIS Deploy ===" -ForegroundColor Cyan
Write-Host ""

# Step 1: Build if needed
if (-not $NoBuild) {
    & "$ProjectRoot\build.ps1"
    if ($LASTEXITCODE -ne 0) { exit 1 }
}

# Verify .COM exists
if (-not (Test-Path $comFile)) {
    Write-Host "ERROR: $comFile not found. Run build first." -ForegroundColor Red
    exit 1
}

$comBytes = [System.IO.File]::ReadAllBytes($comFile)
Write-Host "Deploying: $comName.COM ($($comBytes.Length) bytes)" -ForegroundColor White

# Step 2: Copy base disk to work disk and add .COM file
if (-not (Test-Path $BaseDisk)) {
    Write-Host "ERROR: Base disk not found: $BaseDisk" -ForegroundColor Red
    exit 1
}
Write-Host "Using base disk: $BaseDisk" -ForegroundColor DarkGray
$disk = [System.IO.File]::ReadAllBytes($BaseDisk)

# === Scan existing directory to find used blocks and any old TIKIPAC entry ===
$TOTAL_BLOCKS = ($IMAGE_SIZE - $DATA_OFFSET) / $BLOCK_SIZE
$usedBlocks = New-Object bool[] $TOTAL_BLOCKS
$oldSlot = -1

for ($s = 0; $s -lt $DIR_ENTRIES; $s++) {
    $off = $DATA_OFFSET + ($s * $DIR_ENTRY_SIZE)
    $user = $disk[$off]
    if ($user -eq $CPM_EMPTY) { continue }  # empty slot

    # Check if this is an existing TIKIPAC.COM entry
    $fnMatch = $true
    $fnPadCheck = $comName.ToUpper().PadRight(8).Substring(0, 8)
    $exPadCheck = "COM".PadRight(3).Substring(0, 3)
    for ($i = 0; $i -lt 8; $i++) {
        if ($disk[$off + 1 + $i] -ne [byte][char]$fnPadCheck[$i]) { $fnMatch = $false; break }
    }
    if ($fnMatch) {
        for ($i = 0; $i -lt 3; $i++) {
            if ($disk[$off + 9 + $i] -ne [byte][char]$exPadCheck[$i]) { $fnMatch = $false; break }
        }
    }

    if ($fnMatch) {
        # Found old TIKIPAC.COM — remember slot, free its blocks
        $oldSlot = $s
        Write-Host "  Replacing existing $comName.COM in slot $s" -ForegroundColor DarkGray
    } else {
        # Mark blocks used by other files
        for ($i = 0; $i -lt 16; $i++) {
            $blk = $disk[$off + 16 + $i]
            if ($blk -gt 0 -and $blk -lt $TOTAL_BLOCKS) {
                $usedBlocks[$blk] = $true
            }
        }
    }
}

# Find a free directory slot (reuse old TIKIPAC slot or find empty)
if ($oldSlot -ge 0) {
    $dirSlot = $oldSlot
} else {
    $dirSlot = -1
    for ($s = 0; $s -lt $DIR_ENTRIES; $s++) {
        $off = $DATA_OFFSET + ($s * $DIR_ENTRY_SIZE)
        if ($disk[$off] -eq $CPM_EMPTY) { $dirSlot = $s; break }
    }
    if ($dirSlot -lt 0) {
        Write-Host "ERROR: No free directory slot on disk" -ForegroundColor Red
        exit 1
    }
}

# Find free blocks for our file
$fileSize     = $comBytes.Length
$blocksNeeded = [Math]::Max(1, [Math]::Ceiling($fileSize / $BLOCK_SIZE))
$totalRecords = [Math]::Max(1, [Math]::Ceiling($fileSize / $CPM_RECORD_SIZE))

if ($blocksNeeded -gt 16) {
    Write-Host "ERROR: File too large ($blocksNeeded blocks > 16 max)" -ForegroundColor Red
    exit 1
}

$freeBlocks = @()
for ($b = $FIRST_DATA_BLOCK; $b -lt $TOTAL_BLOCKS; $b++) {
    if (-not $usedBlocks[$b]) { $freeBlocks += $b }
    if ($freeBlocks.Count -ge $blocksNeeded) { break }
}
if ($freeBlocks.Count -lt $blocksNeeded) {
    Write-Host "ERROR: Not enough free blocks ($($freeBlocks.Count) free, need $blocksNeeded)" -ForegroundColor Red
    exit 1
}

# Write directory entry
$fnPad   = $comName.ToUpper().PadRight(8).Substring(0, 8)
$exPad   = "COM".PadRight(3).Substring(0, 3)
$fnBytes = [System.Text.Encoding]::ASCII.GetBytes($fnPad)
$exBytes = [System.Text.Encoding]::ASCII.GetBytes($exPad)

$off = $DATA_OFFSET + ($dirSlot * $DIR_ENTRY_SIZE)
$disk[$off] = 0  # user 0
for ($i = 0; $i -lt 8; $i++) { $disk[$off + 1 + $i] = $fnBytes[$i] }
for ($i = 0; $i -lt 3; $i++) { $disk[$off + 9 + $i] = $exBytes[$i] }
$disk[$off + 12] = 0                        # extent number
$disk[$off + 13] = 0                        # S1
$disk[$off + 14] = 0                        # S2
$disk[$off + 15] = [byte]$totalRecords      # RC

for ($i = 0; $i -lt 16; $i++) {
    if ($i -lt $blocksNeeded) {
        $disk[$off + 16 + $i] = [byte]$freeBlocks[$i]
    } else {
        $disk[$off + 16 + $i] = 0
    }
}

# Write file data into assigned blocks
for ($b = 0; $b -lt $blocksNeeded; $b++) {
    $blkOff = $DATA_OFFSET + ($freeBlocks[$b] * $BLOCK_SIZE)
    $srcOff = $b * $BLOCK_SIZE
    $count  = [Math]::Min($BLOCK_SIZE, $fileSize - $srcOff)
    if ($count -gt 0) {
        [Array]::Copy($comBytes, $srcOff, $disk, $blkOff, $count)
    }
    for ($i = $count; $i -lt $BLOCK_SIZE; $i++) {
        $disk[$blkOff + $i] = 0x1A
    }
}

Write-Host "  Added: $fnPad.$exPad  ${fileSize}B  ${blocksNeeded}blk  RC=$totalRecords" -ForegroundColor Green

# Save disk image
[System.IO.File]::WriteAllBytes($WorkDisk, $disk)
Write-Host ""
Write-Host "Saved: $WorkDisk" -ForegroundColor Green

# Step 3: Launch emulator
if (-not $NoLaunch) {
    if (-not (Test-Path $EmuExe)) {
        Write-Host "WARNING: Emulator not found at $EmuExe" -ForegroundColor Yellow
        Write-Host "You can launch it manually with: $EmuExe -diskb `"$WorkDisk`"" -ForegroundColor Yellow
    } else {
        Write-Host ""
        Write-Host "Starting emulator..." -ForegroundColor Cyan
        Write-Host "  A: $WorkDisk" -ForegroundColor DarkGray
        Write-Host ""
        Write-Host "  After boot: $comName" -ForegroundColor Yellow
        Write-Host ""
        
        Start-Process $EmuExe -ArgumentList "-diska `"$WorkDisk`""
    }
}

Write-Host "Done!" -ForegroundColor Green
