# Layer 2 Netcat (`l2nc`)

Implementación minimalista en C de un netcat de **Capa 2 (Enlace de Datos / OSI Layer 2)** utilizando sockets RAW en Linux. Permite transmitir y recibir flujos de datos directamente sobre tramas Ethernet utilizando un EtherType personalizado (`0x88B5`), sin depender de IP ni de protocolos de red superiores.

## Características Técnicas

- **Sockets RAW (`AF_PACKET`):** Captura e inyecta tráfico directamente a nivel de enlace de datos.
- **EtherType Personalizado (`0x88B5`):** Filtra y encapsula exclusivamente las tramas de la aplicación.
- **I/O Multiplexado (`select`):** Manejo concurrente bidireccional entre la entrada estándar (`stdin`/`stdout`) y la interfaz de red sin bloqueos.
- **Broadcast Nativo:** Envío automático de tramas a la dirección MAC de difusión (`FF:FF:FF:FF:FF:FF`).
- **Cero Dependencias:** Escrito en C puro, requiriendo únicamente las cabeceras estándar de Linux.

## Compilación

```bash
./bin/make
```

## Uso

Requiere privilegios de superusuario (`root`) para abrir sockets RAW y manipular interfaces de red:

```bash
sudo ./l2nc <interfaz>

```

### Ejemplo de uso (Chat / Canal de comunicación L2)

En la **Máquina A** (ej. `eth0` o `wlan0`):

```bash
sudo ./l2nc eth0

```

En la **Máquina B** (conectada a la misma red física/enlace):

```bash
sudo ./l2nc eth0

```

Cualquier texto que escribas en la terminal de una máquina y presiones `Enter` se transmitirá por broadcast en una trama Ethernet cruda de Capa 2 y aparecerá reflejado instantáneamente en la otra terminal.

## Estructura del Proyecto

```text
.
├── usr/
│   ├── src/
│   │   └── l2nc.c          # Código fuente principal (Sockets RAW, L2, select)
│   └── bin/
├── bin/                    # Directorio de binarios compilados.
│   └── make                # Scripts de compilación
├── tmp/                    # Archivos temporales
├── README.md
└── .gitignore
```

### Estructura del Código

1. **Apertura de Socket:** Creación del socket `AF_PACKET` filtrando por `ETH_P_ALL` / EtherType `0x88B5`.
2. **Indexación y Hardware:** Uso de `ioctl` (`SIOCGIFINDEX` y `SIOCGIFHWADDR`) para resolver el índice de la interfaz y obtener la dirección MAC origen local.
3. **Bucle de Multiplexación:** Uso de `select()` para escuchar simultáneamente eventos de lectura en `stdin` (para enviar al medio) y en el descriptor del socket (para imprimir en `stdout` los datos recibidos).
