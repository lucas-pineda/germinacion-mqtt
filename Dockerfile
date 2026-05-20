# =========================
# Etapa de compilación
# =========================
FROM debian:bookworm AS builder

# Instalar dependencias necesarias
RUN apt-get update && apt-get install -y \
    git \
    python3 \
    python3-pip \
    python3-venv \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# Instalar PlatformIO
RUN pip3 install --break-system-packages platformio

# Directorio de trabajo
WORKDIR /app

# Copiar todo el proyecto
COPY . .

# Compilar el firmware
# Cambia "esp32dev" por tu environment si es diferente
RUN pio run

# =========================
# Etapa final
# =========================
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --break-system-packages platformio

WORKDIR /app

COPY . .

# Puerto MQTT común
EXPOSE 1883

# Mantener contenedor activo o ejecutar monitor
CMD ["platformio", "device", "monitor"]
