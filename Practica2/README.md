# PRACTICA 2: Simulacion de una puerta de garaje

La practica consiste en simular el funcionamiento de una puerta de garaje. Para ello se utiliza, ademas del propio micro, una placa con displays y leds (TM1638).

Para poder modelar el funcionamiento de la puerta se hace uso de una maquina de estados. La puerta en este caso recorre 300cm, dando pasos de 3cm cada 100ms.

### Componentes utilizados
- El boton azul integrado en la placa (B1) -> actua como mando a distancia.
- El boton S1 del TM1638, que funciona como fotocelula/sensor de proximidad.

En el display se va mostrando los diferentes mensajes que informan del estado de la puerta, asi como la distancia que le falta por recorrer para abrirse/cerrarse por completo. Ademas los 8 leds de la placa actúan como una barra de progreso, simulando el propio movimiento de la puerta.

### Descripcion de estados
CERRADO Estado inicial.
- Visualización: Muestra "cErrAdA" en pantalla; LEDs apagados.
- Transición: Al pulsar B1 pasa a ABRIENDO.

ABRIENDO Incrementa la posición en 3cm cada 100ms.
- Visualización: Muestra la posición actual en cm.
- Transiciones:Al alcanzar 300cm pasa a ABIERTO.
               Al pulsar B1 pasa a PARADO_ABRIENDO.

ABIERTO La posición se mantiene en 300cm.
- Visualización: Muestra "AbiErtA" en pantalla; los 8 LEDs encendidos.
- Transición: Al pulsar B1 pasa a CERRANDO.

CERRANDO Decrementa la posición en 3cm cada 100ms.
- Visualización: Muestra la posición actual en cm.
- Transiciones:Al alcanzar 0cm pasa a CERRADO.
               Al detectar obstáculo (Fotocélula S1) pasa a PARADO_CERRANDO.
               Al pulsar B1 pasa a PARADO_CERRANDO.

PARADO_ABRIENDO El movimiento de apertura se interrumpe manualmente.
- Visualización: Muestra "PArAdA".
- Transición: Al pulsar B1 invierte el sentido y pasa a CERRANDO.

PARADO_CERRANDO El movimiento de cierre se detiene por seguridad (fotocélula) o por orden manual.
- Visualización: Muestra "PArAdA".
- Transiciones:Si fue detenido por la fotocélula S1, permanece bloqueado hasta que el obstáculo sea retirado. Al pulsar B1 (sin obstáculo) pasa a ABRIENDO.

### Cosas a tener en cuenta
En el codigo se utiliza el siguiente fragmento de codigo en la funcion detectarFlanco:

```c
    bool detectarFlanco(){
	    static bool estable=false;
	    static bool ultimo=false;
	    static uint32_t tCambio=0;
    	bool crudo=(HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin)==GPIO_PIN_RESET);
	    if(crudo!=ultimo){
		    ultimo=crudo;
		    tCambio=HAL_GetTick();
	    }
	    if((HAL_GetTick()-tCambio)>=ANTIRREBOTE_MS && crudo!=estable){
		    estable=crudo;
		    return estable;
	    }
	    return false;
    }
```
De forma resumida esta funcion lo que esta haciendo es ver si el boton B1 esta pulsado o no (realmente lo que mira es el estado del pin en el que esta conectado el boton b1 en el micro).

De forma extensa, la funcion actúa como debouncer (codigo anti-rebotes). Cuando pulsamos un botón mecánico, sus contactos físicos no hacen una conexión perfecta de inmediato, sino que chocan y rebotan durante unos milisegundos. Esto genera falsas oscilaciones eléctricas de la señal entre '0' y '1' lógico. Si el microcontrolador leyese el pin directamente en ese instante, podría interpretar un solo toque como múltiples pulsaciones consecutivas. 

Para evitarnos ese problema, lo que hacemos es esperar un tiempo reducido para darle tiempo a la señal a estabilizarse, y si el estado del boton despues de ese tiempo es igual al que tenia antes no hace nada y devuelve false, si por el contrario es diferente se devuelve true y se actualiza estable (que guarda el estado anterior).