import threading
import time
from typing import Optional

import matplotlib.pyplot as plt
import numpy as np
from scipy.interpolate import make_interp_spline

from threadedserial import ThreadedSerial

ARDUINO_PORT = "COM9"
PLOT_LENGTH = 2000
INTERVAL = 10


# kd = 40
# kd = 20 ki = 850

def is_number(s):
    try:
        float(s)
        return True
    except ValueError:
        return False


class TestVitesse:
    def __init__(self, arduino: ThreadedSerial, kp, ki, kd, consigne):
        self.arduino = arduino
        self.kp = kp
        self.ki = ki
        self.kd = kd
        self.consigne = consigne

        self.time_data = [0]
        self.speed_data = [0]

        self.event = threading.Event()

    def run(self):
        if self.event.is_set():
            print("Test has already been run")
            return

        start_time = time.time()
        self.arduino.send("PR+kp=" + str(self.kp))
        self.arduino.send("PR+ki=" + str(self.ki))
        self.arduino.send("PR+kd=" + str(self.kd))
        self.arduino.send("PR+asservissement_position=0")

        self.arduino.send("PR+log=1")
        self.arduino.send("PR+consigne=" + str(self.consigne))

        self.event.wait()

        end_time = time.time()
        time.sleep(0.1)
        print("Test took " + str(end_time - start_time) + " seconds")

    def __str__(self):
        return f"TestVitesse(kp={self.kp}, ki={self.ki}, kd={self.kd}, consigne={self.consigne})"

    def add_line(self, speed):
        if len(self.time_data) > PLOT_LENGTH / INTERVAL:
            return

        self.time_data.append(len(self.time_data) * INTERVAL)
        self.speed_data.append(speed)

        if len(self.time_data) > PLOT_LENGTH / INTERVAL:
            self.event.set()
            self.arduino.send("PR+log=0")
            self.arduino.send("PR+reset")
            self.arduino.serial.flushInput()


class TestPosition:
    def __init__(self, arduino: ThreadedSerial, kp, ki, kd, consigne):
        self.arduino = arduino
        self.kp = kp
        self.ki = ki
        self.kd = kd
        self.consigne = consigne

        self.time_data = [0]
        self.speed_data = [0]

        self.event = threading.Event()

    def run(self):
        if self.event.is_set():
            print("Test has already been run")
            return

        start_time = time.time()
        self.arduino.send("PR+kp=" + str(self.kp))
        self.arduino.send("PR+ki=" + str(self.ki))
        self.arduino.send("PR+kd=" + str(self.kd))

        self.arduino.send("PR+asservissement_position=1")

        self.arduino.send("PR+log=1")
        self.arduino.send("PR+consigne=" + str(self.consigne))

        self.event.wait()

        end_time = time.time()
        time.sleep(0.1)
        print("Test took " + str(end_time - start_time) + " seconds")

    def __str__(self):
        return f"TestPosition(kp={self.kp}, ki={self.ki}, kd={self.kd}, consigne={self.consigne})"

    def add_line(self, speed):

        self.time_data.append(len(self.time_data) * INTERVAL)
        self.speed_data.append(speed)

        if len(self.time_data) > 10000 or (len(self.speed_data) > 10 and sum(self.speed_data[-10:-1]) == 0):
            self.event.set()
            self.arduino.send("PR+log=0")
            self.arduino.send("PR+reset")
            self.arduino.serial.flushInput()


class ProjetRecherche:
    def __init__(self):
        self.active_test: Optional[TestVitesse] = None
        self.all_tests = []

        self.kp = 0.
        self.ki = 0.
        self.kd = 0.
        self.consigne = 20.
        self.type_str = "v"

        arduino = ThreadedSerial(ARDUINO_PORT, 115200, self.on_message, False, "PR+")

        while arduino.serial.is_open:
            command = input("Enter a command: ")
            if command == "test":
                kp_str = input(f"Enter kp (default {self.kp}): ") or str(self.kp)
                ki_str = input(f"Enter ki (default {self.ki}): ") or str(self.ki)
                kd_str = input(f"Enter kd (default {self.kd}): ") or str(self.kd)
                consigne_str = input(f"Enter consigne (default {self.consigne}): ") or str(self.consigne)
                self.type_str = input(f"Enter type asservissement (v/p) (default {self.type_str}): ") or str(self.type_str)

                if not is_number(kp_str) or not is_number(ki_str) or not is_number(kd_str) or not is_number(consigne_str):
                    print("Invalid input")
                    continue
                self.kp = float(kp_str)
                self.ki = float(ki_str)
                self.kd = float(kd_str)
                self.consigne = float(consigne_str)

                if self.type_str == "v":
                    self.active_test = TestVitesse(arduino, self.kp, self.ki, self.kd, self.consigne)
                elif self.type_str == "p":
                    self.active_test = TestPosition(arduino, self.kp, self.ki, self.kd, self.consigne)
                else:
                    print("Invalid type")
                    continue

                self.all_tests.append(self.active_test)

                threading.Thread(self.active_test.run()).start()
            elif command == "show":
                plt.clf()
                for test in self.all_tests:
                    x = np.array(test.time_data)
                    y = np.array(test.speed_data)
                    X_Y_Spline = make_interp_spline(x, y)
                    X_ = np.linspace(x.min(), x.max(), 5000)
                    Y_ = X_Y_Spline(X_)
                    plt.plot(X_, Y_, label=f"kp={test.kp}, ki={test.ki}, kd={test.kd}, consigne={test.consigne}")
                plt.xlabel('Time')
                plt.ylabel('Speed')
                plt.title('Speed vs. Time')
                plt.legend()
                plt.grid(True)
                plt.ylim([0, ((self.consigne + 0.3 * self.consigne) if self.type_str == "v" else 100)])
                plt.show(block=True)
            elif command == "list":
                for i, test in enumerate(self.all_tests):
                    print(i, test)
            elif command == "remove":
                index = input("Enter index of test to remove: ")
                if not is_number(index):
                    print("Invalid input")
                    continue
                index = int(index)
                if index < 0 or index >= len(self.all_tests):
                    print("Invalid index")
                    continue
                del self.all_tests[index]
            elif command == "clear":
                self.all_tests.clear()
            elif command == "exit":
                arduino.serial.close()
                break
            else:
                print("Invalid command")

    def on_message(self, data):
        if data.startswith("speed="):
            if self.active_test is not None:
                self.active_test.add_line(float(data[6:]))
        else:
            print(data)


if __name__ == '__main__':
    ProjetRecherche()
