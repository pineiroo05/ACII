/**
  ******************************************************************************
  * @file    TM1638.h
  * @brief   Modulo LED&KEY con controlador TM1638, para STM32 con la HAL.
  ******************************************************************************
  *
  * QUE ES EL TM1638
  * ---------------------------------------------------------------------------
  * Un controlador que gobierna, el solo, 8 displays de siete segmentos con
  * punto, 8 LEDs y 8 pulsadores. Todo eso se maneja desde el microcontrolador
  * con SOLO TRES HILOS, mas alimentacion y masa:
  *
  *     CLK   reloj      lo genera siempre el microcontrolador
  *     DIO   dato       BIDIRECCIONAL: sale al escribir y entra al leer teclas
  *     STB   strobe     delimita cada orden. En reposo esta a nivel alto
  *
  * El microcontrolador no tiene periferico para este protocolo, asi que los
  * tres pines se mueven a mano ("bit-banging") con HAL_GPIO_WritePin. Por eso
  * los tres se configuran como salida en el .ioc, y DIO se cambia a entrada
  * durante el instante en que se leen las teclas.
  *
  * COMO SE USA
  * ---------------------------------------------------------------------------
  * No hay funcion de inicializacion: los puertos y pines se asignan a mano
  * sobre la estructura, y despues se limpia el display. Es lo que hacen los
  * proyectos de la asignatura:
  *
  *     TM1638 TM;
  *     TM.clk_port = CLK_GPIO_Port;  TM.clk_pin = CLK_Pin;
  *     TM.stb_port = STB_GPIO_Port;  TM.stb_pin = STB_Pin;
  *     TM.dio_port = DIO_GPIO_Port;  TM.dio_pin = DIO_Pin;
  *     tm1638_DisplayClear(&TM);
  *     tm1638_TurnOn(&TM, 7);
  *
  * QUE LETRAS SE PUEDEN ESCRIBIR
  * ---------------------------------------------------------------------------
  * Siete segmentos no dan para todo el alfabeto. Solo existen estos 38
  * caracteres, y pedir cualquier otro hace que el display muestre un error:
  *
  *     espacio - _ 0123456789 ABCEFHIJLOPSU abcdhinortuy
  *
  * No hay G, K, M, Q, R, V, W, X ni Z, ni las mayusculas D, N, T, Y. Ademas
  * hay dos parejas que se dibujan igual y no se distinguen a la vista:
  * la 'B' se ve como un '8', y la 'O' se ve como un '0'.
  *
  ******************************************************************************
  */

#ifndef TM1638_H
#define TM1638_H

#include <stdbool.h>
#include "main.h"

/* --------------------------------------------------------------------------
 * Ordenes del TM1638 (hoja de caracteristicas, apartado "Command")
 *
 * El chip distingue tres familias de orden por sus dos bits mas altos:
 *
 *     01xx xxxx   orden de datos:   que se va a hacer, leer o escribir
 *     10xx xxxx   orden de display: encendido y brillo
 *     11xx xxxx   orden de direccion: en que posicion de la RAM se escribe
 * -------------------------------------------------------------------------- */

#define DATA_SET     0x40   /* escribir en el display, con auto-incremento    */
#define DATA_READ    0x42   /* leer los cuatro bytes del teclado              */
#define DISPLAY_ON   0x88   /* 1000 1<000>: encendido; los 3 bits bajos, brillo */
#define DISPLAY_OFF  0x80   /* 1000 0<000>: apagado                           */

/**
 * @brief Un modulo TM1638. Solo guarda a que pines esta conectado.
 *
 * Se rellena a mano antes de la primera llamada. Todas las funciones reciben
 * un puntero a esta estructura, asi que puede haber varios modulos a la vez
 * en pines distintos.
 */
typedef struct
{
    GPIO_TypeDef *clk_port;   /* puerto del reloj                  */
    GPIO_TypeDef *dio_port;   /* puerto del dato                   */
    GPIO_TypeDef *stb_port;   /* puerto del strobe                 */
    uint16_t clk_pin;         /* pin del reloj                     */
    uint16_t dio_pin;         /* pin del dato                      */
    uint16_t stb_pin;         /* pin del strobe                    */
} TM1638;

/* ----------------------------------------------------------- display: texto */

/** Apaga el display y pone a cero sus 16 bytes de RAM. Llamar al arrancar. */
void tm1638_DisplayClear(TM1638 *tm);

/**
 * @brief Enciende el display y fija el brillo.
 * @param brightness 0 a 7. El 0 es el mas tenue, NO es apagado.
 *
 * El brillo queda guardado y se reaplica en cada escritura posterior.
 */
void tm1638_TurnOn(TM1638 *tm, uint8_t brightness);

/**
 * @brief Escribe un texto, alineado a la DERECHA.
 * @param c hasta 8 caracteres de los admitidos, mas hasta 8 puntos decimales.
 *
 * El punto no ocupa posicion: se une al caracter que tiene a su izquierda,
 * asi que "12.34" ocupa cuatro digitos y enciende el punto del segundo.
 */
void tm1638_DisplayTxt(TM1638 *tm, char *c);

/**
 * @brief Escribe un solo caracter.
 * @param position 1 (izquierda) a 8 (derecha).
 * @param dot true para encender ademas su punto decimal.
 */
void tm1638_DisplayChar(TM1638 *tm, int position, char c, bool dot);

/**
 * @brief Enciende segmentos sueltos, sin pasar por la tabla de caracteres.
 * @param position 1 (izquierda) a 8 (derecha).
 * @param data un bit por segmento: 0x01=a (arriba), 0x02=b, 0x04=c, 0x08=d,
 *             0x10=e, 0x20=f, 0x40=g (central) y 0x80=punto decimal.
 *
 * Es la via para dibujar simbolos que no estan en la tabla.
 */
void tm1638_Seg(TM1638 *tm, int position, int data);

/**
 * @brief Traduce un caracter a sus segmentos.
 * @return El patron de segmentos, o -1 si el caracter no se puede dibujar.
 */
int8_t chr_to_hex(char c);

/* ------------------------------------------------------------- display: LED */

/**
 * @brief Enciende o apaga uno de los ocho LEDs rojos.
 * @param position 1 (izquierda) a 8 (derecha).
 * @param on 1 enciende, 0 apaga.
 */
void tm1638_Led(TM1638 *tm, int position, int on);

/* ------------------------------------------------------------------ teclado */

/**
 * @brief Lee los ocho pulsadores de una vez.
 * @return Un bit por tecla: bit 0 = S1 ... bit 7 = S8. Cero si no hay ninguna.
 *
 * Devuelve el NIVEL, no el flanco: mientras se mantenga pulsada, cada llamada
 * seguira devolviendo su bit. Detectar la pulsacion y filtrar los rebotes es
 * trabajo del programa que llama.
 */
uint8_t tm1638_ScanButtons(TM1638 *tm);

/**
 * @brief Comprueba si una tecla concreta esta pulsada.
 * @param buttons Lo que devolvio tm1638_ScanButtons.
 * @param position 1 (S1) a 8 (S8).
 */
bool tm1638_KeyState(TM1638 *tm, uint8_t buttons, int position);

/**
 * @brief Numero de la tecla pulsada mas baja.
 * @param buttons Lo que devolvio tm1638_ScanButtons.
 * @return 1 a 8, o 0 si no hay ninguna pulsada.
 *
 * Con varias teclas a la vez devuelve solo la de numero menor. Para saber
 * todas, mirar directamente los bits de tm1638_ScanButtons.
 */
uint8_t tm1638_ReadKey(TM1638 *tm, uint8_t buttons);

/**
 * @brief Espera dando vueltas hasta que se pulse una tecla y devuelve cual.
 *
 * OJO: BLOQUEA. Mientras espera no se ejecuta nada mas, asi que va en contra
 * de todo lo que se pide en las practicas de P2 en adelante. Esta aqui por
 * compatibilidad; en el bucle principal usar tm1638_ScanButtons.
 */
uint8_t tm1638_ReadKey_Blocking(TM1638 *tm);

#endif /* TM1638_H */
