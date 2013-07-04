import sys
import time
import re
from collections import OrderedDict
import Queue
import traceback
import functools
import logging

import serial

import pygame
from pygame.locals import *

from tools import get_first_port, SerialFlusher, send_cmd_serial
import tools

logging.basicConfig(filename='social2.log',level=logging.DEBUG)
log = logging.getLogger('social2')

pygame.init()

WIDTH = 800
HEIGHT = 600
window = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("Social 2")
screen = pygame.display.get_surface()
clock = pygame.time.Clock()

monitor_re = re.compile('<<(.*)>>')

COLORS = {
    'RED': pygame.Color(255, 0, 0),
    'GREEN': pygame.Color(0, 255, 0),
    'BLUE': pygame.Color(0, 0, 255),
    'WHITE': pygame.Color(255, 255, 255),
    'BLACK': pygame.Color(0, 0, 0),
    0: pygame.Color(255, 0, 0),
    1: pygame.Color(215, 104, 0),
    2: pygame.Color(51, 195, 0)
}

# set up fonts
basic_font = pygame.font.SysFont(None, 40)
button_font = pygame.font.SysFont(None, 30)


class Button(object):

    def __init__(self, parent, rect, text):
        self.parent = parent
        self.surface = pygame.Surface(rect.size)
        self.surface.fill(COLORS['BLUE'])
        self.surface_rect = rect
        self.text = button_font.render(text, True, COLORS['WHITE'])
        self.text_rect = self.text.get_rect()
        self.text_rect.center = rect.center

    def draw(self):
        window.blit(self.surface, self.surface_rect)
        window.blit(self.text, self.text_rect)

    def handle_mouse(self, xy):
        if self.surface_rect.collidepoint(xy):
            self.on_click(self)

    def on_click(self):
        raise NotImplemented


class Braceled(object):
    MISSING = 0
    RECENTLY_SEEN = 1
    PRESENT = 2
    x = 0
    y = 10
    width = 230
    height = 200
    balance = 0
    coins = 0

    def __init__(self, id_, position, send_func):
        self.id = id_
        self.last_seen = 0
        self.status = self.MISSING

        y_pos = position / 3
        x_pos = position % 3
        self.x = x_pos * (self.width + 20) + 10
        self.y = y_pos * (self.height + 20) + 10

        margin = 10
        distance = 30
        spacing = 40

        self.rect = pygame.Rect(self.x, self.y, self.width, self.height)
        self.surface = pygame.Surface(self.rect.size)
        self.surface.fill(COLORS[self.status])

        self.texts = []
        self.buttons = []

        # ID in top center
        text = basic_font.render(self.id, True, COLORS['BLACK'])
        text_rect = text.get_rect()
        text_rect.top = self.rect.top + margin
        text_rect.left = self.rect.left + margin
        self.texts.append((text, text_rect))

        # Balance
        text = basic_font.render("Balance", True, COLORS['BLACK'])
        text_rect = text.get_rect()
        text_rect.center = self.rect.center
        text_rect.top = self.rect.top + distance
        self.texts.append((text, text_rect))

        distance += spacing
        text = basic_font.render(str(self.balance), True, COLORS['BLACK'])
        text_rect = text.get_rect()
        text_rect.center = self.rect.center
        text_rect.top = self.rect.top + 60

        # - button
        button_rect = text_rect.copy().move(-20, 0)
        balance_min = Button(self, button_rect, '-')
        def min_it(s):
            if s.parent.balance < -4:
                return
            send_func(self.id, tools.SET_BALANCE, 0, (s.parent.balance - 1 + 5))
        balance_min.on_click = min_it
        self.buttons.append(balance_min)

        # + button
        button_rect2 = text_rect.copy().move(20, 0)
        balance_plus = Button(self, button_rect2, '+')
        def plus_it(s):
            send_func(self.id, tools.SET_BALANCE, 0, (s.parent.balance + 1 + 5))
        balance_plus.on_click = plus_it
        self.buttons.append(balance_plus)

        # Coincount
        distance += spacing
        text = basic_font.render("Coins", True, COLORS['BLACK'])
        text_rect = text.get_rect()
        text_rect.center = self.rect.center
        text_rect.top = self.rect.top + distance
        self.texts.append((text, text_rect))

        distance += spacing
        text = basic_font.render(str(self.coins), True, COLORS['BLACK'])
        text_rect = text.get_rect()
        text_rect.center = self.rect.center
        text_rect.top = self.rect.top + distance

        # - button
        button_rect = text_rect.copy().move(-20, 0)
        balance_min = Button(self, button_rect, '-')
        def min_it(s):
            if s.parent.coins < 1:
                return
            send_func(self.id, tools.SET_COIN_COUNT, 0, (s.parent.coins - 1))
        balance_min.on_click = min_it
        self.buttons.append(balance_min)

        # + button
        button_rect2 = text_rect.copy().move(20, 0)
        balance_plus = Button(self, button_rect2, '+')
        def plus_it(s):
            send_func(self.id, tools.SET_COIN_COUNT, 0, (s.parent.coins + 1))
        balance_plus.on_click = plus_it
        self.buttons.append(balance_plus)

    def update(self, data=None):
        if data:
            if data[0] == 'alive' or data[0] == 'entrance':
                self.last_seen = time.time()
            elif data[0] == 'balance':
                self.balance = int(data[1])
            elif data[0] == 'coincount':
                self.coins = int(data[1])

        if time.time() - self.last_seen > 10:
            self.status = self.MISSING
        elif time.time() - self.last_seen > 5:
            self.status = self.RECENTLY_SEEN
        else:
            self.status = self.PRESENT

    def draw(self):
        self.surface.fill(COLORS[self.status])
        window.blit(self.surface, self.rect)

        for text in self.texts:
            window.blit(text[0], text[1])

        for button in self.buttons:
            button.draw()

        # updating texts
        text = basic_font.render(str(self.balance), True, COLORS['BLACK'])
        text_rect = text.get_rect()
        text_rect.center = self.rect.center
        text_rect.top = self.rect.top + 60
        window.blit(text, text_rect)

        text = basic_font.render(str(self.coins), True, COLORS['BLACK'])
        text_rect = text.get_rect()
        text_rect.center = self.rect.center
        text_rect.top = self.rect.top + 140
        window.blit(text, text_rect)

    def handle_mouse(self, xy):
        for button in self.buttons:
            button.handle_mouse(xy)




class Main():
    _running = True
    at_counter = None

    def __init__(self):
        # setup serial connection
        self.connection = serial.Serial(get_first_port(), 57600)
        self.queue = Queue.Queue()
        self.sf = SerialFlusher(self.connection, self.queue)
        self.sf.start()

        # send_func = functools.partial(send_cmd_serial, self.connection)
        def send_func(*args):
            print "send_func called with args:", args
            send_cmd_serial(self.connection, *args)

        # draw setup for game button
        text = basic_font.render("Reset for game", True, COLORS['BLACK'])
        text_rect = text.get_rect()
        window_rect = window.get_rect()
        text_rect.center = window_rect.center
        text_rect.top = window_rect.top + 550
        button_rect = text_rect.copy().inflate(text_rect.width + 20, text_rect.height + 20)
        self.reset_button = Button(window, button_rect, "Reset for game")

        def reset(s):
            send_func('B', tools.SET_COIN_COUNT, 0, 4)
            send_func('B', tools.SET_BALANCE, 0, 2 + 5)

            send_func('C', tools.SET_COIN_COUNT, 0, 7)
            send_func('C', tools.SET_BALANCE, 0, -1 + 5)

            send_func('D', tools.SET_COIN_COUNT, 0, 6)
            send_func('D', tools.SET_BALANCE, 0, -2 + 5)

            send_func('G', tools.SET_COIN_COUNT, 0, 3)
            send_func('G', tools.SET_BALANCE, 0, 1 + 5)

            send_func('0', tools.OUTPUT_TEST)
        self.reset_button.on_click = reset

        # at counter unit
        self.at_counter_surface = pygame.Surface((100, 100))
        self.at_counter_surface.fill(COLORS['BLACK'])
        self.at_counter_rect = self.at_counter_surface.get_rect()
        self.at_counter_rect.bottom = HEIGHT
        self.not_at_counter_surface = self.at_counter_surface.copy()
        self.not_at_counter_surface.fill(COLORS['WHITE'])

    def handle_events(self):
        # Quitting
        if pygame.key.get_pressed()[K_ESCAPE]:
            self._running = False

        if pygame.key.get_pressed()[K_q] and pygame.key.get_mods() & KMOD_LMETA:
            self._running = False

        for event in pygame.event.get():
            if event.type == QUIT:
                pygame.quit()
                self._running = False
            if event.type == MOUSEBUTTONUP:
                mouse_pos = pygame.mouse.get_pos()
                for id_, bl in self.devices.items():
                    bl.handle_mouse(mouse_pos)
                self.reset_button.handle_mouse(mouse_pos)

        if pygame.key.get_pressed()[K_p]:
            import pdb; pdb.set_trace()


    def process_serial(self):
        try:
            message = self.queue.get_nowait()
            log.info(message)
        except Queue.Empty:
            return
        regex = monitor_re.search(message)
        if regex:
            data = regex.group(1).split(':')
            try:
                self.devices[data[0]].update(data[1:])
            except KeyError:
                # message for us only
                if data[0] == 'Y':
                    if data[1] == 'present_at_counter':
                        self.show_at_counter(data[2])
                    elif data[1] == 'left_counter':
                        self.clear_counter()

        if self.queue.qsize() > 10:
            print "WARNING - queue size:", self.queue.qsize()

    def show_at_counter(self, id_):
        self.at_counter = id_

    def clear_counter(self):
        self.at_counter = None

    def run(self):
        window.fill(COLORS['WHITE'])

        # send_func = functools.partial(send_cmd_serial, self.connection)
        def send_func(*args):
            print "send_func called with args:", args
            send_cmd_serial(self.connection, *args)

        # draw devices
        self.devices = OrderedDict()
        for id_ in 'ABCDG':
            bl = Braceled(id_, len(self.devices), send_func)
            self.devices[id_] = bl

        self.reset_button.draw()

        while self._running:
            try:
                self.process_serial()
                for id_, device in self.devices.items():
                    device.update()
                    device.draw()

                if self.at_counter:
                    window.blit(self.at_counter_surface, self.at_counter_rect)
                    text = basic_font.render(chr(int(self.at_counter)), True, COLORS['WHITE'])
                    text_rect = text.get_rect()
                    text_rect.center = self.at_counter_rect.center
                    window.blit(text, text_rect)
                else:
                    window.blit(self.not_at_counter_surface, self.at_counter_rect)

                self.handle_events()

                pygame.display.update()
                clock.tick(30)
            except Exception:
                traceback.print_exc()

        self.sf.quit()
        sys.exit()


if __name__ == '__main__':
    main = Main()
    main.run()
