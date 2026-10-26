"""Export only reviewed source files, never the local game installation."""
import io
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FILES = (
    '.editorconfig',
    'README.md', 'LICENSE', 'NOTICE.md',
    'docs/ARCHITECTURE.md', 'docs/PERFORMANCE.md',
    'play.sh', 'play-fast.sh', 'play-diagnostics.sh',
    'optimization/build.sh', 'optimization/bt3_opengl_fast.c',
    'optimization/bt3_reduced_resolution.c', 'optimization/record_attacks.py',
    'optimization/test_uniforms.c', 'optimization/test_resolution.c',
    'optimization/test_diagnostics.py', 'optimization/test_launcher.py',
    'scripts/check.sh', 'scripts/export.py',
)


def export(root=ROOT):
    # Read and validate everything before creating the archive.
    sources = []
    for name in FILES:
        path = root / name
        if path.resolve() != path.absolute() or not path.is_file():
            raise ValueError(f'Missing file or symbolic link: {name}')
        sources.append((name, path.read_bytes()))
    destination = root / 'dist'
    destination.mkdir(exist_ok=True)
    archive = destination / 'bt3-low-spec-src.tar.gz'
    temporary = archive.with_suffix('.tmp')
    try:
        with tarfile.open(temporary, 'w:gz') as output:
            for name, data in sources:
                info = tarfile.TarInfo('bt3-low-spec/' + name)
                info.size = len(data)
                info.mode = 0o755 if name.endswith('.sh') else 0o644
                output.addfile(info, io.BytesIO(data))
        temporary.replace(archive)
    finally:
        temporary.unlink(missing_ok=True)
    return archive


if __name__ == '__main__':
    result = export()
    print(f'Exported {len(FILES)} source files: {result}')
    print(f'Size: {result.stat().st_size} bytes. Installation and game data excluded.')
