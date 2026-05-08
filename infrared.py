import machine
import time

# Setup Pin
ir = machine.Pin(14, machine.Pin.IN)   # Sensor IR di pin 14


while True:
    # Baca IR
    ir_nilai = ir.value()
    if ir_nilai == 0:
        status_ir = "Objek terdeteksi"
    else:
        status_ir = "Kosong"


    # Tampilkan hasil
    print("IR:", status_ir)

    time.sleep(2)
