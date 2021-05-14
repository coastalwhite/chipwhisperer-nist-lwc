import os
import random

import chipwhisperer

MAX_PAYLOAD_LENGTH = 100

def return_ss_error(code, additional_info = ""):
    additional_info_txt = " ('{}')"
    return_messages = {
            0x01: "An invalid command was used{}.",
            0x02: "A bad checksum was returned{}.",
            0x03: "A timeout occurred{}.",
            0x04: "Received an invalid argument length{}.",
            0x05: "Unexpected frame byte{}.",
            0x10: "Invalid buffer interaction command{}.",
            0x11: "Tried to execute operation without set key{}.",
            0x12: "Tried to execute operation without set input buffer{}.",
            0x13: "A data buffer overflowed{}.",
            0x14: "Invalid Nonce{}.",
            0x15: "Operation returned error{}.",
    }

    return_msg = return_messages.get(code)
    if return_msg is None:
        return_msg = "(ERROR {}) ".format(code) + "Unknown error occurred{}"

    if len(additional_info) == 0:
        return_msg = return_msg.format("")
    else:
        return_msg = return_msg.format(additional_info_txt.format(additional_info))

    print(return_msg)
    quit(1)

class ReadCmd:
    is_filled = False

    def __init__(self, target):
        res = target.read_cmd(timeout=10000)
        if len(res) >= 4:
            self.cmd = res[1]
            if len(res) > 4:
                self.data = res[3:-2]

            self.is_filled = True

    def is_cmd(self, c):
        if not self.is_filled:
            return False

        if not self.cmd == ord(c):
            return False
        
        return True

class LWC_CW:
    """
    The helper class to control the NIST Lightweight Cryptography Wrapper for
    ChipWhisperer SimpleSerial Targets.

    Loads a hex file and programs the target.

    Provides `encrypt`, `decrypt` and `hash` functions, which will return the
    result of that computation along with a power trace captured.

    Attributes
    ----------
    scope : (OpenADC | CWNano | Unknown)
        The ChipWhisperer scope
    target : SimpleSerial2
        The ChipWhisperer SimpleSerial V2 target

    KEY_BYTES : int
        The number of bytes in the key. Depends on algorithm.
    MAX_INPUT_BYTES : int
        The maximum number of plain/cipher text bytes. Set in the C code.
    MAX_AD_BYTES : int
        The maximum number of associated data bytes. Set in the C code.
    NONCE_BYTES : int
        The number of bytes of the nonce. Depends on algorithm.

    Methods
    -------
    encrypt(
        plain: bytearray,
        key: bytearray,
        associated_data : bytearray = bytearray(),
        nonce : bytearray = bytearray()
    ) : (bytearray, numpy.array)
        Encrypts `plain` using the `key` and `nonce` (or automatically
        generated nonce) to `(cipher_text, trace)`.
    
    decrypt(
        cipher: bytearray,
        key: bytearray,
        associated_data : bytearray = bytearray(),
        nonce : bytearray = bytearray()
    ) : (bytearray, numpy.array)
        Decrypts `cipher` using the `key` and `nonce` (or automatically
        generated nonce) to `(plain_text, trace)`.

    hash(plain: bytearray) : (bytearray, numpy.array)
        Hash `plain` to (hash, trace).
    """

    is_connected = False

    def __init__(self, hex_file_path: str, programmer, KEY_BYTES=256,
            MAX_INPUT_BYTES=256, MAX_AD_BYTES=256, NONCE_BYTES=16, is_hex_path_relative = True):
        # Set all C parameters
        self.MAX_INPUT_BYTES = MAX_INPUT_BYTES
        self.MAX_AD_BYTES = MAX_AD_BYTES

        # Set all algorithm parameters
        self.KEY_BYTES = KEY_BYTES
        self.NONCE_BYTES = NONCE_BYTES

        # Connect to scope and target
        self._connect()

        # Flash the source to the CW
        self._flash_source(hex_file_path, programmer, is_hex_path_relative)

    def _connect(self):
        # Check if we are not already connected
        if self.is_connected:
            return

        self._force_connect()

    def _force_connect(self):
        # Connect to the scope
        self.scope = chipwhisperer.scope()

        # Select the default settings for the scope
        self.scope.default_setup()

        # Connect to the scope
        self.target = chipwhisperer.target(self.scope, chipwhisperer.targets.SimpleSerial2, flush_on_err=False)

        self.is_connected = True

    def _flash_source(self, hex_file_path, programmer, is_path_relative):
        # Formulate the path for the hex file
        if is_path_relative:
            firmware_dir = os.path.dirname(os.path.realpath(__file__))
            hex_path = os.path.join(firmware_dir, hex_file_path)
        else:
            hex_path = os.path(hex_file_path)

        # Program the SimpleSerial V2 target
        chipwhisperer.program_target(self.scope, programmer, hex_path)

    def _send_cmd(self, cmd, scmd, buf):
        # Flush the BUS
        self.target.flush()

        # Send the SimpleSerial V2 command
        self.target.send_cmd(cmd, scmd, buf)

    def _read_and_handle_err(self, additional_info = ""):
        result = self.target.simpleserial_wait_ack(timeout=1000)

        if result is None:
            quit("Device did not ack")
        if result[0] != 0:
            return_ss_error(result[0], additional_info)

    def _send_input_cmd(self, scmd, buf):
        self._send_cmd('i', scmd, buf)
        self._read_and_handle_err("Setting Input data buffer")

    def _send_key_cmd(self, scmd, buf):
        self._send_cmd('k', scmd, buf)
        self._read_and_handle_err("Setting key data buffer")

    def _send_ad_cmd(self, scmd, buf):
        self._send_cmd('a', scmd, buf)
        self._read_and_handle_err("Setting associated data buffer")

    def _set_data(self, buf, f, name, max_bytes):
        # Check if the length isn't bigger than the predefined maximum size for
        # the buf.
        if len(buf) > max_bytes:
            quit("As defined, the '{}' data buffer cannot contain more than {} bytes".format(name, max_bytes))

        # Clear the buffer
        f(0x01, bytearray())

        if (len(buf) % MAX_PAYLOAD_LENGTH) == 0:
            extra_round = 0
        else:
            extra_round = 1

        # Append buffer bytes 192 at the time
        for i in range(len(buf) // MAX_PAYLOAD_LENGTH + extra_round):
            f(0x00, bytearray(buf[i*MAX_PAYLOAD_LENGTH:(i+1)*MAX_PAYLOAD_LENGTH]))

    def set_input_data(self, buf):
        self._set_data(buf, self._send_input_cmd, "Input", self.MAX_INPUT_BYTES)

    def set_key_data(self, buf):
        self._set_data(buf, self._send_key_cmd, "Key", self.KEY_BYTES)

    def set_associated_data(self, buf):
        self._set_data(buf, self._send_ad_cmd, "Associated data", self.MAX_AD_BYTES)

    def _crypt_cmd(self, scmd, buf, cmd, name):
        self._send_cmd(cmd, scmd, buf)

        out = []
        while True:
            res = ReadCmd(self.target)

            # if the 't'erminate command was received
            if res.is_cmd('t'):
                # Verify amount of received chunks
                if len(out) % 256 == res.data[0]:
                    quit("Invalid result length")

                break

            # If we received an error
            if res.is_cmd('e'):
                return_ss_error(res.data[0], "While fetching result data")

            if res.is_cmd('r'):
                out.append(res.data)

        # Read ack
        self._read_and_handle_err("Performing {}".format(name))

        return out

    def _send_aead_cmd(self, scmd, buf):
        return self._crypt_cmd(scmd, buf, 'p', 'encryption/decryption')

    def _send_hash_cmd(self):
        return self._crypt_cmd(0x00, bytearray(), 'h', 'hashing')

    def encrypt(self, plain_text, key, associated_data = bytearray(), nonce =
            bytearray()):
        """
        Encrypts `plain_text` using `key` and `nonce`, and returns the
        `cipher_text` along with a power trace.
.

        All arguments are bytearrays.
        """
        self.set_input_data(bytearray(plain_text))
        self.set_key_data(bytearray(key))
        self.set_associated_data(bytearray(associated_data))

        if len(nonce) == 0:
            nonce = bytearray(random.randbytes(self.NONCE_BYTES))

        self.scope.arm()

        cipher_text = self._send_aead_cmd(0x01, nonce) 

        # Do our wave trace capture
        # and fetch that wave trace
        timed_out = self.scope.capture()
        if timed_out:
            quit("Trace capture timed out.")

        trace = self.scope.get_last_trace()

        return (cipher_text, trace)

    def decrypt(self, cipher_text, key, associated_data = bytearray(), nonce =
            bytearray()):
        """
        Decrypts `cipher_text` using `key` and `nonce`, and returns the
        `plain_text` along with a power trace.

        All parameters are bytearrays.
        """
        self.set_input_data(bytearray(cipher_text))
        self.set_key_data(bytearray(key))
        self.set_associated_data(bytearray(associated_data))

        if len(nonce) == 0:
            nonce = bytearray(random.randbytes(self.NONCE_BYTES))

        return self._send_aead_cmd(0x00, nonce) 

    def hash(self, plain_text):
        """
        Hashes `cipher_text` and returns the result hash along with a power
        trace.
        """
        self.set_input_data(bytearray(plain_text))

        self.scope.arm()

        h = self._send_hash_cmd() 

        # Do our wave trace capture
        # and fetch that wave trace
        timed_out = self.scope.capture()
        if timed_out:
            quit("Trace capture timed out.")

        trace = self.scope.get_last_trace()

        return (h, trace)

    def _fetch_status(self):
        self._send_cmd('s', 0x00, bytearray([]))
        res = ReadCmd(self.target)

        if res.is_cmd('r'):
            key_is_done = res.data[0] != 0
            input_min_len = res.data[1] != 0

            self._read_and_handle_err("Status fetch")

            return (key_is_done, input_min_len)

        if res.is_cmd('e'):
            return_ss_error(res.data[0])
        
        quit("Unexpected response received")
