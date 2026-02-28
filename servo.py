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

    
com = int(input("Select port number: "))


serialInst.baudrate = 9600
serialInst.port = portsList[com]  # use the clean device path directly
serialInst.open()

while True:
    command = input("Arduino Command (0-180): ")
    print(command)
    # Sanitize user input
    input_position = max(1, min(180, int(command)))
    message = f"{input_position}\n"
    print(f"Sending: {message.strip()}")
    serialInst.write(message.encode('utf-8'))
    if command == 'exit':
        breakpoint