"""Exercise launch failures and configuration without starting the actual game."""
import importlib.util
import os
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import record_attacks as recording

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('exportar', ROOT / 'scripts/export.py')
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)


class Launcher(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        shutil.copyfile(ROOT / 'play-fast.sh', self.root / 'play-fast.sh')

    def fake_game(self):
        game = self.root / 'Tenkaichi3Decomp/Tenkaichi3Decomp'
        game.parent.mkdir(parents=True)
        # A harmless executable replaces the port in every launch test.
        game.write_text('#!/bin/sh\nprintf "%s\\n" "$BT3_BAJA_RESOLUCION" '
                        '"$BT3_SOMBREADO_SIMPLE" "$BT3_METER" "$BT3_GS" '
                        '"$BT3_INTERP" "$@"\n')
        game.chmod(0o755)

    def launch(self, **updates):
        env = {key: value for key, value in os.environ.items()
               if not key.startswith('BT3_') and key != 'LD_PRELOAD'}
        env.update(updates)
        return subprocess.run(['sh', str(self.root / 'play-fast.sh'), 'two words'],
                              cwd='/', env=env, capture_output=True, text=True, timeout=5)

    def test_missing_game_has_actionable_error(self):
        result = self.launch()
        self.assertEqual(result.returncode, 1)
        self.assertIn('Missing port executable', result.stderr)

    def test_missing_adapter_does_not_silently_launch_unoptimized(self):
        self.fake_game()
        result = self.launch()
        self.assertEqual(result.returncode, 1)
        self.assertIn('build.sh', result.stderr)
        self.assertEqual(result.stdout, '')

    def test_disabled_adapters_preserve_overrides_arguments_and_working_directory(self):
        self.fake_game()
        result = self.launch(BT3_OPTIMIZACION_GL='0', BT3_BAJA_RESOLUCION='0',
                             BT3_SOMBREADO_SIMPLE='1', BT3_METER='0')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.splitlines(), ['0', '1', '0', 'gpu', '1', 'two words'])

    def test_interpolation_defaults_to_60_fps_and_can_be_disabled(self):
        self.fake_game()
        result = self.launch(BT3_OPTIMIZACION_GL='0')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('gpu\n1\ntwo words', result.stdout)
        result = self.launch(BT3_OPTIMIZACION_GL='0', BT3_INTERP='0')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('gpu\n0\ntwo words', result.stdout)

    def test_missing_lowres_adapter_is_reported(self):
        self.fake_game()
        (self.root / 'optimization').mkdir()
        (self.root / 'optimization/libbt3_opengl_fast.so').touch()
        result = self.launch()
        self.assertEqual(result.returncode, 1)
        self.assertIn('Missing adapters', result.stderr)

    def test_preload_path_with_spaces_is_rejected(self):
        self.root = self.root / 'folder with spaces'
        self.root.mkdir()
        shutil.copyfile(ROOT / 'play-fast.sh', self.root / 'play-fast.sh')
        self.fake_game()
        result = self.launch()
        self.assertEqual(result.returncode, 1)
        self.assertIn('without spaces', result.stderr)
        self.assertEqual(self.launch(BT3_OPTIMIZACION_GL='0').returncode, 0)

    def test_diagnostic_preflight_does_not_create_partial_session(self):
        with patch.object(recording, 'ROOT', self.root):
            with patch('builtins.print'):
                self.assertEqual(recording.main(), 2)
        self.assertFalse((self.root / 'performance-diagnostics').exists())


class SourceExport(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in exporter.FILES:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, path)

    def test_only_allowlisted_sources_are_exported(self):
        private = self.root / 'Tenkaichi3Decomp/gamedata/private.bin'
        private.parent.mkdir(parents=True)
        private.write_bytes(b'not for publication')
        (self.root / '.env').write_text('PRIVATE=value\n')
        archive = exporter.export(self.root)
        with tarfile.open(archive) as package:
            self.assertEqual(set(package.getnames()),
                             {'bt3-low-spec/' + name for name in exporter.FILES})
            self.assertTrue(all(member.isfile() for member in package.getmembers()))
            self.assertEqual(package.getmember('bt3-low-spec/play.sh').mode, 0o755)

    def test_symlink_is_rejected_before_archive_is_created(self):
        path = self.root / 'README.md'
        path.unlink()
        path.symlink_to(ROOT / 'README.md')
        with self.assertRaises(ValueError):
            exporter.export(self.root)
        self.assertFalse((self.root / 'dist').exists())


if __name__ == '__main__':
    unittest.main()
