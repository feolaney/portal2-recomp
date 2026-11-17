from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import gameprep


class GamePrepTests(unittest.TestCase):
    def test_game_addresses_stay_stable_and_tools_have_separate_layout(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'bin/linux32').mkdir(parents=True)
            (root / 'portal2_linux').touch()
            names = ['libtier0.so', 'libvstdlib.so', 'client.so', 'server.so', 'engine.so']
            for name in names:
                (root / 'bin/linux32' / name).touch()
            def elf(path, *_):
                start = 0x08048000 if path.name == 'vbsp_linux' else 0
                size = 0x08800000 if start else 0x12345
                return mock.Mock(loads=[(1, 0, start, 0, 0, size)])
            with mock.patch.object(gameprep, 'Elf', side_effect=elf):
                before = gameprep.plan({'linux_root': str(root)})
                authoring = ['puzzlemaker_dll.so', 'vrad_dll.so', 'vvis_dll.so', 'bsppack.so']
                for name in authoring + ['vbsp_linux']:
                    (root / 'bin/linux32' / name).touch()
                self.assertEqual(gameprep.plan({'linux_root': str(root)}), before)
                tool = gameprep.plan({'linux_root': str(root)}, 'vbsp')
            names = {p.name for p, _, _ in tool}
            self.assertTrue({'vrad_dll.so', 'vvis_dll.so', 'bsppack.so', 'vbsp_linux'} <= names)
            self.assertFalse({'client.so', 'server.so', 'portal2_linux', 'puzzlemaker_dll.so'} & names)
            self.assertTrue(all(base >= 0x11000000 for p, base, _ in tool if p.suffix == '.so'))
            self.assertEqual(tool[-1][1], 0)  # ET_EXEC keeps its linked addresses.
            self.assertTrue(all(base + size < 0x30000000 for _, base, size in tool))
