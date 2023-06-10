import logging
import threading

import serial


class ThreadedSerial:
    def __init__(self, port, baudrate, on_message, raw, prefix=None):
        self.prefix = prefix
        self.raw = raw
        self.on_message = on_message

        self.serial = serial.Serial(port, baudrate)  # Create our serial

        self.thread = threading.Thread(target=self.read, daemon=True)  # Create the reading thread
        self.thread.start()

    def read(self):
        while self.serial.is_open:
            if self.serial.inWaiting() > 0:
                if not self.raw:
                    data = self.serial.readline().decode("utf-8").strip()
                    if not data.startswith(self.prefix):
                        logging.warning("Received invalid message: " + data)
                        continue
                    data = data[3:]
                else:
                    data = self.serial.read(self.serial.inWaiting())

                self.on_message(data)

    def send(self, message):
        if self.raw:
            self.serial.write(message)
        else:
            self.serial.write(bytes(message + "\n", 'utf-8'))
