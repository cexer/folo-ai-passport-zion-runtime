"""Bounded USB UI driver. Synthetic keys use the real queue/router, not GPIO."""
import re
import time
import serial


class ZionDevice:
    def __init__(self, port, initial_key=None):
        self.serial = serial.Serial()
        self.serial.port = port
        self.serial.baudrate = 115200
        self.serial.timeout = 0.1
        self.serial.dtr = True
        self.serial.rts = False
        self.serial.open()
        self.frames = []
        self.trace = []
        self.transfers = []
        self.install_frames = []
        self.address = None
        if initial_key is not None:
            assert initial_key in ('UP', 'DOWN', 'OK', 'LONG')
            self.serial.write(('ZION KEY ' + initial_key + '\n').encode())
            self.serial.flush()
        self.pump(0.3)

    def close(self):
        self.serial.close()

    def pump(self, seconds):
        until = time.monotonic() + seconds
        while time.monotonic() < until:
            line = self.serial.readline().decode('utf8', 'replace').strip()
            if 'ZION_UI ' in line:
                line = line[line.index('ZION_UI '):]
                self.frames.append({k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)})
                self.trace.append(line)
            if 'ZION_TRANSFER_UI ' in line:
                frame={k:int(v) for k,v in re.findall(r'(\w+)=(-?\d+)',line)}
                frame['host_time']=time.monotonic();self.transfers.append(frame)
            if 'ZION_INSTALL_UI ' in line:
                frame={k:int(v) for k,v in re.findall(r'(\w+)=(-?\d+)',line)}
                name=re.search(r' name=(.*?) package=',line)
                if name: frame['name']=name[1]
                reason=re.search(r' reason=(\S+)',line)
                if reason: frame['reason']=reason[1]
                self.install_frames.append(frame)
            match = re.search(r'ZION_NET address=([0-9.]+)', line)
            if match:
                self.address = match[1]
                self.trace.append('ZION_NET address=[IP]')
            if 'ZION_ACTION action=' in line:
                self.trace.append(line)
            if any(marker in line for marker in ('ZION_AP auth=', 'ZION WEB_SCAN', 'Z592 SESSION_REARM', 'ZION_PORTAL')):
                self.trace.append(line)
            if 'ZION_FONTS ' in line or 'ZION_FONT_TEST ' in line:
                self.trace.append(line)
            if any(marker in line for marker in ('Z592 BOOT slot=', 'ELF file SHA256:', 'Loaded app from partition', 'ZION_PRODUCT ready', 'ZION_COMMUNITY_QR', 'Z592_RADIO', 'Z592_TEST_', 'Z592_LINK', 'Z592_GATT', 'Z592_TRANSFER', 'Z592_READ_WAIT', 'Z592_TITLE', 'Z592_INSTALL_END', 'Z592_INSTALL_FAILED', 'Z592_CLEANUP', 'Z592_SNAPSHOT', 'Z592 DISCONNECT')):
                self.trace.append(line)

    def command(self, command):
        self.serial.write((command + '\n').encode())
        self.serial.flush()
        self.pump(0.15)

    def key(self, name):
        assert name in ('UP', 'DOWN', 'OK', 'LONG')
        # Never resend a key after a slow Flash operation: only state queries
        # are retried. A repeated OK could otherwise perform a second action.
        self.command('ZION KEY ' + name)

    def state(self):
        count = len(self.frames)
        until = time.monotonic() + 5
        self.command('ZION STATE')
        while time.monotonic() < until:
            self.pump(0.1)
            if len(self.frames) > count:
                return self.frames[-1]
        raise TimeoutError('No Zion UI snapshot within five seconds')

    def wait(self, timeout=35, **expected):
        until = time.monotonic() + timeout
        got = {}
        while time.monotonic() < until:
            try:
                got = self.state()
            except TimeoutError:
                continue
            if all(got.get(k) == v for k, v in expected.items()):
                return got
        raise AssertionError({'expected': expected, 'actual': got})

    def wait_ready(self, timeout=20):
        until = time.monotonic() + timeout
        while time.monotonic() < until:
            if any('ZION_PRODUCT ready' in line for line in self.trace):
                return
            self.pump(.05)
        raise TimeoutError('Product startup marker not observed')

    def home(self):
        for _ in range(4):
            state = self.state()
            assert not state['busy'], 'Wait for the active operation before navigating'
            if state['page'] == 0:
                return state
            self.key('LONG')
        raise AssertionError('Cannot return to list')

    def select_list(self, selected):
        state = self.home()
        assert 0 <= selected < state['apps'] + 2
        for _ in range(state['apps'] + 3):
            if state['selected'] == selected:
                return state
            self.key('DOWN')
            state = self.state()
        raise AssertionError('List selection did not settle')

    def network_address(self):
        self.command('ZION NET')
        until = time.monotonic() + 5
        while not self.address and time.monotonic() < until:
            self.pump(0.2)
        assert self.address and self.address != '0.0.0.0', 'STA has no address'
        return self.address
