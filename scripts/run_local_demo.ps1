param(
    [string]$source = "sample.mp4",
    [int]$chunks = 4,
    [string]$output = "out.mp4"
)

Write-Host "Building demo..."
cmake --preset dev
cmake --build --preset dev --target single_machine_demo

Write-Host "Running demo..."
.
"build/dev/single_machine_demo/single_machine_demo.exe" $source $chunks $output
