@echo off
for /r %%f in (*.vert, *.frag) do (
    echo %%f
    glslc -g -O --target-spv=spv1.6 --target-env=vulkan1.3 -I. "%%f" -o "%%f.spv"
)
