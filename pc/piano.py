import serial
import serial.tools.list_ports
import inquirer
import typing
from typing import Union, List, Dict, Tuple
from serial.tools.list_ports_common import ListPortInfo
import time

def get_ports_list() -> Dict[List[ListPortInfo], List[str]]:
    ports  : List[ListPortInfo]            = serial.tools.list_ports.comports()
    result : Dict[ListPortInfo, List[str]] = {'ports': ports, 'strings': []}
    for port in ports:

        representing_string : str = f"PID: {port.pid}, NAME: {port.name}"
        if port.manufacturer is not None and port.manufacturer != "n/a":
            representing_string += f", MANUFACTURER: {port.manufacturer}"

        if port.description is not None and port.description != "n/a":
            representing_string += f", DESCRIPTION: {port.description}"

        result['strings'].append(representing_string)

    return result

def main():
    ports_info : Dict[List[ListPortInfo], List[str]] = get_ports_list()
    choices : List[Tuple[str, int]] = [(port, i) for i, port in enumerate(ports_info['strings'])]

    questions = [
        inquirer.List(
            "index",
            message="Choose ESP COM-port",
            choices=choices,
        ),
    ]
    answers = inquirer.prompt(questions)
    if answers:
        selected = answers["index"]
        print(f"Your chose: {ports_info['strings'][selected]}")
    else:
        print("Cancel.")
        return

    with serial.Serial(ports_info['ports'][selected].device, 115200, timeout=1) as ser:
        time.sleep(1.5)
        ser.write(b'rgbgbr')
        ser.flush()

if __name__ == "__main__":
    main()
