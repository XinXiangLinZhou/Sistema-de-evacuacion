# Sistema-de-evacuacion

- Descripción del proyecto

  Este proyecto consiste en un sistema de evacuación diseñado para monitorizar la calidad del aire dentro de un edificio y ayudar en situaciones de emergencia.
  
  El sistema utiliza diferentes sensores ambientales para detectar condiciones peligrosas, como una mala calidad del aire o la presencia de un posible incendio. Cuando se detecta una situación de riesgo, el sistema activa señales visuales mediante luces LED de diferentes colores para alertar a las personas y guiar la evacuación del edificio.
  
  Además, el sistema incorpora un contador de personas utilizando un sensor de distancia, permitiendo registrar cuántas personas abandonan el edificio y comprobar si todavía queda gente en el interior.
  
  La comunicación entre los dispositivos se realiza mediante MQTT y la visualización de datos se implementa con Node-RED, mostrando el estado de los sensores y del sistema en tiempo real.

- Tecnologías utilizadas

    - ESP32
    - ESP32-C3
      - LEDs RGB
    - Node-RED
    - MQTT (Mosquitto)
    - Sensores ambientales:
      - SI7021 (temperatura y humedad)
      - HC-SR04 (sensor diatancia)
        
- Funcionalidades principales
    - Monitorización de temperatura y humedad para la evaluación de la calidad del aire.
    - Sistema visual de alerta mediante LEDs.
    - Conteo de personas durante la evacuación.
    - Visualización en tiempo real desde Node-RED.
    - Detección de errores y desconexión de sensores.
    - Comunicación IoT basada en el protocolo MQTT (modelo publisher/subscriber).
