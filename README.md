# Sistema de evacuacion
## 1. Descripción del proyecto

  Este proyecto consiste en un sistema de evacuación diseñado para monitorizar la calidad del aire dentro de un edificio y ayudar en situaciones de emergencia.
  
  El sistema utiliza diferentes sensores ambientales para detectar condiciones peligrosas, como una mala calidad del aire o la presencia de un posible incendio. Cuando se detecta una situación de riesgo, el sistema activa señales visuales mediante luces LED de diferentes colores para alertar a las personas y guiar la evacuación del edificio.
  
  Además, el sistema incorpora un contador de personas utilizando un sensor de distancia, permitiendo registrar cuántas personas abandonan el edificio y comprobar si todavía queda gente en el interior.
  
  La comunicación entre los dispositivos se realiza mediante MQTT y la visualización de datos se implementa con Node-RED, mostrando el estado de los sensores y del sistema en tiempo real.

## 2. Tecnologías utilizadas
    - ESP32
    - ESP32-C3
      - LEDs RGB
    - Node-RED
    - MQTT (Mosquitto)
    - Sensores ambientales:
      - SI7021 (temperatura y humedad)
      - HC-SR04 (sensor diatancia)
        
## 3. Funcionalidades principales
    - Monitorización de temperatura y humedad para la evaluación de la calidad del aire.
    - Sistema visual de alerta mediante LEDs.
    - Conteo de personas durante la evacuación.
    - Visualización en tiempo real desde Node-RED.
    - Detección de errores y desconexión de sensores.
    - Comunicación IoT basada en el protocolo MQTT (modelo publisher/subscriber).

## 4. Instalación del Framework (ESP-IDF)
### 4.1 Clonar el repositorio en un carpeta local
```bash
git clone -b v5.5.3 --recursive https://github.com/espressif/esp-idf.git 
```

### 4.2 Instalar toolchain
```bash
cd ~/esp-idf
./install.sh esp32,esp32c3
```

### 4.3 Cargar las variables y activar el entorno virtual
```bash 
source ~/sed/esp-idf/export.sh
```

## 5. Ejecución del ESP32 para la recepción de datos de sensores
### 5.1 Configurar el Hardware (Target)
```bash
cd ~/EVAC_SYSTEM
idf.py set-target esp32
```
### 5.2 Configuración WiFi y MQTT
  Editar la wifi y ip en main/Kconfig.projbuild ：
  
    - SSID
    - PASSWORD
    - BROKER URL
    
### 5.3 Verificación de configuración
  ```bash
idf.py menuconfig
```
### 5.4 Compilación y ejecución
 ```bash
idf.py -p /dev/ttyUSB0 build flash monitor
```

## 6. Node-RED (Docker)
### 6.1 Instalación de Docker
 ```bash
sudo apt update
sudo apt install docker.io
 ```
### 6.2 Instalación Mosquitto clients
 ```bash
sudo apt update
sudo apt install mosquitto-clients
```
### 6.3 Ejecutar Node-RED
 ```bash
docker run -it -p 1880:1880 -v ./data:/data --name mynodered nodered/node-red 
 ```
### 6.4 Acceder a Node-RED
```bash
http://localhost:1880
```
E importar este fichero json node-red-flows.json al Node-RED
### 6.5 Acceder a dashboard
```bash
http://localhost:1880/dashboard
```
## 7. Mender

###7.1 Installa mender en el proyecto
```bash
mkdir -p external/mender-mcu-client

git clone --branch 0.12.3 --recursive \
https://github.com/joelguittet/mender-mcu-client.git \
external/mender-mcu-client/
```
### 7.2 Verificación de configuración
```bash
idf.py menuconfig
Component config
    → Mender Firmware Update
```

### 7.3 creación de un artefacto para ESP32
```bash
mender-artifact write module-image \
  -T rootfs-image \
  -n release-1 \
  -t esp32 \
  -o release-1.mender \
  -f build/final_c3.bin
```

## 8. Ejecución del ESP32-C3 para detecta alerta

### 8.1 Verificación de configuración
```bash
idf.py menuconfig
Component config
   → Mender Firmware Update
```
### 8.2 creación de un artefacto para ESP32-C3
```bash
idf.py fullclean
idf.py build
idf.py flash monitor
```

### 8.3 Compilación y ejecución
```bash
idf.py -p /dev/ttyAMY0 build flash monitor
```
## Enlace del video
https://drive.google.com/file/d/178jUFE5TLgCmtSkbceOvxDpeCeOab7PC/view?usp=drivesdk
  
