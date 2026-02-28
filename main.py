import serial
import serial.tools.list_ports

ports = serial.tools.list_ports.comports()
serialInst = serial.Serial()
portsList = []

print("Available Ports:")
print("=========================")
auto_selected_port = None
for i, port in enumerate(ports):
    portsList.append(port.device)  # .device gives you just the path, e.g. /dev/cu.usbmodem...
    print(f"{i}: {port.device}")
    if port.device.startswith("/dev/cu.usbmodem"):
        auto_selected_port = port.device
        com = i
    

if not auto_selected_port:
    com = int(input("Select port number: "))


serialInst.baudrate = 9600
serialInst.port = portsList[com]  # use the clean device path directly
serialInst.open()

while True:
    command = input("Arduino Command (ON/OFF/exit): ")
    serialInst.write(command.encode('utf-8'))
    if command == 'exit':
        breakpoint