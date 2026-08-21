# Create a dedicated WSL2 distro for zkaedi-lab (run from Windows PowerShell).
# UNVERIFIED in the build container - validate on your machine.
$Base = "H:\wsl\zkaedi-lab"
New-Item -ItemType Directory -Force -Path $Base | Out-Null
# Import a minimal Ubuntu rootfs you have already downloaded:
wsl --import zkaedi-lab $Base "$Base\ubuntu-24.04-rootfs.tar" --version 2
wsl -d zkaedi-lab -u root -- bash -c "apt-get update && apt-get install -y python3 podman uidmap slirp4netns && useradd -m lab"
Write-Host "Distro ready. Snapshot baseline:"
wsl --export zkaedi-lab "$Base\snapshots\baseline.tar"
