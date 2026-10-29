APLICACIÓN SIGNOS VITALES ESP32 - WI-FI + BLUETOOTH BLE

CONTENIDO
- index.html, manifest.json, sw.js e icons/: archivos de la aplicación web instalable.
- Signos_Vitales_WiFi_Bluetooth.ino: firmware Arduino para ESP32 con Wi-Fi y Bluetooth BLE.

IMPORTANTE: para que Bluetooth funcione debes actualizar el programa del ESP32.
La página sola no puede activar Bluetooth en el firmware anterior.

PASOS
1. Abre Signos_Vitales_WiFi_Bluetooth.ino en Arduino IDE.
2. En las líneas ssid y password escribe el nombre y la contraseña de tu Wi-Fi 2.4 GHz.
3. Selecciona la placa ESP32 Dev Module y carga el programa al ESP32.
4. En GitHub abre tu repositorio signos-vitales-esp32.
5. Reemplaza el index.html y sube también los demás archivos del ZIP, conservando la carpeta icons.
6. Abre la página publicada mediante HTTPS.
7. Para Wi-Fi, escribe la IP que aparece en el Monitor Serie.
8. Para Bluetooth, pulsa Buscar ESP32 por Bluetooth y selecciona SignosVitalesESP32.

COMPATIBILIDAD BLUETOOTH
La conexión de esta página usa Web Bluetooth (BLE). Requiere HTTPS y un navegador compatible,
por ejemplo Chrome o Edge actualizado en Android o computador. No suele funcionar en Safari
de iPhone/iPad ni en navegadores sin Web Bluetooth. Si no aparece el dispositivo, comprueba
que el ESP32 esté encendido y que hayas cargado el nuevo firmware.

UUID BLE
Servicio:        12345678-1234-1234-1234-1234567890ab
Característica:  12345678-1234-1234-1234-1234567890ac
Nombre BLE:      SignosVitalesESP32

NOTA DE SEGURIDAD Y USO
No publiques las credenciales Wi-Fi. Las lecturas son experimentales y el prototipo no es un
instrumento médico ni debe utilizarse para diagnosticar o tomar decisiones de salud.
