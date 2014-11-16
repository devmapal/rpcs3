import unittest
import fnmatch
import os
import sys
import difflib
from subprocess import call

rpcs3_binary = None
tty_log = None
tests_dir = None

class TestPS3Autotests(unittest.TestCase):
    def test_ps3_autotests(self):
        matches = []
        for root, dirnames, filenames in os.walk(tests_dir):
            elfs = fnmatch.filter(filenames, '*.elf')
            if len(elfs) != 1:
                continue

            expecteds = fnmatch.filter(filenames, '*expected')
            if len(expecteds) != 1:
                continue

            elf_filename = os.path.join(root, elfs[0])
            expected = os.path.join(root, expecteds[0])
            matches.append((elf_filename, expected))

        for elf, expected in matches:
            call([rpcs3_binary, '-t', elf])

            with open(expected, 'U') as a_file:
                fromlines = a_file.readlines()
            with open(tty_log, 'U') as a_file:
                tolines = a_file.readlines()

            diff = difflib.unified_diff(fromlines, tolines)
            diff = ''.join(diff)

            self.assertEqual(diff, '', '\n\nDiff for ' + elf + ':\n' + diff)


if __name__ == '__main__':
    if len(sys.argv) != 4:
        sys.exit("Usage: python3 tests.py 'rpcs3_binary' 'tty_log' 'tests_dir'")

    rpcs3_binary = sys.argv[1]
    tty_log = sys.argv[2]
    tests_dir = sys.argv[3]
    del sys.argv[1:]

    unittest.main()
