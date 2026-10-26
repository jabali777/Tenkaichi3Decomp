import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import record_attacks as recording


class RecordingIsolation(unittest.TestCase):
    def test_snapshots_cannot_overwrite_original_saves_or_settings(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            game = root / 'Tenkaichi3Decomp'
            card = game / 'saves/card1'
            card.mkdir(parents=True)
            (card / 'save').write_bytes(b'original-save')
            (game / 'bt3_settings.txt').write_text('scale=1\n')
            (root / 'optimization').mkdir()
            (root / 'optimization/libbt3_opengl_fast.so').write_bytes(b'adapter')
            (root / 'optimization/libbt3_resolution.so').write_bytes(b'lowres')
            (root / 'play-fast.sh').write_text('#!/bin/sh\n')
            session = recording.prepare_session(root)
            self.assertEqual((session / 'saves-initial/card1/save').read_bytes(), b'original-save')
            (session / 'saves-test/card1/save').write_bytes(b'changed')
            (session / 'settings-test.txt').write_text('scale=8\n')
            self.assertEqual((card / 'save').read_bytes(), b'original-save')
            self.assertEqual((session / 'saves-initial/card1/save').read_bytes(), b'original-save')
            self.assertEqual((game / 'bt3_settings.txt').read_text(), 'scale=1\n')
            self.assertEqual((session / 'settings-initial.txt').read_text(), 'scale=1\n')
            self.assertEqual(json.loads((session / 'state.json').read_text())['estado'], 'preparado')
            self.assertEqual((session / 'adapter-initial.so').read_bytes(), b'adapter')
            self.assertEqual((session / 'resolution-initial.so').read_bytes(), b'lowres')

    def test_live_input_is_recorded_instead_of_inherited_replay_or_demo(self):
        with patch.dict(os.environ, {'BT3_DEMO': '1', 'BT3_PAD_PLAY': '/fake/replay',
                                    'BT3_SHOT': '1', 'BT3_SKIP_FORCE': '1'}):
            session = Path('/tmp/local-diagnostic-session')
            env = recording.recording_environment(session)
            for key in ('BT3_DEMO', 'BT3_PAD_PLAY', 'BT3_SHOT', 'BT3_SKIP_FORCE'):
                self.assertNotIn(key, env)
            self.assertEqual(env['BT3_PAD_REC'], str(session / 'controls.pad'))
            self.assertEqual(env['BT3_SAVES'], str(session / 'saves-test'))
            self.assertEqual(env['BT3_SETTINGS'], str(session / 'settings-test.txt'))
            self.assertEqual(env['BT3_METER'], '1')
            self.assertEqual(env['BT3_SEED'], '1')


if __name__ == '__main__':
    unittest.main()
