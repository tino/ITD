import os
import time
import threading
import Queue

import serial
from pyfirmata import util


def get_first_port():
    for resource in os.listdir('/dev'):
        if resource.startswith('tty.usb'):
            return '/dev/%s' % resource

connection = serial.Serial(get_first_port(), 57600)


def send_cmd(to, operation, operand1, operand2):
    operand2_str = "%s%s" % util.to_two_bytes(operand2)
    string = 'AF%sZ%s%s%sFA' % (to, operation, operand1, operand2_str)
    print "sending %s" % string
    return connection.write(string)


def read():
    while True:
        try:
            print queue.get_nowait()
        except Queue.Empty:
            return


class SerialFlusher(threading.Thread):
    def __init__(self, connection, queue):
        super(SerialFlusher, self).__init__()
        self.running = True
        self.connection = connection
        self.queue = queue

    def run(self):
        while self.running:
            try:
                while self.connection.inWaiting():
                    self.queue.put(self.connection.readline())
                time.sleep(0.001)
            except (AttributeError, serial.SerialException, OSError), e:
                # this way we can kill the thread by setting the connection object
                # to None, or when the serial port is closed by connection.exit()
                self.queue.put(e)
                break
            except Exception, e:
                # catch 'error: Bad file descriptor'
                # iterate may be called while the serial port is being closed,
                # causing an "error: (9, 'Bad file descriptor')"
                if getattr(e, 'errno', None) == 9:
                    break
                try:
                    if e[0] == 9:
                        break
                except (TypeError, IndexError):
                    pass
                self.queue.put(e)
                raise

    def quit(self):
        self.running = False


queue = Queue.Queue()

sf = SerialFlusher(connection, queue)
sf.start()
