# FC-067 developer packet inspection, not GPU/native replay proof.
param([Parameter(Mandatory=$true)][string]$CaptureDirectory)
$ErrorActionPreference = 'Stop'
$packets = @(Get-ChildItem -LiteralPath $CaptureDirectory -Directory |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'manifest.json') })
if ($packets.Count -eq 0) { throw 'No captured frames' }
foreach ($directory in $packets) {
    $path = Join-Path $directory.FullName 'pvr-scene.json'
    $file = Get-Item -LiteralPath $path
    if ($file.Length -gt 32MB) { throw 'Packet byte limit exceeded' }
    $packet = Get-Content -Raw -LiteralPath $path | ConvertFrom-Json
    $manifest = Get-Content -Raw -LiteralPath (Join-Path $directory.FullName 'manifest.json') | ConvertFrom-Json
    if ($packet.schema -ne 'flycast-pvr-scene-v1' -or $packet.coordinate_space -ne 'pvr-projected' -or
        $packet.camera_provenance -ne 'unknown' -or $packet.world_transform_provenance -ne 'unknown') {
        throw 'Unexpected schema or upgraded coordinate provenance'
    }
    if ($packet.frame_id -ne $manifest.frame_id -or $packet.game_id -ne $manifest.game_id) {
        throw 'Packet/manifest identity mismatch'
    }
    if ($packet.vertices.Count -gt 65536 -or $packet.indices.Count -gt 262144 -or
        $packet.draws.Count -gt 8192 -or $packet.passes.Count -gt 10 -or $packet.viewport_bits.Count -ne 16) {
        throw 'Packet aggregate limit exceeded'
    }
    $nonfinite = [System.Collections.Generic.HashSet[uint32]]::new()
    for ($i=0; $i -lt $packet.vertices.Count; ++$i) {
        $vertex=$packet.vertices[$i]
        if ($vertex.Count -ne 11) { throw 'Invalid vertex layout' }
        foreach ($axis in 0..2) {
            if (([uint32]$vertex[$axis] -band 2139095040) -eq 2139095040) { [void]$nonfinite.Add([uint32]$i) }
        }
    }
    $badReferences=0; $restarts=0
    foreach ($index in $packet.indices) {
        if ([uint64]$index -eq 4294967295) { ++$restarts; continue }
        if ([uint64]$index -ge $packet.vertices.Count) { throw 'Out-of-range index' }
        if ($nonfinite.Contains([uint32]$index)) { ++$badReferences }
    }
    foreach ($draw in $packet.draws) {
        if ([uint64]$draw.first + [uint64]$draw.count -gt $packet.indices.Count) { throw 'Out-of-range draw' }
    }
    if ($nonfinite.Count -ne $packet.nonfinite_position_count -or
        $badReferences -ne $packet.nonfinite_index_reference_count) { throw 'Nonfinite accounting mismatch' }
    if ($badReferences -ne 0) { throw 'Referenced nonfinite geometry: not eligible for replay validation' }
    [pscustomobject]@{
        Frame=$packet.frame_id; Vertices=$packet.vertices.Count; Indices=$packet.indices.Count
        Draws=$packet.draws.Count; Restarts=$restarts; UnusedNonfinite=$nonfinite.Count
        Camera=$packet.camera_provenance; NativeReplayProven=$false
        SHA256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    }
}
