/**
  ******************************************************************************
  * @file    TM1638.c
  * @brief   Modulo LED&KEY con controlador TM1638. Implementacion.
  ******************************************************************************
  *
  * EL PROTOCOLO, EN CUATRO IDEAS
  * ---------------------------------------------------------------------------
  * 1. Cada orden va enmarcada por STB: baja al empezar, sube al terminar.
  *
  * 2. Los bits se envian por DIO EMPEZANDO POR EL MENOS SIGNIFICATIVO, y el
  *    chip los captura en el flanco de SUBIDA de CLK. Por eso tm1638_SendData
  *    recorre el byte del bit 0 al 7, y no al reves.
  *
  * 3. El display tiene 16 bytes de RAM, y estan intercalados:
  *
  *       direccion 0, 2, 4 ... 14  ->  segmentos del digito 1, 2, 3 ... 8
  *       direccion 1, 3, 5 ... 15  ->  LED       del digito 1, 2, 3 ... 8
  *
  *    De ahi las dos cuentas que se ven mas abajo: 2*(n-1) para un digito y
  *    2*n-1 para un LED.
  *
  * 4. Para leer las teclas el mismo hilo DIO cambia de sentido: el
  *    microcontrolador lo suelta, lo pasa a entrada, y es el TM1638 quien
  *    escribe en el los cuatro bytes del teclado.
  *
  * SI HAY QUE MODIFICARLO
  * ---------------------------------------------------------------------------
  * Cuidado con las funciones de mas bajo nivel, las que mueven los pines y las
  * que enmarcan cada orden. El protocolo se genera a mano, asi que LA ANCHURA
  * DEL PULSO DE RELOJ ES EL TIEMPO QUE TARDAN EN EJECUTARSE LAS INSTRUCCIONES
  * que hay entre subir y bajar CLK. Quitar trabajo de esos bucles acelera el
  * reloj, y el TM1638 admite 1 MHz como maximo.
  *
  * Dicho de otro modo: ahi, una simplificacion que parece inocente puede
  * romper la comunicacion, y no se ve leyendo el codigo. Hace falta un
  * osciloscopio para comprobarlo.
  *
  ******************************************************************************
  */

#include "TM1638.h"
#include "stm32f4xx_hal.h"
#include <string.h>          /* strlen, en tm1638_DisplayTxt */

/**
 * Brillo actual, de 0 a 7.
 *
 * Hace falta recordarlo porque el TM1638 pierde el ajuste en cuanto se le
 * manda otra orden de display, y cada escritura de digito o de LED termina
 * mandando una. Si no se recordase, tm1638_TurnOn pareceria funcionar pero el
 * ajuste duraria solo hasta la primera escritura de un digito o de un LED.
 *
 * Arranca en 7, el maximo, para que el display se vea sin tener que ajustar
 * nada.
 */
static uint8_t s_brightness = 7;


/* ==========================================================================
 * Nivel 0: mover un pin
 *
 * Envoltorios de una linea sobre la HAL. No aportan logica; estan para que
 * las secuencias de mas abajo se lean como el cronograma de la hoja de
 * caracteristicas en vez de como una lista de HAL_GPIO_WritePin.
 * ========================================================================== */

static void tm1638_SDOhigh(TM1638 *tm)
{
    HAL_GPIO_WritePin(tm->dio_port, tm->dio_pin, GPIO_PIN_SET);
}

static void tm1638_SDOlow(TM1638 *tm)
{
    HAL_GPIO_WritePin(tm->dio_port, tm->dio_pin, GPIO_PIN_RESET);
}

static void tm1638_STBhigh(TM1638 *tm)
{
    HAL_GPIO_WritePin(tm->stb_port, tm->stb_pin, GPIO_PIN_SET);
}

static void tm1638_STBlow(TM1638 *tm)
{
    HAL_GPIO_WritePin(tm->stb_port, tm->stb_pin, GPIO_PIN_RESET);
}

static void tm1638_CLKhigh(TM1638 *tm)
{
    HAL_GPIO_WritePin(tm->clk_port, tm->clk_pin, GPIO_PIN_SET);
}

static void tm1638_CLKlow(TM1638 *tm)
{
    HAL_GPIO_WritePin(tm->clk_port, tm->clk_pin, GPIO_PIN_RESET);
}


/* ==========================================================================
 * Nivel 1: enmarcar una orden y enviar un byte
 *
 * Secuencias comprobadas en placa. No se han modificado.
 * ========================================================================== */

/** Abre una transmision: deja CLK y STB en reposo y luego baja STB. */
static void tm1638_StartPacket(TM1638 *tm)
{
    tm1638_CLKhigh(tm);
    tm1638_STBhigh(tm);
    tm1638_STBlow(tm);
    tm1638_CLKlow(tm);
}

/** Cierra la transmision y devuelve las lineas a reposo. */
static void tm1638_EndPacket(TM1638 *tm)
{
    tm1638_CLKlow(tm);
    tm1638_STBlow(tm);
    tm1638_CLKhigh(tm);
    tm1638_STBhigh(tm);
}

/**
 * Separa dos ordenes dentro de la misma transmision.
 *
 * El TM1638 da por terminada una orden cuando STB sube, asi que este pulso es
 * lo que le dice "lo anterior ya esta, empieza a contar de nuevo". El nombre
 * viene de la version original; describe mal lo que hace, pero se conserva
 * para no romper nada.
 */
static void tm1638_Confirm(TM1638 *tm)
{
    tm1638_STBlow(tm);
    tm1638_SDOlow(tm);
    tm1638_STBhigh(tm);
    tm1638_SDOhigh(tm);
    tm1638_STBlow(tm);
    tm1638_SDOlow(tm);
}

/**
 * Envia un byte, bit menos significativo primero.
 *
 * Para cada bit: se baja CLK, se pone el dato en DIO, y se sube CLK. Es ese
 * flanco de subida el que el chip usa para capturar.
 */
static void tm1638_SendData(TM1638 *tm, uint8_t Data)
{
    uint8_t ByteData[8] = {0};

    /* El byte, desmenuzado en bits. El "&& 1" convierte cualquier valor
     * distinto de cero en un 1 limpio. */
    for (uint8_t j = 0; j < 8; j++)
    {
        ByteData[j] = (Data & (0x01 << j)) && 1;
    }

    for (int8_t j = 0; j < 8; j++)
    {
        tm1638_CLKlow(tm);
        if (ByteData[j] == GPIO_PIN_SET)
        {
            tm1638_SDOhigh(tm);
        }
        else
        {
            tm1638_SDOlow(tm);
        }
        tm1638_CLKhigh(tm);
    }
}

/**
 * Reaplica el encendido y el brillo guardado.
 *
 * Se llama al final de cada escritura porque el chip necesita la orden de
 * display para que lo escrito se vea.
 */
static void tm1638_ApplyDisplayControl(TM1638 *tm)
{
    tm1638_SendData(tm, DISPLAY_ON | s_brightness);
}

/**
 * Muestra "Error X" cuando a una funcion le llega un argumento imposible.
 *
 * La biblioteca no puede devolver un codigo de error -las funciones publicas
 * no devuelven codigo de error-, asi que
 * avisa por el unico medio que tiene: la propia pantalla.
 *
 * El cerrojo evita la recursion. Escribir el aviso pasa por la tabla de
 * caracteres, que a su vez puede fallar; sin el cerrojo, un error dentro del
 * aviso volveria a llamar al aviso. Hoy no ocurre porque las letras de
 * "Error" son todas dibujables, pero dependia de ese detalle.
 */
static void tm1638_Error(TM1638 *tm, char codigo)
{
    static bool dentro = false;

    if (dentro)
    {
        return;
    }
    dentro = true;

    char texto[8] = { 'E', 'r', 'r', 'o', 'r', ' ', codigo, '\0' };
    tm1638_DisplayTxt(tm, texto);

    dentro = false;
}


/* ==========================================================================
 * Nivel 2: display
 * ========================================================================== */

/**
 * Borra los 16 bytes de RAM y deja el display apagado.
 *
 * Aprovecha el auto-incremento: se da la direccion 0 una sola vez y luego los
 * 16 bytes seguidos, en vez de repetir la direccion en cada uno.
 */
void tm1638_DisplayClear(TM1638 *tm)
{
    tm1638_StartPacket(tm);

    tm1638_SendData(tm, DATA_SET);      /* escribir con auto-incremento */
    tm1638_Confirm(tm);

    tm1638_SendData(tm, 0xc0);          /* empezar en la direccion 0 */
    for (uint8_t i = 0; i < 16; i++)
    {
        tm1638_SendData(tm, 0x00);
    }
    tm1638_Confirm(tm);

    tm1638_SendData(tm, DISPLAY_OFF);

    tm1638_EndPacket(tm);
}

/**
 * Enciende el display con el brillo pedido.
 *
 * El 0 es un brillo valido: el TM1638 tiene ocho niveles, del 0 al 7, y el 0
 * es el mas tenue, NO el apagado. Para apagar esta DISPLAY_OFF, que es una
 * orden distinta.
 */
void tm1638_TurnOn(TM1638 *tm, uint8_t brightness)
{
    if (brightness > 7)
    {
        tm1638_Error(tm, '1');
        return;
    }

    s_brightness = brightness;          /* que sobreviva a la siguiente escritura */
    tm1638_ApplyDisplayControl(tm);
}

/**
 * Escribe un caracter en una posicion, opcionalmente con su punto decimal.
 *
 * El punto es el bit 7 del patron de segmentos, de ahi el 0x80.
 */
void tm1638_DisplayChar(TM1638 *tm, int position, char c, bool dot)
{
    int hex = chr_to_hex(c);

    if (hex < 0)                        /* ese caracter no se puede dibujar */
    {
        tm1638_Error(tm, '7');
        return;
    }

    if (dot)
    {
        hex += 0x80;
    }

    tm1638_Seg(tm, position, hex);
}

/**
 * Escribe un texto alineado a la derecha.
 *
 * Se recorre la cadena DE DERECHA A IZQUIERDA y se va rellenando desde el
 * ultimo digito hacia atras; lo que sobra por la izquierda queda en blanco.
 * Asi "42" sale como "      42" sin que quien llama tenga que rellenar nada.
 *
 * Los puntos no consumen posicion: al encontrar uno se marca para el caracter
 * que vendra a continuacion, que es el que tiene a su izquierda en el texto.
 */
void tm1638_DisplayTxt(TM1638 *tm, char *c)
{
    /* Ocho espacios que llenan el array justo: no hay sitio para el cero
     * final, y no hace falta, porque solo se accede por indice. */
    char padded[8] = "        ";
    uint8_t dot[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    uint8_t p = 0;                      /* digitos ya colocados */

    for (int8_t i = strlen(c) - 1; i >= 0; i--)
    {
        if (p == 8)                     /* no caben mas de ocho */
        {
            tm1638_Error(tm, '2');
            return;
        }

        if (c[i] == '.')
        {
            dot[p] = 1;
        }
        else
        {
            padded[p++] = c[i];
        }
    }

    /* padded[0] es el digito de mas a la derecha y la posicion 8 es la de mas
     * a la derecha del modulo, asi que el recorrido va al reves. */
    for (int8_t i = 7; i >= 0; i--)
    {
        tm1638_DisplayChar(tm, 7 - i + 1, padded[i], dot[i]);
    }
}

/**
 * Escribe LOS SEGMENTOS de un digito, sin pasar por la tabla de caracteres.
 *
 * Los digitos ocupan las direcciones pares: la posicion 1 es la 0, la 2 es la
 * 2, y asi hasta la 8, que es la 14. De ahi el 2*(position-1).
 */
void tm1638_Seg(TM1638 *tm, int position, int data)
{
    if (!(position >= 1 && position <= 8))
    {
        tm1638_Error(tm, '5');
        return;
    }
    if (!(data >= 0x00 && data <= 0xff))
    {
        tm1638_Error(tm, '6');
        return;
    }

    tm1638_StartPacket(tm);

    tm1638_SendData(tm, DATA_SET);
    tm1638_Confirm(tm);

    tm1638_SendData(tm, 0xc0 + 2 * (position - 1));   /* direccion par: digito */

    tm1638_CLKhigh(tm);
    tm1638_SDOhigh(tm);
    tm1638_SDOlow(tm);
    tm1638_SendData(tm, data);
    tm1638_Confirm(tm);

    tm1638_ApplyDisplayControl(tm);

    tm1638_EndPacket(tm);
}

/**
 * Enciende o apaga uno de los ocho LEDs.
 *
 * Los LEDs ocupan las direcciones impares, intercaladas con las de los
 * digitos: el LED 1 es la 1, el 2 es la 3, y el 8 es la 15. De ahi el
 * 2*position-1. Solo cuenta el bit 0 del byte.
 */
void tm1638_Led(TM1638 *tm, int position, int on)
{
    if (!(position >= 1 && position <= 8))
    {
        tm1638_Error(tm, '3');
        return;
    }
    if (!(on == 0 || on == 1))
    {
        tm1638_Error(tm, '4');
        return;
    }

    tm1638_StartPacket(tm);

    tm1638_SendData(tm, DATA_SET);
    tm1638_Confirm(tm);

    tm1638_SendData(tm, 0xc0 + 2 * position - 0x01);  /* direccion impar: LED */

    tm1638_CLKhigh(tm);
    tm1638_SDOhigh(tm);
    tm1638_SDOlow(tm);

    tm1638_SendData(tm, on);
    tm1638_Confirm(tm);

    tm1638_ApplyDisplayControl(tm);

    tm1638_EndPacket(tm);
}


/* ==========================================================================
 * Nivel 2: teclado
 * ========================================================================== */

/**
 * Lee el estado de los ocho pulsadores.
 *
 * Es la unica funcion en la que DIO cambia de sentido. La secuencia es:
 * mandar la orden de lectura, pasar DIO a entrada, sacar 32 pulsos de reloj
 * leyendo el nivel en cada uno, y devolver DIO a salida.
 *
 * Los 32 bits que llegan NO estan en el orden de las teclas: el chip los
 * organiza por su matriz interna, y las ocho que interesan caen en las
 * posiciones 1, 9, 17, 25 (teclas S1 a S4) y 5, 13, 21, 29 (S5 a S8). Esa es
 * toda la explicacion del reparto de mas abajo, que de otro modo parece
 * arbitrario.
 *
 * El pin se lee SIEMPRE desde los campos de la estructura, nunca desde las
 * macros DIO_GPIO_Port y DIO_Pin del proyecto. Con las macros pareceria
 * funcionar, porque el .ioc etiqueta ese pin asi, pero la biblioteca dejaria
 * de servir para cualquier otra asignacion de pines y, con dos modulos a la
 * vez, leeria siempre el mismo.
 */
uint8_t tm1638_ScanButtons(TM1638 *tm)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    uint8_t bits[32] = {0};
    uint8_t keys = 0;
    int indice = -1;
    int ok = 1;                         /* ver la nota sobre el bucle, abajo */

    tm1638_StartPacket(tm);
    tm1638_SendData(tm, DATA_READ);

    /* DIO pasa a entrada: a partir de aqui manda el TM1638 */
    GPIO_InitStruct.Pin   = tm->dio_pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(tm->dio_port, &GPIO_InitStruct);

    for (int i = 0; i < 4; i++)         /* cuatro bytes */
    {
        tm1638_CLKhigh(tm);
        for (int j = 0; j < 8; j++)     /* ocho bits cada uno */
        {
            tm1638_CLKhigh(tm);
            indice++;
            bits[indice] = HAL_GPIO_ReadPin(tm->dio_port, tm->dio_pin);

            /* Esta comparacion es redundante para el resultado: si ninguna
             * de las ocho posiciones que se miran al final vale 1, keys seria
             * 0 de todas formas. Se conserva por una razon de TIEMPO, no de
             * logica: quitar instrucciones de este bucle acelera el reloj que
             * se esta generando, y el TM1638 admite 1 MHz como maximo. */
            if (bits[indice] && j != 0)
            {
                ok = 0;                 /* hay algo pulsado */
            }

            tm1638_CLKlow(tm);
        }
    }
    tm1638_CLKhigh(tm);
    tm1638_STBhigh(tm);

    /* DIO vuelve a salida. La velocidad baja es la que ha tenido siempre y
     * sobra para este protocolo, que va a unas decenas de kHz. */
    GPIO_InitStruct.Pin   = tm->dio_pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(tm->dio_port, &GPIO_InitStruct);

    /* De la matriz del chip al orden natural S1..S8 */
    if (ok == 0)
    {
    if (bits[1])  keys |= 0x01;         /* S1 */
    if (bits[9])  keys |= 0x02;         /* S2 */
    if (bits[17]) keys |= 0x04;         /* S3 */
    if (bits[25]) keys |= 0x08;         /* S4 */
    if (bits[5])  keys |= 0x10;         /* S5 */
    if (bits[13]) keys |= 0x20;         /* S6 */
    if (bits[21]) keys |= 0x40;         /* S7 */
    if (bits[29]) keys |= 0x80;         /* S8 */
    }

    return keys;
}

/** ¿Esta pulsada la tecla `position`? `buttons` viene de tm1638_ScanButtons. */
bool tm1638_KeyState(TM1638 *tm, uint8_t buttons, int position)
{
    if (!(position >= 1 && position <= 8))
    {
        tm1638_Error(tm, 'K');
        return false;
    }

    return (buttons & (1 << (position - 1))) != 0;
}

/**
 * Numero de la tecla pulsada mas baja, o 0 si no hay ninguna.
 *
 * Devuelve la MAS BAJA, no la suma de las pulsadas: sumarlas haria que S1 y
 * S2 a la vez dieran 3, que es indistinguible de S3 sola. Para saber todas
 * las pulsadas hay que mirar los bits de tm1638_ScanButtons.
 */
uint8_t tm1638_ReadKey(TM1638 *tm, uint8_t buttons)
{
    (void)tm;                           /* no hace falta, se mantiene la firma */

    for (uint8_t i = 0; i < 8; i++)
    {
        if ((buttons & (1 << i)) != 0)
        {
            return i + 1;
        }
    }
    return 0;
}

/**
 * Espera activa hasta que alguien pulse.
 *
 * BLOQUEA: no deja correr nada mas mientras espera. Va contra lo que piden
 * las practicas de P2 en adelante y no la usa ningun proyecto; se conserva
 * por compatibilidad.
 */
uint8_t tm1638_ReadKey_Blocking(TM1638 *tm)
{
    uint8_t buttons = 0;

    do {
        buttons = tm1638_ScanButtons(tm);
    } while (buttons == 0);

    return tm1638_ReadKey(tm, buttons);
}


/* ==========================================================================
 * Tabla de caracteres
 *
 * Cada valor dice que segmentos se encienden, un bit por segmento:
 *
 *        aaaa          0x01 = a        0x08 = d        0x40 = g
 *       f    b         0x02 = b        0x10 = e        0x80 = punto
 *       f    b         0x04 = c        0x20 = f
 *        gggg
 *       e    c       Ejemplo: '7' es 0x07 = a + b + c, los tres trazos que
 *       e    c       hacen falta para un siete.
 *        dddd  o
 *
 * Faltan las letras que con siete trazos no se distinguirian: G, K, M, Q, R,
 * V, W, X, Z y las mayusculas D, N, T, Y. Y hay dos que si estan pero se
 * dibujan igual que un numero, cosa que conviene saber antes de usarlas:
 * 'B' sale identica a '8' y 'O' identica a '0'.
 * ========================================================================== */

int8_t chr_to_hex(char c)
{
    switch (c)
    {
    /* --- cifras --- */
    case '0': return 0x3f;
    case '1': return 0x06;
    case '2': return 0x5b;
    case '3': return 0x4f;
    case '4': return 0x66;
    case '5': return 0x6d;
    case '6': return 0x7d;
    case '7': return 0x07;
    case '8': return 0x7f;
    case '9': return 0x6f;

    /* --- signos --- */
    case ' ': return 0x00;              /* todo apagado */
    case '_': return 0x08;              /* solo el trazo de abajo */
    case '-': return 0x40;              /* solo el trazo central */

    /* --- mayusculas --- */
    case 'A': return 0x77;
    case 'B': return 0x7f;              /* igual que el 8 */
    case 'C': return 0x39;
    case 'E': return 0x79;
    case 'F': return 0x71;
    case 'H': return 0x76;
    case 'I': return 0x06;              /* igual que el 1 */
    case 'J': return 0x0e;
    case 'L': return 0x38;
    case 'O': return 0x3f;              /* igual que el 0 */
    case 'P': return 0x73;
    case 'S': return 0x6d;              /* igual que el 5 */
    case 'U': return 0x3e;

    /* --- minusculas --- */
    case 'a': return 0x5f;
    case 'b': return 0x7c;
    case 'c': return 0x58;
    case 'd': return 0x5e;
    case 'h': return 0x74;
    case 'i': return 0x04;
    case 'n': return 0x54;
    case 'o': return 0x5c;
    case 'r': return 0x50;
    case 't': return 0x78;
    case 'u': return 0x1c;
    case 'y': return 0x6e;
    }

    return -1;                          /* no se puede dibujar */
}
