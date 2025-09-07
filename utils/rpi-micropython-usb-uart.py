from machine import UART, Pin
from utime import sleep

uart1 = UART(1, baudrate=115200, tx=Pin(4), rx=Pin(5))

while True:
    message = input("KEYPAD > ")
    uart1.write(message)
