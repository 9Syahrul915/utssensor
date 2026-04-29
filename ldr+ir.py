import machine
import time

# Setup Pin
ir = machine.Pin(14, machine.Pin.IN)   # Sensor IR di pin 14
ldr = machine.ADC(machine.Pin(34))     # Sensor LDR di pin 34 (ADC)
ldr.atten(machine.ADC.ATTN_11DB)       # Range 0 - 3.3V

print("Sensor IR + LDR Siap...")

while True:
    # Baca IR
    ir_nilai = ir.value()
    if ir_nilai == 0:
        status_ir = "Ada Objek"
    else:
        status_ir = "Kosong"

    # Baca LDR
    ldr_nilai = ldr.read()             # Nilai 0 - 4095
    if ldr_nilai < 1000:
        
        status_ldr = "Gelap"
    elif ldr_nilai < 3000:
        status_ldr = "Redup"
    else:
        status_ldr = "Terang"

    # Tampilkan hasil
    print("IR:", status_ir, "| LDR:", ldr_nilai, "-", status_ldr)

    time.sleep(1)