"""End-to-end CLI and terminal interaction tests; standard library + FFmpeg."""
import json
import os
from pathlib import Path
import pty
import select
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
EXE = str(ROOT / 'pikupiku')

class RendererTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='line-boil-tests-')
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.source = self.folder / 'input with spaces.ppm'
        pixels = bytearray()
        for y in range(64):
            for x in range(64):
                pixels.extend((40, 90, 190) if (x-32)**2+(y-32)**2 < 400 else (245, 235, 210))
        self.source.write_bytes(b'P6\n64 64\n255\n' + pixels)
        self.output = self.folder / 'result.gif'

    def render(self, *options, output=None):
        return subprocess.run([EXE, str(self.source), str(output or self.output), '--width', '64', *options], capture_output=True, timeout=30)

    def info(self, path):
        return json.loads(subprocess.check_output(['ffprobe','-v','error','-show_streams','-of','json',str(path)]))['streams'][0]

    def rgb(self, path):
        return subprocess.check_output(['ffmpeg','-v','error','-i',str(path),'-f','rawvideo','-pix_fmt','rgb24','-'])

    def test_animation_and_determinism(self):
        self.assertEqual(self.render('--frames','5','--seed','9').returncode, 0)
        self.assertEqual(int(self.info(self.output)['nb_frames']), 5)
        data = self.rgb(self.output); size = 64*64*3
        self.assertNotEqual(data[:size], data[size:2*size])
        other = self.folder / 'second.gif'
        self.assertEqual(self.render('--frames','5','--seed','9',output=other).returncode, 0)
        self.assertEqual(self.output.read_bytes(), other.read_bytes())

    def test_zero_and_monochrome(self):
        self.assertEqual(self.render('--strength','0').returncode, 0)
        data=self.rgb(self.output); size=64*64*3
        self.assertTrue(all(data[i:i+size]==data[:size] for i in range(0,len(data),size)))
        self.assertEqual(self.render('--saturation','0','--overwrite').returncode, 0)
        data=self.rgb(self.output)
        self.assertTrue(all(data[i]==data[i+1]==data[i+2] for i in range(0,len(data),3)))

    def test_controls_and_pingpong(self):
        result=self.render('--frames','5','--pingpong','1','--threshold','20','--noise-scale','7','--pressure','.8','--contrast','1.2','--brightness','10','--smoothing','0','--palette','32','--dither','1','--ink','.7','--grain','2','--simplify','.1')
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(int(self.info(self.output)['nb_frames']),8)

    def test_bad_options_and_protected_outputs(self):
        for options in [('--fps','1.5'),('--strength','nan'),('--seed','-1'),('--palette','3'),('--pressure','2'),('--unknown','1'),('--ink',)]:
            self.assertNotEqual(self.render(*options).returncode,0)
        original=self.source.read_bytes()
        self.assertNotEqual(self.render('--overwrite',output=self.source).returncode,0)
        self.assertEqual(original,self.source.read_bytes())
        self.output.write_bytes(b'keep me')
        self.assertNotEqual(self.render().returncode,0)
        self.assertEqual(self.output.read_bytes(),b'keep me')
        self.source.write_bytes(b'invalid image')
        self.assertNotEqual(self.render('--overwrite').returncode,0)
        self.assertEqual(self.output.read_bytes(),b'keep me')

    def test_terminal_render(self):
        master, slave=pty.openpty()
        process=subprocess.Popen([EXE],stdin=slave,stdout=slave,stderr=slave,cwd=self.folder)
        os.close(slave)
        self.addCleanup(lambda: process.poll() is None and process.kill())
        self.addCleanup(os.close,master)
        def expect(text):
            data=b''; deadline=time.monotonic()+30
            while text.encode() not in data:
                if time.monotonic()>deadline: self.fail(f'TUI did not show {text}: {data[-1000:]}')
                if select.select([master],[],[],.2)[0]: data+=os.read(master,65536)
        def send(text): os.write(master,(text+'\n').encode())
        expect('Choose:'); send('2'); expect('Image path:'); send(str(self.source))
        expect('Choose:'); send('4'); expect('Number to edit'); send('4'); expect('New value'); send('5')
        expect('Number to edit'); send('b'); expect('Choose:'); send('6')
        expect('Render complete.'); send(''); expect('Choose:'); send('q')
        self.assertEqual(process.wait(timeout=5),0)
        self.assertEqual(int(self.info(self.folder/'animation.gif')['nb_frames']),5)

if __name__=='__main__': unittest.main()
