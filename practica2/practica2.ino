const int botonA = 8;
const int botonB = 9;
const int ledEntrega = 7;
const int ledError = 6;
const int detectorRecogida = 10;
const int potenciometro = 1;
const int ledsBinario0 = 5;
const int ledsBinario1 = 4;
const int ledsBinario2 = 3;
const int ledsBinario3 = 2;
const int ledExistencias1 = 11;
const int ledExistencias2 = 12;
const int ledExistencias3 = 13;

// Estructura para almacenar la información de cada producto
struct Producto {
  char nombre[20];         // Nombre del producto
  int precio;              // Precio del producto
  int compartimentos[3];   // Cantidad en cada compartimento
  int temperaturaIdeal;    // Temperatura ideal para el producto
  int stockDeseado;        // Stock deseado del producto
  int ledExistencias;      // LED indicador de existencias
  unsigned long tiempoParpadeoLed; // Tiempo para controlar el parpadeo del LED
  bool estadoLed;          // Estado actual del LED (encendido/apagado)
  bool parpadeando;        // Indica si el LED está parpadeando
};

// Definición de productos
#define NUM_PRODUCTOS 3
Producto productos[NUM_PRODUCTOS] = {
  {"Bocata Lomo", 5, {3, 2, 1}, 5, 10, ledExistencias1, 0, false, false},
  {"Snack", 7, {2, 3, 2}, 20, 12, ledExistencias2, 0, false, false},
  {"Agua", 3, {1, 2, 3}, 15, 15, ledExistencias3, 0, false, false}
};

enum Estado {idle, seleccion, recogida, comprar, tecnico, pretecnico, password, postecnico, fueraServicio};
enum EstadoBoton{PULSADO, SUELTO, TR_PULS, TR_SOLT};
Estado estado = idle;
EstadoBoton estadoBotonA = SUELTO, estadoBotonB = SUELTO, estadoBotonPuente = SUELTO;
unsigned long t_ini = 0, t_initecnico = 0, t_iniEliminarSaldo = 0, tiempoParpadeo = 0, t_iniRecogida = 0;

// Variables para detectar doble pulsación
unsigned long t_ultimaPulsacionA = 0;
bool esperandoDobleClick = false;

bool bloq = false, saldoEliminado = false, parpadeando = false, avisoTimeout = false;
int saldo = 0, contadorParpadeo = 0, pinParpadeo = -1;
int productoSeleccionado = 0; // Índice del producto seleccionado actualmente

// Variables para el sistema de contraseña
const int PASSWORD_LENGTH = 4; // Longitud de la contraseña
const int PASSWORD[PASSWORD_LENGTH] = {0, 0, 0, 0}; // Contraseña predefinida (valores de 0 a 15)
int passwordInput[PASSWORD_LENGTH] = {0}; // Almacena la entrada del usuario
int passwordIndex = 0; // Índice actual en la entrada de contraseña
unsigned long t_password = 0; // Tiempo para controlar la entrada de contraseña

void setup() {
  // put your setup code here, to run once
    delay(500);
    pinMode(2,OUTPUT);
    pinMode(3,OUTPUT);
    pinMode(4,OUTPUT);
    pinMode(5,OUTPUT);
    pinMode(6,OUTPUT); // LED ERROR
    pinMode(7,OUTPUT); // LED TRANS
    pinMode(8,INPUT_PULLUP); // SI USO PULLUP PORQUE no   HE PUESTO LAS RESISTENCIAS YO
    pinMode(9,INPUT_PULLUP);
    pinMode(10,INPUT_PULLUP);
    pinMode(11,OUTPUT); // LED EXISTENCIAS1
    pinMode(12,OUTPUT); // LED EXISTENCIAS2
    pinMode(13,OUTPUT); // LED EXISTENCIAS3
    Serial.begin(9600);
    t_ini = millis();
    Serial.println();
    Serial.println("PROGRAMA CORRIENDO; Máquina expendedora con múltiples productos");
    
    // Mostrar información de los productos disponibles
    for (int i = 0; i < NUM_PRODUCTOS; i++) {
      Serial.print("Producto ");
      Serial.print(i+1);
      Serial.print(": ");
      Serial.print(productos[i].nombre);
      Serial.print(" - Precio: ");
      Serial.print(productos[i].precio);
      Serial.print(" EUR");
      Serial.print(" - Stock deseado: ");
      Serial.print(productos[i].stockDeseado);
      
      // Calcular stock total actual
      int stockTotal = 0;
      for (int j = 0; j < 3; j++) {
        stockTotal += productos[i].compartimentos[j];
      }
      Serial.print(" - Stock actual: ");
      Serial.println(stockTotal);
      
      // Inicializar variables de parpadeo
      productos[i].tiempoParpadeoLed = millis();
      productos[i].estadoLed = false;
      productos[i].parpadeando = false;
    }
    
    // Inicializar LEDs de existencias
    actualizarLedsExistencias();
}
/*
https://docs.arduino.cc/retired/hacking/software/PortManipulation/ DOC AQUI
(1 << pin): Es una operación de desplazamiento de bits. 
Aquí, el número 1 se desplaza a la izquierda pin veces. 
Esto crea un número binario en el que solo el bit en la posición pin es 1, 
y todos los demás bits son 0. Por ejemplo, si pin es 3, 
entonces (1 << pin) resulta en 00001000 en binario.*/

void swagDigitalWrite(int pin, bool valor) {
  if (pin >= 2 && pin <= 7) {
    if (valor) {
      PORTD |= (1 << pin); // Se usa OR para poder sobreescribir el 1 en el 0
    } else {
      PORTD &= ~(1 << pin); // Se usa AND para poder sobreescribir el 0 en el 1
    }
  } else if (pin >= 8 && pin <= 13) {
    if (valor) {
      PORTB |= (1 << (pin - 8)); 
    } else {
      PORTB &= ~(1 << (pin - 8));
    }
  }
}

bool swagDigitalRead(int pin) {
  if (pin >= 2 && pin <= 7) {
    return PIND & (1 << pin); // Lee el bit correspondiente en PIND
  } else if (pin >= 8 && pin <= 13) {
    return PINB & (1 << (pin - 8)); // Ajuste para el rango de pines 8-13
  }
  return false; // Retorna false si el pin no está en el rango válido
}

int swagAnalogRead(int pin) {
// file:///C:/Users/carmo/Desktop/Sistemas%20electronicos%20programables/practica1/Atmel-7810-Automotive-Microcontrollers-ATmega328P_Datasheet.pdf
// Página 206 IMPORTANTE 211 tmabien=??
  // Configurar el pin como entrada analógica y activar el bit 7
  ADMUX = (ADMUX & 0xF0) | (pin & 0x0F) | (0 << 7)| (1 << 6); // Hago mascara para que no se modifiquen los bits, 6 es REFS0 y 7 es REF1
  // Eso anterior en la página 218
  // Serial.println(ADMUX);
  // Iniciar la conversión
  ADCSRA |= (1 << ADSC); // bit 6 en ADCSRA es ADSC, ver pág 218
  // Esperar a que la conversión termine
  while (ADCSRA & (1 << ADSC));
    //Serial.println("Esperando a que termine la conversión ADC...");
    //Serial.println(ADSC);

  // Retornar el valor leído
  return ADC;
}

EstadoBoton gestionBoton(EstadoBoton estadoAnteriorBoton, bool lectura){
  if(lectura == LOW){ // PULSADO
      if(estadoAnteriorBoton == PULSADO ||
          estadoAnteriorBoton == TR_PULS)
          return PULSADO;
      else
          return TR_PULS;
  }
  else{ // SUELTO
      if(estadoAnteriorBoton == PULSADO ||
        estadoAnteriorBoton == TR_PULS)
          return TR_SOLT;
      else
          return SUELTO;
  }
}

int seleccionarCompartimento(int compartimentos[]) {
  int maxIndex  = 0;
  for (int i = 1; i < 3; i++) {
    if (compartimentos[i] > compartimentos[maxIndex]) {
      maxIndex = i;
    }
  }
  return maxIndex ;
}

void iniciarParpadeo(int pin, bool *pParpadeando, int *pContadorParpadeo, int *pPinParpadeo, unsigned long *pTiempoParpadeo) {
  *pParpadeando = true;
  *pContadorParpadeo = 0;
  *pPinParpadeo = pin;
  *pTiempoParpadeo = millis();
}
// SOLO PASAS EL PUNTERO DE LAS QUE VAYAS A MODIFICAR
void gestionarParpadeo(bool *pParpadeando, int *pContadorParpadeo, int *pPinParpadeo, unsigned long *pTiempoParpadeo) {
  if (*pParpadeando) {
    if (millis() - *pTiempoParpadeo >= 200) {
      *pTiempoParpadeo = millis();
      if (*pContadorParpadeo < 6) {
        swagDigitalWrite(*pPinParpadeo, !swagDigitalRead(*pPinParpadeo));
        (*pContadorParpadeo)++;
      } else {
        *pParpadeando = false;
        swagDigitalWrite(*pPinParpadeo, LOW);
      }
    }
  }
}

// Función para actualizar los LEDs de existencias de productos
// Implementa la lógica de parpadeo proporcional a las existencias:
// - Sin existencias: LED encendido fijo
// - Pocas existencias (menos del 50% del stock deseado): LED parpadeando con periodo proporcional
// - Existencias suficientes: LED apagado
void actualizarLedsExistencias() {
  for (int i = 0; i < NUM_PRODUCTOS; i++) {
    // Calcular el stock total del producto
    int stockTotal = 0;
    for (int j = 0; j < 3; j++) {
      stockTotal += productos[i].compartimentos[j];
    }
    
    // Sin existencias -> LED encendido fijo
    if (stockTotal == 0) {
      swagDigitalWrite(productos[i].ledExistencias, HIGH);
      productos[i].parpadeando = false;
      productos[i].estadoLed = true;
    }
    // Pocas existencias (menos del 50% del stock deseado) -> LED parpadeando con periodo proporcional
    else if (stockTotal < productos[i].stockDeseado / 2) {
      // Si no estaba parpadeando antes, inicializar el tiempo
      if (!productos[i].parpadeando) {
        productos[i].tiempoParpadeoLed = millis();
        productos[i].estadoLed = false;
      }
      
      productos[i].parpadeando = true;
      
      // Calcular periodo de parpadeo proporcional a las existencias
      // Más existencias = parpadeo más lento (periodo más largo)
      // Menos existencias = parpadeo más rápido (periodo más corto)
      // Periodo entre 200ms (mínimo) y 1000ms (máximo)
      int periodoBase = 200; // Periodo mínimo en ms
      int periodoMax = 1000; // Periodo máximo en ms
      int periodoParpadeo = periodoBase + (stockTotal * (periodoMax - periodoBase) / (productos[i].stockDeseado / 2));
      
      // Gestionar el parpadeo del LED
      if (millis() - productos[i].tiempoParpadeoLed >= periodoParpadeo) {
        productos[i].tiempoParpadeoLed = millis();
        productos[i].estadoLed = !productos[i].estadoLed;
        swagDigitalWrite(productos[i].ledExistencias, productos[i].estadoLed);
      }
    }
    // Existencias suficientes -> LED apagado
    else {
      swagDigitalWrite(productos[i].ledExistencias, LOW);
      productos[i].parpadeando = false;
      productos[i].estadoLed = false;
    }
  }
}


void loop() {
// put your main code here, to run repeatedly:
  estadoBotonA = gestionBoton(estadoBotonA, swagDigitalRead(botonA));
  estadoBotonB = gestionBoton(estadoBotonB, swagDigitalRead(botonB));
  estadoBotonPuente = gestionBoton(estadoBotonPuente, swagDigitalRead(detectorRecogida));
  gestionarParpadeo(&parpadeando, &contadorParpadeo, &pinParpadeo, &tiempoParpadeo);
  actualizarLedsExistencias(); // Actualizar los LEDs de existencias
  
  int valor = swagAnalogRead(potenciometro) * 16 / 1024;
  bool valorBinario[4]; // Array para almacenar el valor binario
  // Convertir el valor a binario manualmente
  for (int i = 3; i >= 0; i--) {
    valorBinario[i] = valor % 2;
    valor /= 2;
  }
  // Mostrar el valor en los LEDs
  swagDigitalWrite(ledsBinario3, valorBinario[3]);
  swagDigitalWrite(ledsBinario2, valorBinario[2]);
  swagDigitalWrite(ledsBinario1, valorBinario[1]);
  swagDigitalWrite(ledsBinario0, valorBinario[0]);
  
  // Detectar doble pulsación del botón A
  if (estadoBotonA == TR_SOLT) {
    unsigned long tiempoActual = millis();
    if (esperandoDobleClick) {
      if (tiempoActual - t_ultimaPulsacionA <= 500) {
        estado = fueraServicio;
        Serial.println("MAQUINA FUERA DE SERVICIO");
        iniciarParpadeo(ledError, &parpadeando, &contadorParpadeo, &pinParpadeo, &tiempoParpadeo);
      }
      esperandoDobleClick = false;
    } else {

      esperandoDobleClick = true;
      t_ultimaPulsacionA = tiempoActual;
    }
  }
  
  // Si ha pasado demasiado tiempo desde la primera pulsación, cancelamos la espera
  if (esperandoDobleClick && millis() - t_ultimaPulsacionA > 250) {
    esperandoDobleClick = false;
  }
  
  switch (estado){
  case fueraServicio:
    break;
    
  case idle:
    // Añadir saldo
    if (estadoBotonA == TR_SOLT && estadoBotonB == !PULSADO && !saldoEliminado) {
      saldo += swagAnalogRead(potenciometro) * 16 / 1024;
      Serial.print(saldo);
      Serial.print(" EUR actualmente en el saldo\n");
    }
    // Entrar en modo técnico
    else if (estadoBotonA == PULSADO && estadoBotonB == PULSADO) { //TECNICO
      if (bloq == 0) {
        t_initecnico = millis();
        bloq = 1;
      }
      if (bloq == 1 && millis() - t_initecnico >= 2000) {
        estado = pretecnico; // Cambiamos a un estado intermedio para evitar activar funciones al soltar
        Serial.println("MODO TECNICO ACTIVADO");
      }
    } 
    // Seleccionar producto, CADA PRODUCTO TIENE 3 COMPARTIMENTOS.
    else if (estadoBotonB == TR_SOLT && estadoBotonA == !PULSADO) {
      // Usar el valor mostrado en los LEDs para seleccionar el producto directamente
      int valorPotenciometro = swagAnalogRead(potenciometro) * 16 / 1024;
      // Asegurar que el valor está entre 1 y NUM_PRODUCTOS
      if (valorPotenciometro >= 1 && valorPotenciometro <= NUM_PRODUCTOS) {
        productoSeleccionado = valorPotenciometro - 1; // Ajustar a índice base 0
      } else {
        // Si el valor está fuera del rango, usar el primer producto
        productoSeleccionado = 0;
      }
      Serial.print("Producto seleccionado: ");
      Serial.print(productos[productoSeleccionado].nombre);
      Serial.print(" - Precio: ");
      Serial.print(productos[productoSeleccionado].precio);
      Serial.println(" EUR");
      estado = seleccion;
    } 
    // Eliminar saldo
    else if (estadoBotonA == PULSADO && estadoBotonB == SUELTO) {
      if (bloq == 0) {
        t_iniEliminarSaldo = millis();
        bloq = 1;
      }
      if (bloq == 1 && millis() - t_iniEliminarSaldo >= 5000) {
        saldo = 0;
        Serial.println("Saldo eliminado");
        iniciarParpadeo(ledError, &parpadeando, &contadorParpadeo, &pinParpadeo, &tiempoParpadeo);
        saldoEliminado = true;
        bloq = 0;
      }
    } else {
      bloq = 0; // Resetear el bloqueo si se suelta algún botón
      saldoEliminado = false;
    }
    break;
    
  case seleccion:
    // Confirmar compra
    if (estadoBotonB == TR_SOLT && estadoBotonA == !PULSADO) {
      if (saldo >= productos[productoSeleccionado].precio) {
        int compartimentoSeleccionado = seleccionarCompartimento(productos[productoSeleccionado].compartimentos);
        if (productos[productoSeleccionado].compartimentos[compartimentoSeleccionado] > 0) {
          saldo -= productos[productoSeleccionado].precio;
          productos[productoSeleccionado].compartimentos[compartimentoSeleccionado]--;
          Serial.print("Producto ");
          Serial.print(productos[productoSeleccionado].nombre);
          Serial.println(" entregado del compartimento: " + String(compartimentoSeleccionado + 1));
          Serial.println("POR FAVOR RETIRE EL PRODUCTO");
          
          // Mostrar estado de los compartimentos del producto
          Serial.print("Compartimentos de ");
          Serial.print(productos[productoSeleccionado].nombre);
          Serial.println(": ");
          for (int i = 0; i < 3; i++) {
            Serial.print("[Comp. ");
            Serial.print(i + 1);
            Serial.print("]: ");
            Serial.println(productos[productoSeleccionado].compartimentos[i]);
          }
          
          // Calcular stock total y mostrar estado
          int stockTotal = 0;
          for (int j = 0; j < 3; j++) {
            stockTotal += productos[productoSeleccionado].compartimentos[j];
          }
          Serial.print("Stock total: ");
          Serial.print(stockTotal);
          Serial.print("/");
          Serial.print(productos[productoSeleccionado].stockDeseado);
          
          // Mostrar estado de existencias
          if (stockTotal == 0) {
            Serial.println(" - ¡SIN EXISTENCIAS!");
          } else if (stockTotal < productos[productoSeleccionado].stockDeseado / 2) {
            Serial.println(" - ¡POCAS EXISTENCIAS! (LED parpadeando)");
          } else {
            Serial.println(" - Existencias suficientes");
          }
          
          iniciarParpadeo(ledEntrega, &parpadeando, &contadorParpadeo, &pinParpadeo, &tiempoParpadeo);
          estado = recogida; // Cambiar al estado recogida para esperar la recogida
          t_iniRecogida = millis(); // Iniciar el contador para el timeout
          avisoTimeout = false; // Reiniciar el flag de aviso de timeout
        } else {
          Serial.println("El producto seleccionado no tiene existencias.");
          iniciarParpadeo(ledError, &parpadeando, &contadorParpadeo, &pinParpadeo, &tiempoParpadeo);
          estado = idle;
        }
      } else {
        Serial.println("Saldo insuficiente");
        iniciarParpadeo(ledError, &parpadeando, &contadorParpadeo, &pinParpadeo, &tiempoParpadeo);
        estado = idle;
      }
    }
    // Volver al estado idle
    else if (estadoBotonA == TR_SOLT && estadoBotonB == !PULSADO) {
      Serial.println("Selección cancelada");
      estado = idle;
    }
    break;

  case recogida:
    // Comprobar si han pasado 4 segundos sin recoger el producto
    if (!avisoTimeout && millis() - t_iniRecogida >= 4000) {
      Serial.println("QUE LO RECOJAS!!!!!!!!!!");
      avisoTimeout = true; // Marcar que ya se ha mostrado el aviso
    }
    
    if (estadoBotonPuente == PULSADO) {
      Serial.println("Producto recogido");
      estado = idle;
    }
    break;

  case pretecnico: // Estado intermedio para evitar activar funciones al soltar los botones
    if (estadoBotonA == SUELTO && estadoBotonB == SUELTO) {
      estado = password; // Cambiar al estado de verificación de contraseña
      passwordIndex = 0; // Reiniciar el índice de entrada de contraseña
      for (int i = 0; i < PASSWORD_LENGTH; i++) {
        passwordInput[i] = 0; // Reiniciar la entrada de contraseña
      }
      Serial.println("Introduzca la contraseña (4 dígitos):");
      Serial.println("Use el potenciómetro para seleccionar un valor (0-15) y pulse el botón A para confirmar cada dígito.");
      bloq = 0; // Resetear el bloqueo
    }
    break;
    
  case tecnico:
    if (estadoBotonA == PULSADO && estadoBotonB == PULSADO) {
      // Si ambos botones se pulsan a la vez, preparamos para salir del modo técnico
      if (bloq == 0) {
        t_initecnico = millis();
        bloq = 1;
      }
      if (bloq == 1 && millis() - t_initecnico >= 2000) {
        estado = postecnico; // Cambiamos a un estado intermedio para evitar activar funciones al soltar
        bloq = 0;
        Serial.println("MODO TECNICO DESACTIVADO");
      }
    } else if (estadoBotonA == TR_SOLT && estadoBotonB != PULSADO) {
      // Seleccionar producto y modificar su precio
      int valorPotenciometro = swagAnalogRead(potenciometro) * 16 / 1024;
      int prodIndex = 0;
      // Usar el valor mostrado en los LEDs para seleccionar el producto directamente
      if (valorPotenciometro >= 1 && valorPotenciometro <= NUM_PRODUCTOS) {
        prodIndex = valorPotenciometro - 1; // Ajustar a índice base 0
      }
      int nuevoPrecio = valorPotenciometro; // Precio mínimo de 0
      productos[prodIndex].precio = nuevoPrecio;
      Serial.print("Nuevo precio para ");
      Serial.print(productos[prodIndex].nombre);
      Serial.print(": ");
      Serial.println(nuevoPrecio);
    } else if (estadoBotonB == TR_SOLT && estadoBotonA != PULSADO) { 
      // Seleccionar producto y compartimento para recargar
      int valorPotenciometro = swagAnalogRead(potenciometro) * 16 / 1024;
      int prodIndex = 0;
      // Usar el valor mostrado en los LEDs para seleccionar el producto directamente
      if (valorPotenciometro >= 1 && valorPotenciometro <= NUM_PRODUCTOS) {
        prodIndex = valorPotenciometro - 1; // Ajustar a índice base 0
      }
      // Para el compartimento, usar el valor % 3 para obtener 0, 1 o 2
      int compartimentoSeleccionado = valorPotenciometro % 3;
      
      productos[prodIndex].compartimentos[compartimentoSeleccionado]++;
      Serial.print("Añadida una unidad de ");
      Serial.print(productos[prodIndex].nombre);
      Serial.print(" al compartimento: ");
      Serial.println(compartimentoSeleccionado + 1);
      
      // Mostrar estado de los compartimentos del producto
      Serial.print("Compartimentos de ");
      Serial.print(productos[prodIndex].nombre);
      Serial.println(": ");
      for (int i = 0; i < 3; i++) {
        Serial.print("[Comp. ");
        Serial.print(i + 1);
        Serial.print("]: ");
        Serial.println(productos[prodIndex].compartimentos[i]);
      }
      
      // Calcular stock total y mostrar estado
      int stockTotal = 0;
      for (int j = 0; j < 3; j++) {
        stockTotal += productos[prodIndex].compartimentos[j];
      }
      Serial.print("Stock total: ");
      Serial.print(stockTotal);
      Serial.print("/");
      Serial.print(productos[prodIndex].stockDeseado);
      
      // Mostrar estado de existencias
      if (stockTotal == 0) {
        Serial.println(" - ¡SIN EXISTENCIAS! (LED fijo)");
      } else if (stockTotal < productos[prodIndex].stockDeseado / 2) {
        Serial.println(" - ¡POCAS EXISTENCIAS! (LED parpadeando)");
      } else {
        Serial.println(" - Existencias suficientes");
      }
    } else {
      bloq = 0; // Resetear el bloqueo si se sueltan los botones
    }
    break;
    
  case password: // Estado para verificar la contraseña
    // Mostrar el valor actual del potenciómetro
    int valorPotenciometro = swagAnalogRead(potenciometro) * 16 / 1024;
    
    // Si se pulsa el botón A, se confirma el dígito actual
    if (estadoBotonA == TR_SOLT && estadoBotonB != PULSADO) {
      passwordInput[passwordIndex] = valorPotenciometro;
      Serial.print("Dígito ");
      Serial.print(passwordIndex + 1);
      Serial.print(" introducido: ");
      Serial.println(valorPotenciometro);
      
      passwordIndex++;
      
      // Si ya se han introducido todos los dígitos, verificar la contraseña
      if (passwordIndex >= PASSWORD_LENGTH) {
        bool passwordCorrect = true;
        for (int i = 0; i < PASSWORD_LENGTH; i++) {
          if (passwordInput[i] != PASSWORD[i]) {
            passwordCorrect = false;
            break;
          }
        }
        
        if (passwordCorrect) {
          Serial.println("Contraseña correcta. Accediendo al modo técnico...");
          estado = tecnico;
        } else {
          Serial.println("Contraseña incorrecta. Volviendo al modo normal.");
          iniciarParpadeo(ledError, &parpadeando, &contadorParpadeo, &pinParpadeo, &tiempoParpadeo);
          estado = idle;
        }
      }
    }
    
    // Si se pulsa el botón B, se cancela la entrada de contraseña
    if (estadoBotonB == TR_SOLT && estadoBotonA != PULSADO) {
      Serial.println("Entrada de contraseña cancelada.");
      estado = idle;
    }
    break;
    
  case postecnico: // Estado intermedio para evitar activar funciones al soltar los botones
    if (estadoBotonA == SUELTO && estadoBotonB == SUELTO) {
      estado = idle;
      bloq = 0; // Resetear el bloqueo
    }
    break;
  }

}
