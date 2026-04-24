import random
import time

print("Simulasi DHT11 - Tekan Ctrl+C untuk berhenti\n")

try:
    while True:
        suhu = round(random.uniform(24.0, 35.0), 1)
        kelembapan = round(random.uniform(40.0, 90.0), 1)
        print(f"Suhu: {suhu}°C  |  Kelembapan: {kelembapan}%")
        time.sleep(2)

except KeyboardInterrupt:
    print("\nProgram dihentikan.")