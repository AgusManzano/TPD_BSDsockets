# TPD BSD Sockets

## Objetivo

Familiarizarse con la biblioteca BSD Sockets y entender las particularidades de TCP y UDP.
Intercomunicar varios sistemas y adaptarse a protocolos ya existentes.

## Descripción de la Aplicación

La aplicación recibirá mensajes de chats en un formato TCP especial.
Cada conexión TCP recibida pertenece a una instancia de chat entre un cliente y un agente de ventas.
Cada instancia de chat será asignada un ID único.
Analizar cada mensaje (cliente y agente) desde el punto de vista de sentiment analysis (positivo, neutral, negativo).
Utilizar una API de sentiment analysis provista por la cátedra para analizar los mensajes.
Almacenar cada mensaje analizado usando el protocolo syslog en un servidor syslog (por ejemplo, syslog-ng).

## Formato del protocolo TCP

```
<USER><SEP><TIMESTAMP><SEP><MESSAGE_BODY><MSEP>...USER<SEP>TIMESTAMP<SEP>MESSAGE_BODY<MSEP>.
<SEP> es el Byte 0x02.
<MSEP> es el Byte 0x04.
<USER> es el nombre de usuario en ASCII 7 bits (máximo 40 caracteres).
<TIMESTAMP> es la marca de tiempo en formato YYYY-MM-DD HH:mm:ss.
<MESSAGE_BODY> es el mensaje del usuario en ASCII 7 bits (máximo 200 caracteres).
```

## Requerimientos de la Aplicación

- Soportar múltiples conexiones TCP de instancias de chats en simultáneo.
- Asignar un identificador único a cada instancia de chat.
- Realizar un sentiment analysis de cada mensaje.
- Loguear remotamente cada mensaje analizado usando el protocolo syslog.

## API de Sentiment Analysis

- URL Base: http://api.udesa.matsunaga.com.ar:15000
- Autenticación: Header HTTP X-API-Token: <token>
- Endpoint: POST /analyze
- Request Body: { "message": "<mensaje_a_analizar>" }
- Responses: 200 OK con el sentiment, 400 Bad Request si el request body es inválido.

## Formato del mensaje de syslog

```
<PRIVAL> TIMESTAMP HOSTNAME CONVERSATION_ID ROLE MESSAGE WITH SENTIMENT ANALYSIS
Ejemplo: <163> Sep 16 02:20:06 localhost chat-0001 AGENT "Esto es un mensaje" (sentiment: positive sentiment_value: 0.8)
```

## Recomendaciones Generales

- Hacer manejo de errores con logging en pantalla o archivo.
- Testear las funcionalidades/módulos de la aplicación de manera aislada antes de la integración.
- Usar código de ayuda para delimitar y parsear campos dentro de una PDU.

## Entregables

- Código fuente con instrucciones de uso y compilación.
- Presentación Powerpoint grabada en video mostrando la arquitectura y experiencia de programación.
- Adjuntar un archivo .txt que explique detalladamente todo lo necesario para entender el trabajo práctico, incluyendo:
  - Descripción de cada sección del código.
  - Decisiones de diseño.
  - Ejemplos de uso.
  - Instrucciones de compilación y ejecución.

## Instrucciones de Compilación y Ejecución

### test_pdu_candidate.c

Esta función contempla casos de múltiples PDUs en una sola comunicación TCP, algo que no está en el protocolo pero funciona igualmente.

Hay un array donde se pueden hacer pruebas. Notar que el tamaño luego se usa abajo en un loop que simula el loop de lectura.

```c
char *test_buffers[7] = {
  "USER\x02TIMESTAMP\x02MESSAGE_BODY\x04",
  "USER\x02TIMESTAMP\x02MESSAGE_BODY\x04USER\x02TIMESTAMP\x02MESSAGE_BODY\x04",
  "USER\x02TIMESTAMP\x02MESSAGE_BODY\x04USER\x02TIMESTAMP\x02MESSAGE_BODY",
  "USER\x02TIMESTAMP\x02MESSAGE_BODY",
  "\x04USER\x02TIMESTAMP\x02MESSAGE_BODY\x04",
  "USER\x02TIMESTAMP\x02MESSAGE_BODY\x04",
  "USER\x02TIMESTAMP\x02MESSAGE_BODY\x04"
};
```

Este for() emula el loop de lectura:

```c
int i;
for (i=0; i < 7; i++)
```

Se compila así:
```
$ gcc -Wall test_pdu_candidate.c
```
Se ejecuta así:
```
$ ./a.out
```

### test_skip_single.c

Se compila así:
```
$ gcc -Wall test_skip_single.c
```
Se ejecuta así:
```
$ ./a.out "string a delimitar con delimitadores"
```

### unixtimestamp.c

Se compila así:
```
$ gcc -Wall unixtimestamp.c
```
Se ejecuta así:
```
$ ./a.out
```

## Ejemplos de Formato del Protocolo TCP y Salida Esperada

### Ejemplo de Entrada

```
USER\x02TIMESTAMP\x02MESSAGE_BODY\x04
```

### Ejemplo de Salida

```
try_parse PDU
USER\x02TIMESTAMP\x02MESSAGE_BODY\x04
```

### Ejemplo de Entrada con Múltiples PDUs

```
USER\x02TIMESTAMP\x02MESSAGE_BODY\x04USER\x02TIMESTAMP\x02MESSAGE_BODY\x04
```

### Ejemplo de Salida con Múltiples PDUs

```
try_parse PDU
USER\x02TIMESTAMP\x02MESSAGE_BODY\x04
try_parse PDU
USER\x02TIMESTAMP\x02MESSAGE_BODY\x04
```
